import glob as sys_glob  # <--- FIXED: Renamed to avoid conflict with Scapy
import os
import re

from scapy.all import *

# --- Configuration ---
# Use the renamed 'sys_glob' to find files
INPUT_FILES = sys_glob.glob("*.txt")

def clean_mac(mac_str):
    """Resolves names to MACs or returns a dummy MAC."""
    if not mac_str: return None
    if "Broadcast" in mac_str:
        return "ff:ff:ff:ff:ff:ff"
    if "Spanning-tree" in mac_str:
        return "01:80:c2:00:00:00"
    if "Routerbo_" in mac_str: # Mikrotik
        suffix = mac_str.split("_")[1] if "_" in mac_str else "00:00:00"
        return f"00:0c:42:{suffix}"
    if "Cisco_" in mac_str:
        suffix = mac_str.split("_")[1] if "_" in mac_str else "00:00:00"
        return f"00:00:0c:{suffix}"
    if "Dell_" in mac_str:
        suffix = mac_str.split("_")[1] if "_" in mac_str else "00:00:00"
        return f"f8:bc:12:{suffix}"

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

    # Regex to capture the summary line.
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

            is_ip_traffic = False
            src_ip = "0.0.0.0"
            dst_ip = "0.0.0.0"

            ip_pattern = r"\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}"
            if re.match(ip_pattern, src_raw) and re.match(ip_pattern, dst_raw):
                is_ip_traffic = True
                src_ip = src_raw
                dst_ip = dst_raw

            packet = None

            # --- Logic Builder ---

            if proto == "STP":
                packet = Dot3(src=src_mac, dst=dst_mac) / LLC() / STP()

            elif proto == "ARP":
                op = 2 if "is at" in info else 1
                ips = re.findall(ip_pattern, info)
                psrc, pdst = ("0.0.0.0", "0.0.0.0")
                if len(ips) >= 2:
                    if op == 1:
                        pdst = ips[0]
                        psrc = ips[1]
                    else:
                        psrc = ips[0]
                packet = Ether(src=src_mac, dst=dst_mac) / ARP(
                    op=op, psrc=psrc, pdst=pdst
                )

            elif proto == "ICMP":
                # --- FIXED ICMP LOGIC ---
                # Default to Echo Request (8)
                itype = 8

                # Only switch to Reply (0) if we explicitly see "ping) reply"
                # This prevents "(reply in 22)" text in Requests from triggering this.
                if "ping) reply" in info.lower() or "echo (ping) reply" in info.lower():
                    itype = 0
                elif "redirect" in info.lower():
                    itype = 5
                elif "unreachable" in info.lower():
                    itype = 3

                # Parse ID and Sequence Number so Wireshark groups them correctly
                id_val = 0
                seq_val = 0
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

                packet = Ether(src=src_mac, dst=dst_mac) / IP(src=src_ip, dst=dst_ip) / TCP(sport=sport, dport=dport, flags=flags)

            elif proto == "UDP" or "DNS" in proto or "MDNS" in proto:
                sport, dport = parse_ports(info)
                if "DNS" in proto:
                    dport = 53 if src_ip != "0.0.0.0" else 5353
                    sport = 12345

                packet = (
                    Ether(src=src_mac, dst=dst_mac)
                    / IP(src=src_ip, dst=dst_ip)
                    / UDP(sport=sport, dport=dport)
                )
                if "DNS" in proto:
                    packet = packet / DNS()

            else:
                if is_ip_traffic:
                    packet = Ether(src=src_mac, dst=dst_mac) / IP(
                        src=src_ip, dst=dst_ip
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
