import subprocess
import time


def open_terminal_with_command(command, password=None):
    """
    Opens a new terminal window and runs the specified command.
    If a password is provided, it is used for sudo commands.
    """
    if password:
        command = f"echo {password} | sudo -S {command}"
    return subprocess.Popen(["kitty", "bash", "-c", f"{command}; exec bash"])


def main():
    # Sudo password (replace with your actual password)
    sudo_password = ""
    n_tests = 5  # Number of test iterations

    import csv
    import os

    commands = [
        ("make run_custom_cable", sudo_password),  # Requires sudo
        ("make run_rx", None),  # No sudo required
        ("make run_tx", None),  # No sudo required
    ]

    csv_filename = "tx_timing_results.csv"
    gif_path = "penguin-received.gif"
    target_size = 10968

    # Prepare CSV file
    with open(csv_filename, mode="w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["Test Number", "Elapsed Time (seconds)"])

    for test_num in range(n_tests):
        print(f"Test iteration {test_num + 1}/{n_tests}")
        procs = []
        timer_started = False
        start_time = None
        elapsed_time = None

        for cmd, pwd in commands:
            print(f"Running: {cmd}")
            proc = open_terminal_with_command(cmd, password=pwd)
            procs.append(proc)
            time.sleep(2)  # Wait 2 seconds before starting the next command
            # Start timer when 'make run_tx' is executed
            if cmd == "make run_tx":
                start_time = time.time()
                timer_started = True

        # Wait until penguin-received.gif reaches target size
        if timer_started:
            while True:
                if os.path.exists(gif_path):
                    size = os.path.getsize(gif_path)
                    if size == target_size:
                        elapsed_time = time.time() - start_time
                        print(f"penguin-received.gif reached {target_size} bytes. Elapsed time: {elapsed_time:.4f} seconds")
                        break
                time.sleep(0.1)

        # Store result in CSV
        with open(csv_filename, mode="a", newline="", encoding="utf-8") as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow([test_num + 1, f"{elapsed_time:.4f}" if elapsed_time else "N/A"])

        # Close all terminals by terminating the processes
        for proc in procs:
            proc.terminate()
        print("Closed all terminals. Restarting loop...\n")


if __name__ == "__main__":
    main()
