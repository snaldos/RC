import os

import matplotlib.pyplot as plt
import pandas as pd


def plot_efficiency_vs_variable(csv_path, variable_name, output_folder, output_name):
    df = pd.read_csv(csv_path, na_values=None, keep_default_na=False)
    # Use Measured Efficiency S for y-axis, but drop rows where either y value is NA or not a number
    df_plot = df.copy()
    # Convert to numeric, coerce errors to NaN
    df_plot["Measured Efficiency S"] = pd.to_numeric(
        df_plot["Measured Efficiency S"], errors="coerce"
    )
    df_plot["Theoretical Efficiency S_theoretical"] = pd.to_numeric(
        df_plot["Theoretical Efficiency S_theoretical"], errors="coerce"
    )
    df_plot[variable_name] = pd.to_numeric(df_plot[variable_name], errors="coerce")
    # Only keep rows where x and y values are not NaN
    df_plot = df_plot.dropna(
        subset=[
            variable_name,
            "Measured Efficiency S",
            "Theoretical Efficiency S_theoretical",
        ]
    )
    x = df_plot[variable_name]
    y_measured = df_plot["Measured Efficiency S"]
    y_theoretical = df_plot["Theoretical Efficiency S_theoretical"]

    # Create a figure with two columns: left for plot, right for table
    fig, (ax_table, ax_plot) = plt.subplots(
        1, 2, figsize=(16, 7), gridspec_kw={"width_ratios": [1.2, 2.8]}
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
    # Set y-axis from 0 to 1
    ax_plot.set_ylim(0, 1)
    ax_plot.set_xlabel(variable_name, fontsize=13)
    ax_plot.set_ylabel("Efficiency S", fontsize=13)
    ax_plot.set_title(f"Efficiency vs {variable_name}", fontsize=15)
    ax_plot.grid(True, which="both", linestyle="--", linewidth=0.5)
    ax_plot.legend(fontsize=11)
    ax_plot.tick_params(axis="both", which="major", labelsize=11)
    fig.subplots_adjust(wspace=0.25)
    # Ensure y-axis is not inverted (should not be needed now)

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
    table.set_fontsize(9)
    table.scale(1.1, 1.3)
    ax_table.axis("off")
    # Make sure table fits well
    table.auto_set_column_width(col=list(range(len(df_table.columns))))

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
