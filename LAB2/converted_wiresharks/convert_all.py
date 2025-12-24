import glob as sys_glob
import os
import re

from scapy.all import *

# --- Configuration ---
INPUT_FILES = sys_glob.glob("*.txt")

# --- Global Flow State ---
# Structure: { frozenset({ip1, ip2}): { 'control': {ip1: port, ip2: port}, 'data': {ip1: port, ip2: port} } }
flow_state = {}

# Tracks TCP Sequence numbers
# Key: (src_ip, src_port, dst_ip, dst_port) -> Value: int (Next Sequence Num)
seq_tracker = {}

def clean_mac(mac_str):
    if not mac_str:
        return None
    if "Broadcast" in mac_str:
        return "ff:ff:ff:ff:ff:ff"
    if "Spanning-tree" in mac_str:
        return "01:80:c2:00:00:00"
    if "IPv6mcast" in mac_str:
        return "33:33:00:00:00:fb"
    if "IPv4mcast" in mac_str:
        return "01:00:5e:00:00:fb"
    if "LLDP_Multicast" in mac_str:
        return "01:80:c2:00:00:0e"

    if "Routerbo_" in mac_str:
        suffix = mac_str.split("_")[1] if "_" in mac_str else "00:00:00"
        return f"00:0c:42:{suffix}"
    if "Cisco_" in mac_str:
        suffix = mac_str.split("_")[1] if "_" in mac_str else "00:00:00"
        return f"00:00:0c:{suffix}"
    if "Dell_" in mac_str:
        suffix = mac_str.split("_")[1] if "_" in mac_str else "00:00:00"
        return f"f8:bc:12:{suffix}"
    if "ProxmoxS_" in mac_str:
        suffix = mac_str.split("_")[1] if "_" in mac_str else "00:00:00"
        return f"bc:24:11:{suffix}"

    if re.match(r"([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}", mac_str):
        return mac_str
    return None


def parse_ports(info_str):
    port_match = re.search(r"(\d+)\s*[\>|→]\s*(\d+)", info_str)
    if port_match:
        return int(port_match.group(1)), int(port_match.group(2))
    return None

def get_seq_ack(flow_key, rev_key, text_seq, text_ack, payload_len, flags):
    """
    Calculates Seq/Ack.
    Crucially: Adds +1 for SYN or FIN flags which consume a sequence number.
    """
    virtual_len = payload_len
    if "S" in flags or "F" in flags:
        virtual_len += 1

    # Sequence
    if text_seq is not None:
        final_seq = text_seq
        seq_tracker[flow_key] = final_seq + virtual_len
    else:
        final_seq = seq_tracker.get(flow_key, 0)
        seq_tracker[flow_key] = final_seq + virtual_len

    # Ack
    if text_ack is not None:
        final_ack = text_ack
    else:
        final_ack = seq_tracker.get(rev_key, 0)

    return final_seq, final_ack

def convert_file(input_filename):
    output_filename = os.path.splitext(input_filename)[0] + ".pcap"
    print(f"Processing: {input_filename} -> {output_filename}")

    packets = []
    summary_pattern = re.compile(
        r"^\s*(\d+)\s+(\d+\.\d+)\s+(\S+)\s+(\S+)\s+([A-Za-z0-9\-\.]+)\s+(\d+)\s+(.*)"
    )

    flow_state.clear()
    seq_tracker.clear()

    try:
        with open(input_filename, "r", encoding="utf-8", errors="ignore") as f:
            lines = f.readlines()
    except Exception as e:
        print(f"  Error reading file: {e}")
        return

    for line in lines:
        match = summary_pattern.match(line)
        if match:
            pkt_num, timestamp, src_raw, dst_raw, proto, length, info = match.groups()
            timestamp = float(timestamp)

            src_mac = clean_mac(src_raw) or "00:00:00:00:00:01"
            dst_mac = clean_mac(dst_raw) or "00:00:00:00:00:02"

            src_ip, dst_ip = "0.0.0.0", "0.0.0.0"
            is_ipv6 = False
            ipv4_pattern = r"\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}"

            if re.match(ipv4_pattern, src_raw):
                src_ip = src_raw
            if re.match(ipv4_pattern, dst_raw):
                dst_ip = dst_raw
            if ":" in src_raw and "Routerbo" not in src_raw:
                is_ipv6, src_ip = True, src_raw
            if ":" in dst_raw and "Routerbo" not in dst_raw:
                is_ipv6, dst_ip = True, dst_raw

            packet = None
            ip_key = frozenset({src_ip, dst_ip})

            if ip_key not in flow_state:
                flow_state[ip_key] = {"control": {}, "data": {}}

            # --- Port Logic ---
            parsed_ports = parse_ports(info)
            sport, dport = 0, 0

            # 1. Store ports if explicitly stated in text (e.g. handshakes)
            if parsed_ports:
                sport, dport = parsed_ports
                if sport == 21 or dport == 21:
                    flow_state[ip_key]["control"] = {src_ip: sport, dst_ip: dport}
                else:
                    flow_state[ip_key]["data"] = {src_ip: sport, dst_ip: dport}

            # 2. Retrieve ports for protocol lines (FTP, HTTP, etc)
            else:
                port_map = {}

                # --- HARDCODED QUIT FIX ---
                # "QUIT" is an FTP command, so it MUST go to the Control Channel
                is_quit = "QUIT" in info

                if proto == "FTP" or is_quit:
                    port_map = flow_state[ip_key].get("control", {})
                elif proto == "FTP-DATA":
                    port_map = flow_state[ip_key].get("data", {})
                elif proto == "TCP":
                    port_map = flow_state[ip_key].get("control") or flow_state[
                        ip_key
                    ].get("data", {})

                sport = port_map.get(src_ip, 0)
                dport = port_map.get(dst_ip, 0)

                # Fallback defaults if tracking failed
                if sport == 0 or dport == 0:
                    if proto == "FTP" or is_quit:
                        sport, dport = (
                            (21, 49152) if src_ip.endswith(".10") else (49152, 21)
                        )
                    elif proto == "FTP-DATA":
                        sport, dport = (
                            (20, 49152) if src_ip.endswith(".10") else (49152, 20)
                        )
                    else:
                        sport, dport = 12345, 80

            # --- Seq/Ack Parsing ---
            text_seq, text_ack, win = None, None, 8192
            m_seq = re.search(r"Seq=(\d+)", info)
            m_ack = re.search(r"Ack=(\d+)", info)
            m_win = re.search(r"Win=(\d+)", info)
            if m_seq:
                text_seq = int(m_seq.group(1))
            if m_ack:
                text_ack = int(m_ack.group(1))
            if m_win:
                win = int(m_win.group(1))
                if win > 65535:
                    win = 65535

            # --- Protocol Construction ---

            if proto == "STP":
                packet = Dot3(src=src_mac, dst=dst_mac) / LLC() / STP()

            elif proto == "ARP":
                op = 2 if "is at" in info else 1
                ips = re.findall(ipv4_pattern, info)
                psrc, pdst = ("0.0.0.0", "0.0.0.0")
                if op == 1 and len(ips) >= 2:
                    pdst, psrc = ips[0], ips[1]
                elif len(ips) >= 1:
                    psrc = ips[0]
                packet = Ether(src=src_mac, dst=dst_mac) / ARP(
                    op=op, psrc=psrc, pdst=pdst, hwsrc=src_mac, hwdst=dst_mac
                )

            elif "ICMP" in proto:
                if "v6" in proto or is_ipv6:
                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IPv6(src=src_ip, dst=dst_ip)
                        / ICMPv6Unknown()
                    )
                else:
                    itype = 8
                    if "echo (ping) reply" in info.lower():
                        itype = 0
                    elif "unreachable" in info.lower():
                        itype = 3
                    elif "redirect" in info.lower():
                        itype = 5

                    id_val, seq_val = 0, 0
                    id_m = re.search(r"id=0x([0-9a-fA-F]+)", info)
                    seq_m = re.search(r"seq=(\d+)", info)
                    if id_m:
                        id_val = int(id_m.group(1), 16)
                    if seq_m:
                        seq_val = int(seq_m.group(1))
                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IP(src=src_ip, dst=dst_ip)
                        / ICMP(type=itype, id=id_val, seq=seq_val)
                    )

            elif "DNS" in proto or "MDNS" in proto:
                if dport == 0:
                    dport = 53
                if "MDNS" in proto:
                    sport, dport = 5353, 5353

                dns_id = 0
                id_m = re.search(r"0x([0-9a-fA-F]+)", info)
                if id_m:
                    dns_id = int(id_m.group(1), 16)

                qname, qtype_int = ".", 1
                q_m = re.search(
                    r"\b(A|AAAA|PTR|TXT|SRV|SOA|NS|CNAME|MX)\s+([a-zA-Z0-9\.\-\_\:]+)",
                    info,
                )
                if q_m:
                    qname = q_m.group(2)

                dns_layer = DNS(id=dns_id, rd=1, qd=DNSQR(qname=qname, qtype=1))
                if "response" in info.lower():
                    dns_layer.qr = 1
                    if "No such name" in info:
                        dns_layer.rcode = 3
                    soa_m = re.search(r"SOA\s+([a-zA-Z0-9\.\-\_]+)", info)
                    if soa_m:
                        dns_layer.ns = DNSRRSOA(
                            rrname=qname,
                            type=6,
                            ttl=60,
                            mname=soa_m.group(1),
                            rname="dns.cloudflare.com",
                            serial=2023,
                        )

                if is_ipv6:
                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IPv6(src=src_ip, dst=dst_ip)
                        / UDP(sport=sport, dport=dport)
                        / dns_layer
                    )
                else:
                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IP(src=src_ip, dst=dst_ip)
                        / UDP(sport=sport, dport=dport)
                        / dns_layer
                    )

            elif (
                proto == "FTP"
                or "FTP-DATA" in proto
                or proto == "TCP"
                or "TLS" in proto
                or "HTTP" in proto
            ):
                flags = "S"
                if "ACK" in info:
                    flags = "A"
                if "SYN" in info:
                    flags = "S"  # Reset
                if "SYN" in info and "ACK" in info:
                    flags = "SA"
                if "FIN" in info:
                    flags = "F"
                if "RST" in info:
                    flags = "R"
                if "PUSH" in info or "PSH" in info:
                    flags += "P"

                payload = b""
                is_quit = "QUIT" in info

                if proto == "FTP" or is_quit:
                    # Explicitly handle QUIT here to ensure it gets PA flags
                    flags = "PA"
                    if "Request:" in info:
                        payload = info.split("Request:")[1].strip().encode() + b"\r\n"
                    elif "Response:" in info:
                        payload = info.split("Response:")[1].strip().encode() + b"\r\n"
                elif proto == "FTP-DATA":
                    flags = "PA"
                    len_m = re.search(r"(\d+) bytes", info)
                    payload = b"X" * (int(len_m.group(1)) if len_m else 10)

                # --- TCP Stream Continuity ---
                flow_key = (src_ip, sport, dst_ip, dport)
                rev_key = (dst_ip, dport, src_ip, sport)
                curr_seq, curr_ack = get_seq_ack(
                    flow_key, rev_key, text_seq, text_ack, len(payload), flags
                )

                if is_ipv6:
                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IPv6(src=src_ip, dst=dst_ip)
                        / TCP(
                            sport=sport,
                            dport=dport,
                            flags=flags,
                            seq=curr_seq,
                            ack=curr_ack,
                            window=win,
                        )
                        / Raw(load=payload)
                    )
                else:
                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IP(src=src_ip, dst=dst_ip)
                        / TCP(
                            sport=sport,
                            dport=dport,
                            flags=flags,
                            seq=curr_seq,
                            ack=curr_ack,
                            window=win,
                        )
                        / Raw(load=payload)
                    )

            elif "UDP" in proto or "MNDP" in proto:
                if is_ipv6:
                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IPv6(src=src_ip, dst=dst_ip)
                        / UDP(sport=sport, dport=dport)
                    )
                else:
                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IP(src=src_ip, dst=dst_ip)
                        / UDP(sport=sport, dport=dport)
                    )

            if packet:
                packet.time = timestamp
                packets.append(packet)

    if packets:
        wrpcap(output_filename, packets)
        print(f"  -> Created {output_filename} ({len(packets)} packets)")
    else:
        print(f"  -> Skipped (No valid packet summaries found)")


def main():
    if not INPUT_FILES:
        return
    for f in INPUT_FILES:
        convert_file(f)
    print("\nAll done!")


if __name__ == "__main__":
    main()
