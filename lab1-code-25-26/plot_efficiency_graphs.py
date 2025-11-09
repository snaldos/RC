import os

import matplotlib.pyplot as plt
import pandas as pd


def plot_efficiency_vs_variable(csv_path, variable_name, output_folder, output_name):
    df = pd.read_csv(csv_path)
    # Use Measured Efficiency S for y-axis
    x = df[variable_name]
    y_measured = df["Measured Efficiency S"]
    y_theoretical = df["Theoretical Efficiency S_theoretical"]
    plt.figure(figsize=(8, 5))
    plt.plot(
        x,
        y_measured,
        marker="o",
        linestyle="-",
        color="blue",
        label="Measured Efficiency",
    )
    plt.plot(
        x,
        y_theoretical,
        marker="s",
        linestyle="--",
        color="orange",
        label="Theoretical Efficiency",
    )
    plt.xlabel(variable_name)
    plt.ylabel("Efficiency S")
    plt.title(f"Efficiency vs {variable_name}")
    plt.grid(True)
    plt.legend()
    os.makedirs(output_folder, exist_ok=True)
    output_path = os.path.join(output_folder, output_name)
    plt.savefig(output_path)
    plt.close()
    print(f"Saved graph to {output_path}")


if __name__ == "__main__":
    # Plot for test_baudrate.csv
    plot_efficiency_vs_variable(
        "datasets/test_baudrate.csv", "Baudrate", "graphs", "efficiency_vs_baudrate.png"
    )
    # Plot for test_fer.csv
    plot_efficiency_vs_variable(
        "datasets/test_fer.csv", "FER", "graphs", "efficiency_vs_fer.png"
    )
    # Plot for test_max_payload_size.csv
    plot_efficiency_vs_variable(
        "datasets/test_max_payload_size.csv",
        "MaxPayloadSize",
        "graphs",
        "efficiency_vs_max_payload_size.png",
    )
    # Plot for test_prop_delay.csv
    plot_efficiency_vs_variable(
        "datasets/test_prop_delay.csv",
        "PropDelay (us)",
        "graphs",
        "efficiency_vs_prop_delay.png",
    )
