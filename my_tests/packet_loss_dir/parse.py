import re
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import pandas as pd

def extract_numbers_from_log(log_file_path):
    # Define the regular expression patterns
    pattern1 = r'Packets received: (\d+)'
    pattern2 = r'Packets dropped at NIC: (\d+)'
    pattern3 = r'rx_ring_size=(\d+)'
    pattern4 = r'processing_time=(\d+)'
    pattern5 = r'rx_rate=(\d+)'

    # Open and read the log file
    with open(log_file_path, 'r') as file:
        log_contents = file.read()

    # Find all matches using the regular expressions
    numbers1 = [int(x) for x in re.findall(pattern1, log_contents)]
    numbers2 = [int(x) for x in re.findall(pattern2, log_contents)]
    numbers3 = re.findall(pattern3, log_contents)
    numbers3 = [int(numbers3[i]) for i in range(0, len(numbers3), 2)]
    numbers4 = re.findall(pattern3, log_contents)
    numbers4 = [int(numbers4[i]) for i in range(0, len(numbers4), 2)]
    numbers5 = re.findall(pattern3, log_contents)
    numbers5 = [int(numbers5[i]) for i in range(0, len(numbers5), 2)]
    return numbers1, numbers2, numbers3, numbers4, numbers5

log_file_path = './copyless-results'  # Replace with the path to your log file
numbers1, numbers2, numbers3, numbers4, numbers5 = extract_numbers_from_log(log_file_path)

# Check if any of the lists are empty before proceeding
if not all(len(lst) > 0 for lst in [numbers1, numbers2, numbers3, numbers4, numbers5]):
    print("One or more lists are empty.")
else:
    df = pd.DataFrame({
        "received": numbers1,
        "dropped": numbers2,
        "ring size": numbers3,
        "processing time": numbers4,
        "rx rate": numbers5
    })

    df["total"] = df["dropped"] + df["received"]
    
    # Avoid division by zero
    df["packet loss"] = df["dropped"] / df["total"].replace(0, pd.NA)

    print(df)

    # Normalize packet loss for alpha transparency
    alpha_values = df["packet loss"].fillna(0).clip(0, 1).values

    plt.scatter(x=df["rx rate"], y=df["ring size"], alpha=alpha_values)
    plt.savefig('plot.png')
