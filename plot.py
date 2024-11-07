import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
import glob
import os

csv_files = glob.glob('res/*.csv')
all_data = []

# Loop through each CSV file and gather the data
for csv_file in csv_files:
    base_name = os.path.basename(csv_file).replace('_timing.csv', '')
    df = pd.read_csv(csv_file)

    def time_to_milliseconds(time_str):
        if 'm' in time_str and 's' in time_str:
            minutes, seconds = time_str.split('m')
            total_seconds = float(minutes) * 60 + float(seconds.rstrip('s'))
            return total_seconds * 1000  # Convert to milliseconds
        elif 's' in time_str:
            # Convert to milliseconds
            return float(time_str.rstrip('s')) * 1000
        return 0

    df['RealTime'] = df['RealTime'].apply(time_to_milliseconds)

    all_data.append(
        (base_name, df['Optimization'].values, df['RealTime'].values))

plt.figure(figsize=(10, 6))

n_benchmarks = len(all_data)
n_modes = 2  # Assuming there are always 4 modes in each CSV
bar_width = 0.2
x_indices = np.arange(n_benchmarks)

colors = ['blue', 'green', 'red', 'purple']

for i in range(n_modes):
    mode_times = [data[2][i] for data in all_data]
    mode_label = all_data[0][1][i]
    plt.bar(x_indices + i * bar_width, mode_times,
            width=bar_width, label=mode_label, color=colors[i])

# Add labels and title for absolute times
plt.xlabel('Benchmark')
plt.ylabel('Real Time (milliseconds)')
plt.title('Native Compiler vs LLVM Performance')
plt.xticks(x_indices + bar_width * (n_modes - 1) / 2,
           [data[0] for data in all_data])  # Set benchmark names
plt.legend(title='Optimization Mode', loc='best')
plt.grid(True)

plt.savefig('real_time_benchmark_modes.png')
plt.show()

plt.figure(figsize=(10, 6))

for i in range(1, n_modes):  # Start from 1 to skip "no-optimizations"
    normalized_times = [
        data[2][i] / data[2][0] for data in all_data
    ]  # Normalize by "no-optimizations" (data[2][0])
    mode_label = all_data[0][1][i]
    plt.bar(x_indices + (i - 1) * bar_width, normalized_times,
            width=bar_width, label=mode_label, color=colors[i])

plt.axhline(y=1.0, color='black', linestyle='--',
            linewidth=1.5, label='No Optimizations Baseline')

plt.xlabel('Benchmark')
plt.ylabel('Normalized Real Time (w.r.t No Optimizations)')
plt.title(
    'Normalized Real Time for Different Optimization Levels (w.r.t No Optimizations)')
plt.xticks(x_indices + bar_width * (n_modes - 2) / 2,
           [data[0] for data in all_data])  # Set benchmark names
plt.legend(title='Optimization Mode', loc='best')
plt.grid(True)

plt.savefig('normalized_real_time_benchmark_modes.png')
plt.show()
