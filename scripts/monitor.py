import time

def read_sriov_stats(file_path):
    """读取SR-IOV stats文件中的tx_bytes和tx_packets值"""
    stats = {'tx_bytes': None, 'tx_packets': None}
    
    with open(file_path, 'r') as f:
        for line in f:
            if 'tx_bytes' in line:
                stats['tx_bytes'] = int(line.split(':')[1].strip())
            elif 'tx_packets' in line:
                stats['tx_packets'] = int(line.split(':')[1].strip())
    
    return stats['tx_bytes'], stats['tx_packets']

def main():
    stats_file = "/sys/class/net/ens1f0/device/sriov/1/stats"
    last_bytes, last_packets = None, None
    
    try:
        while True:
            # 读取当前统计值
            current_bytes, current_packets = read_sriov_stats(stats_file)
            
            # 计算实时速率（第一次不计算）
            if last_bytes is not None and last_packets is not None:
                time_diff = 0.1  # 100ms转换为秒
                
                # 计算带宽（bytes转Mbps）
                byte_diff = current_bytes - last_bytes
                mbps = (byte_diff * 8) / (time_diff * 1e6)  # 1 Mbps = 1e6 bits
                
                # 计算包速率（packets/s转千包每秒）
                packet_diff = current_packets - last_packets
                kpps = packet_diff / (time_diff * 1e3)  # 1 kpps = 1000 packets/s
                
                # 实时输出结果（同一行刷新）
                print(f"\rBandwidth: {mbps:.2f} Mbps | Packet Rate: {kpps:.2f} kpps", end='', flush=True)
            
            # 更新上一次的值
            last_bytes, last_packets = current_bytes, current_packets
            
            # 等待100ms
            time.sleep(0.1)
    
    except KeyboardInterrupt:
        print("\nMonitoring stopped by user.")

if __name__ == "__main__":
    main()