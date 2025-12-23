import re
import os
# Import Scapy first to allow 'glob' to be imported correctly afterwards
from scapy.all import *
import glob 

# --- Configuration ---
# Finds all .txt files in the current directory
INPUT_FILES = glob.glob("*.txt")

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
    
    # Valid MAC check
    if re.match(r"([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}", mac_str):
        return mac_str
    
    # Fallback for when Source/Dest are IP addresses (we use a dummy MAC)
    return None

def parse_ports(info_str):
    """extracts src_port and dst_port from Info string like '57514 > 21' or '57514 → 21'"""
    # Regex for "1234 > 80" or "1234 → 80"
    port_match = re.search(r"(\d+)\s*[\>|→]\s*(\d+)", info_str)
    if port_match:
        return int(port_match.group(1)), int(port_match.group(2))
    return 1024, 80 # Default fallback

def convert_file(input_filename):
    output_filename = os.path.splitext(input_filename)[0] + ".pcap"
    print(f"Processing: {input_filename} -> {output_filename}")
    
    packets = []
    
    # Regex to capture the summary line. 
    # Handles variable spaces and columns.
    # Ex: 1 0.000 1.2.3.4 5.6.7.8 TCP 60 Info...
    summary_pattern = re.compile(r"^\s*(\d+)\s+(\d+\.\d+)\s+(\S+)\s+(\S+)\s+([A-Za-z0-9\-\.]+)\s+(\d+)\s+(.*)")

    try:
        with open(input_filename, 'r', encoding='utf-8', errors='ignore') as f:
            lines = f.readlines()
    except Exception as e:
        print(f"  Error reading file: {e}")
        return

    for line in lines:
        match = summary_pattern.match(line)
        if match:
            pkt_num, timestamp, src_raw, dst_raw, proto, length, info = match.groups()
            timestamp = float(timestamp)
            
            # Attempt to resolve MACs. If None, it means the column likely holds an IP.
            src_mac = clean_mac(src_raw) or "00:00:00:00:00:01"
            dst_mac = clean_mac(dst_raw) or "00:00:00:00:00:02"
            
            # Check if Source/Dest are IPs
            is_ip_traffic = False
            src_ip = "0.0.0.0"
            dst_ip = "0.0.0.0"
            
            # Simple regex to check for IP format
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
                # Try to extract IPs from Info
                ips = re.findall(ip_pattern, info)
                psrc, pdst = ("0.0.0.0", "0.0.0.0")
                if len(ips) >= 2:
                    # Logic: "Who has 172.16.20.254? Tell 172.16.20.1" -> 172.16.20.1 is asking (src)
                    if op == 1: 
                        pdst = ips[0] # Who has X
                        psrc = ips[1] # Tell Y
                    else: 
                        psrc = ips[0] # "X is at..."
                
                packet = Ether(src=src_mac, dst=dst_mac) / ARP(op=op, psrc=psrc, pdst=pdst)

            elif proto == "ICMP":
                # Check for Echo Request/Reply
                itype = 8 # Request
                if "reply" in info.lower(): itype = 0
                packet = Ether(src=src_mac, dst=dst_mac) / IP(src=src_ip, dst=dst_ip) / ICMP(type=itype)

            elif proto == "TCP" or "TLS" in proto or "HTTP" in proto or "FTP" in proto:
                # TCP Handling
                sport, dport = parse_ports(info)
                flags = "S" # Default Syn
                if "ACK" in info: flags = "A"
                if "SYN" in info and "ACK" in info: flags = "SA"
                if "FIN" in info: flags = "F"
                if "RST" in info: flags = "R"
                
                # Create IP/TCP packet
                packet = Ether(src=src_mac, dst=dst_mac) / IP(src=src_ip, dst=dst_ip) / TCP(sport=sport, dport=dport, flags=flags)

            elif proto == "UDP" or "DNS" in proto or "MDNS" in proto:
                # UDP Handling
                sport, dport = parse_ports(info)
                # DNS specific
                if "DNS" in proto: 
                    dport = 53 if src_ip != "0.0.0.0" else 5353 # Standard or MDNS fallback
                    sport = 12345
                
                packet = Ether(src=src_mac, dst=dst_mac) / IP(src=src_ip, dst=dst_ip) / UDP(sport=sport, dport=dport)
                if "DNS" in proto:
                    packet = packet / DNS() # Add empty DNS layer just to mark it

            else:
                # Fallback for unknown IPv4 protocols
                if is_ip_traffic:
                    packet = Ether(src=src_mac, dst=dst_mac) / IP(src=src_ip, dst=dst_ip)
            
            # --- Save ---
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
