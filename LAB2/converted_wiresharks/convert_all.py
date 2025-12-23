import glob as sys_glob
import os
import re

from scapy.all import *

# --- Configuration ---
INPUT_FILES = sys_glob.glob("*.txt")


def clean_mac(mac_str):
    """Resolves names to MACs or returns a dummy MAC."""
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

    # Vendor prefixes
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
    return 1024, 80


def convert_file(input_filename):
    output_filename = os.path.splitext(input_filename)[0] + ".pcap"
    print(f"Processing: {input_filename} -> {output_filename}")

    packets = []
    # Regex to capture the summary line
    summary_pattern = re.compile(
        r"^\s*(\d+)\s+(\d+\.\d+)\s+(\S+)\s+(\S+)\s+([A-Za-z0-9\-\.]+)\s+(\d+)\s+(.*)"
    )

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

            # IP detection
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

            # --- Logic ---

            if proto == "STP":
                packet = Dot3(src=src_mac, dst=dst_mac) / LLC() / STP()

            elif proto == "ARP":
                op = 2 if "is at" in info else 1
                ips = re.findall(ipv4_pattern, info)
                psrc, pdst = ("0.0.0.0", "0.0.0.0")

                # Correct ARP logic: Reply has only 1 IP in text
                if op == 1:  # Request
                    if len(ips) >= 2:
                        pdst, psrc = ips[0], ips[1]
                else:  # Reply
                    if len(ips) >= 1:
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
                    # --- FIXED ICMP LOGIC ---
                    itype = 8  # Default to Request

                    # Strict check: Only mark as reply if it explicitly says "Echo (ping) reply"
                    # This avoids matching "(reply in 22)" which appears in requests.
                    if "echo (ping) reply" in info.lower():
                        itype = 0
                    elif "unreachable" in info.lower():
                        itype = 3
                    elif "redirect" in info.lower():
                        itype = 5

                    id_val, seq_val = 0, 0
                    id_match = re.search(r"id=0x([0-9a-fA-F]+)", info)
                    seq_match = re.search(r"seq=(\d+)", info)
                    if id_match:
                        id_val = int(id_match.group(1), 16)
                    if seq_match:
                        seq_val = int(seq_match.group(1))

                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IP(src=src_ip, dst=dst_ip)
                        / ICMP(type=itype, id=id_val, seq=seq_val)
                    )

            elif "DNS" in proto or "MDNS" in proto:
                sport, dport = parse_ports(info)
                if "DNS" in proto and dport == 80:
                    dport = 53
                if "MDNS" in proto:
                    sport, dport = 5353, 5353

                dns_id = 0
                id_match = re.search(r"0x([0-9a-fA-F]+)", info)
                if id_match:
                    dns_id = int(id_match.group(1), 16)

                qname = "."
                qtype_map = {
                    "A": 1,
                    "NS": 2,
                    "CNAME": 5,
                    "SOA": 6,
                    "PTR": 12,
                    "MX": 15,
                    "TXT": 16,
                    "AAAA": 28,
                    "SRV": 33,
                }
                q_type_int = 1

                q_match = re.search(
                    r"\b(A|AAAA|PTR|TXT|SRV|SOA|NS|CNAME|MX)\s+([a-zA-Z0-9\.\-\_\:]+)",
                    info,
                )
                if q_match:
                    q_type_int = qtype_map.get(q_match.group(1), 1)
                    qname = q_match.group(2)

                dns_layer = DNS(
                    id=dns_id, rd=1, qd=DNSQR(qname=qname, qtype=q_type_int)
                )

                if "response" in info.lower():
                    dns_layer.qr = 1
                    if "No such name" in info:
                        dns_layer.rcode = 3

                    soa_match = re.search(r"SOA\s+([a-zA-Z0-9\.\-\_]+)", info)
                    if soa_match:
                        mname = soa_match.group(1)
                        # Use DNSRRSOA class to fix 'AttributeError: mname'
                        soa_rr = DNSRRSOA(
                            rrname=qname,
                            type=6,
                            ttl=60,
                            mname=mname,
                            rname="dns.cloudflare.com",
                            serial=20230101,
                            refresh=10000,
                            retry=2400,
                            expire=604800,
                            minimum=3600,
                        )
                        dns_layer.ns = soa_rr

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

            elif proto == "TCP" or "TLS" in proto or "HTTP" in proto or "FTP" in proto:
                sport, dport = parse_ports(info)
                flags = "S"
                if "ACK" in info:
                    flags = "A"
                if "SYN" in info and "ACK" in info:
                    flags = "SA"
                if "FIN" in info:
                    flags = "F"
                if "RST" in info:
                    flags = "R"
                if "PUSH" in info or "PSH" in info:
                    flags += "P"

                if is_ipv6:
                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IPv6(src=src_ip, dst=dst_ip)
                        / TCP(sport=sport, dport=dport, flags=flags)
                    )
                else:
                    packet = (
                        Ether(src=src_mac, dst=dst_mac)
                        / IP(src=src_ip, dst=dst_ip)
                        / TCP(sport=sport, dport=dport, flags=flags)
                    )

            elif proto == "UDP" or "MNDP" in proto:
                sport, dport = parse_ports(info)
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
        print("No .txt files found in this directory!")
        return

    print(f"Found {len(INPUT_FILES)} files. Starting conversion...")
    for f in INPUT_FILES:
        convert_file(f)
    print("\nAll done!")


if __name__ == "__main__":
    main()
