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
    stats_file = [0]*4
    stats_file[0] = "/sys/class/net/ens1f0/device/sriov/0/stats"
    stats_file[1] = "/sys/class/net/ens1f0/device/sriov/1/stats"
    stats_file[2] = "/sys/class/net/ens1f0/device/sriov/2/stats"
    stats_file[3] = "/sys/class/net/ens1f0/device/sriov/3/stats"
    last_bytes, last_packets = [-1]*4, [-1]*4
    
    try:
        while True:
            for i in range(0,4):
                # 读取当前统计值
                current_bytes, current_packets = read_sriov_stats(stats_file[i])
                
                # 计算实时速率（第一次不计算）
                if last_bytes[i] != -1 and last_packets[i] != -1:
                    time_diff = 1  # 100ms转换为秒
                    
                    # 计算带宽（bytes转Mbps）
                    byte_diff = current_bytes - last_bytes[i]
                    mbps = (byte_diff * 8) / (time_diff * 1e6)  # 1 Mbps = 1e6 bits
                    
                    # 计算包速率（packets/s转千包每秒）
                    packet_diff = current_packets - last_packets[i]
                    kpps = packet_diff / (time_diff * 1e3)  # 1 kpps = 1000 packets/s
                    
                    # 实时输出结果（同一行刷新）
                    # print(f"\rBandwidth: {mbps:.2f} Mbps | Packet Rate: {kpps:.2f} kpps", end='', flush=True)
                    print(f"{i}, {mbps:.2f}, {kpps:.2f}")
            
                # 更新上一次的值
                last_bytes[i], last_packets[i] = current_bytes, current_packets
            
            # 等待100ms
            time.sleep(1)
    
    except KeyboardInterrupt:
        print("\nMonitoring stopped by user.")

if __name__ == "__main__":
    main()