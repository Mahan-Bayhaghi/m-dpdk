import re
import pandas as pd
import matplotlib.pyplot as plt

def extract_data_from_log(log_file_path):
    # Define the regular expression patterns
    pattern = r'Testing rx_rate=(\d+), rx_ring_size=(\d+), processing_time=(\d+).*?Packets received: (\d+), Packets dropped at NIC: (\d+).*?Statistics for network device:.*?Successful packets:.*?(\d+).*?Failed packets:.*?(\d+).*?(?=Test complete)'

    data = []
    
    with open(log_file_path, 'r') as file:
        log_contents = file.read()
        
        matches = re.findall(pattern, log_contents, re.DOTALL)
        
        for match in matches:
            rx_rate, rx_ring_size, processing_time, packets_received, packets_dropped, successful_packets, failed_packets = match
            
            data.append({
                "Packets received": int(packets_received),
                "Packets dropped": int(packets_dropped),
                "rx_rate": int(rx_rate),
                "rx_ring_size": int(rx_ring_size),
                "processing_time": int(processing_time)
            })

    return pd.DataFrame(data)

log_file_path = 'copyless-results'  # Replace with your actual log file path
df = extract_data_from_log(log_file_path)

# Calculate packet loss
df['packet_loss'] = df['Packets dropped'] / (df['Packets received'] + df['Packets dropped'])

# Plotting
plt.figure(figsize=(10, 6))
scatter = plt.scatter(df['rx_rate'], df['rx_ring_size'], c=1-df['packet_loss'], alpha=0.5, cmap='hot')
plt.colorbar(scatter, label='Packet Loss Rate')
plt.title('Packet Loss Based on RX Rate and RX Ring Size')
plt.xlabel('RX Rate (bps)')
plt.ylabel('RX Ring Size (bytes)')
plt.grid()
plt.savefig('packet_loss_plot.png')
plt.show()
