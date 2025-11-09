import os

import matplotlib.pyplot as plt
import pandas as pd


def plot_efficiency_vs_variable(csv_path, variable_name, output_folder, output_name):
    df = pd.read_csv(csv_path, na_values=None, keep_default_na=False)
    # Use Measured Efficiency S for y-axis
    x = df[variable_name]
    y_measured = df["Measured Efficiency S"]
    y_theoretical = df["Theoretical Efficiency S_theoretical"]

    # Create a figure with two columns: left for plot, right for table
    fig, (ax_table, ax_plot) = plt.subplots(
        1, 2, figsize=(14, 6), gridspec_kw={"width_ratios": [1, 2]}
    )

    # Plot on the left
    ax_plot.plot(
        x,
        y_measured,
        marker="o",
        linestyle="-",
        color="blue",
        label="Measured Efficiency",
    )
    ax_plot.plot(
        x,
        y_theoretical,
        marker="s",
        linestyle="--",
        color="orange",
        label="Theoretical Efficiency",
    )
    ax_plot.set_xlabel(variable_name)
    ax_plot.set_ylabel("Efficiency S")
    ax_plot.set_title(f"Efficiency vs {variable_name}")
    ax_plot.grid(True)
    ax_plot.legend()

    # Table on the right
    # Rename columns and remove 'Test Number' as requested
    col_rename = {
        "Baudrate": "C (bps)",
        "Elapsed Time (seconds)": "T_total (s)",
        "Throughput R (bps)": "R (bps)",
        "Measured Efficiency S": "S_measured",
        "Theoretical Efficiency S_theoretical": "S_theoretical",
        "FER": "FER",
        "PropDelay (us)": "T_prop (us)",
        "MaxPayloadSize": "L (bytes)",
    }
    # Remove 'Test Number' if present
    df_table = df.copy()
    if "Test Number" in df_table.columns:
        df_table = df_table.drop(columns=["Test Number"])
    # Rename columns if present
    df_table = df_table.rename(columns=col_rename)
    # Format FER column to 4 decimal places if present, but keep 'N/A' as is
    if "FER" in df_table.columns:

        def fer_format(v):
            if v == "N/A":
                return "N/A"
            try:
                return f"{float(v):.4f}"
            except Exception:
                return v

        df_table["FER"] = df_table["FER"].apply(fer_format)
    # Replace empty strings and NaN with 'N/A' for display
    df_table = df_table.replace({"": "N/A"})
    df_table = df_table.fillna("N/A")
    # Show all rows and renamed columns
    table = ax_table.table(
        cellText=df_table.values,
        colLabels=df_table.columns,
        loc="center",
        cellLoc="center",
    )
    table.auto_set_font_size(False)
    table.set_fontsize(8)
    table.scale(1, 1.5)
    ax_table.axis("off")

    plt.tight_layout()
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
