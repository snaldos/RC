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
    sudo_password = "your_password_here"
    import csv
    import os

    prop_delays = [
        0,
        1000,
        10000,
        100000,
        1000000,
        2000000,
        3000000,
        4000000,
        5000000,
        6000000,
        7000000,
        8000000,
    ]
    byte_errs = [
        0,
        0.00005,
        0.00006,
        0.00007,
        0.00008,
        0.00009,
        0.0001,
        0.0002,
        0.0003,
        0.0004,
        0.0005,
        0.0006,
        0.0007,
        0.0008,
        0.0009,
        0.001,
        0.005,
        0.006,
        0.007,
        0.008,
        0.009,
        0.01,
        0.1,
    ]
    baudrates = [1200, 1800, 2400, 4800, 9600, 19200, 38400, 57600, 115200]
    max_payload_sizes = [
        8000,
        50,
        100,
        200,
        500,
        800,
        1000,
        2000,
        3000,
        4000,
        5000,
        6000,
        7000,
        8000,
        9000,
        10000,
    ]
    n_tests = 1  # Number of test iterations per delay

    gif_path = "penguin-received.gif"
    target_size = 10968

    # --- Prop Delay Test ---
    csv_filename = "test_prop_delay.csv"
    with open(csv_filename, mode="w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["PropDelay (us)", "Test Number", "Elapsed Time (seconds)"])

    for prop_delay in prop_delays:
        for test_num in range(n_tests):
            print(f"Testing prop_delay={prop_delay} (Test {test_num + 1}/{n_tests})")
            procs = []
            timer_started = False
            start_time = None
            elapsed_time = None

            if os.path.exists(gif_path):
                os.remove(gif_path)

            commands = [
                (
                    f"make run_custom_cable CUSTOM_PROP_DELAY={prop_delay}",
                    sudo_password,
                ),
                ("make run_rx", None),
                ("make run_tx", None),
            ]

            for cmd, pwd in commands:
                print(f"Running: {cmd}")
                proc = open_terminal_with_command(cmd, password=pwd)
                procs.append(proc)
                time.sleep(2)
                if cmd == "make run_tx":
                    start_time = time.time()
                    timer_started = True

            if timer_started:
                last_size = None
                size = None
                last_change_time = time.time()
                while True:
                    if os.path.exists(gif_path):
                        size = os.path.getsize(gif_path)
                    if size == target_size:
                        elapsed_time = time.time() - start_time
                        print(
                            f"penguin-received.gif reached {target_size} bytes. Elapsed time: {elapsed_time:.4f} seconds"
                        )
                        break
                    if size != last_size:
                        last_size = size
                        last_change_time = time.time()
                    elif time.time() - last_change_time > 30:
                        elapsed_time = -1
                        print(
                            f"File size stuck at {size} bytes for 30 seconds. Skipping test."
                        )
                        break
                    time.sleep(0.1)
                    print(time.time() - last_change_time)

            with open(csv_filename, mode="a", newline="", encoding="utf-8") as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow(
                    [
                        prop_delay,
                        test_num + 1,
                        f"{elapsed_time:.4f}" if elapsed_time else "N/A",
                    ]
                )

            for proc in procs:
                proc.terminate()
            print("Closed all terminals. Restarting loop...\n")

    # --- Byte Error Test ---
    byte_err_csv = "test_byte_err.csv"
    with open(byte_err_csv, mode="w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["ByteErr", "Test Number", "Elapsed Time (seconds)"])

    for byte_err in byte_errs:
        for test_num in range(n_tests):
            print(f"Testing byte_err={byte_err} (Test {test_num + 1}/{n_tests})")
            procs = []
            timer_started = False
            start_time = None
            elapsed_time = None

            if os.path.exists(gif_path):
                os.remove(gif_path)

            commands = [
                (f"make run_custom_cable CUSTOM_BYTE_ERR={byte_err}", sudo_password),
                ("make run_rx", None),
                ("make run_tx", None),
            ]

            for cmd, pwd in commands:
                print(f"Running: {cmd}")
                proc = open_terminal_with_command(cmd, password=pwd)
                procs.append(proc)
                time.sleep(2)
                if cmd == "make run_tx":
                    start_time = time.time()
                    timer_started = True

            if timer_started:
                last_size = None
                size = None
                last_change_time = time.time()
                while True:
                    if os.path.exists(gif_path):
                        size = os.path.getsize(gif_path)
                    if size == target_size:
                        elapsed_time = time.time() - start_time
                        print(
                            f"penguin-received.gif reached {target_size} bytes. Elapsed time: {elapsed_time:.4f} seconds"
                        )
                        break
                    if size != last_size:
                        last_size = size
                        last_change_time = time.time()
                    elif time.time() - last_change_time > 30:
                        elapsed_time = -1
                        print(
                            f"File size stuck at {size} bytes for 30 seconds. Skipping test."
                        )
                        break
                    time.sleep(0.1)
                    print(time.time() - last_change_time)

            with open(byte_err_csv, mode="a", newline="", encoding="utf-8") as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow(
                    [
                        byte_err,
                        test_num + 1,
                        f"{elapsed_time:.4f}" if elapsed_time else "N/A",
                    ]
                )

            for proc in procs:
                proc.terminate()
            print("Closed all terminals. Restarting loop...\n")

    # --- Baudrate Test ---
    baudrate_csv = "test_baudrate.csv"
    with open(baudrate_csv, mode="w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["Baudrate", "Test Number", "Elapsed Time (seconds)"])

    for baudrate in baudrates:
        for test_num in range(n_tests):
            print(f"Testing baudrate={baudrate} (Test {test_num + 1}/{n_tests})")
            procs = []
            timer_started = False
            start_time = None
            elapsed_time = None

            if os.path.exists(gif_path):
                os.remove(gif_path)

            commands = [
                (f"make run_custom_cable CUSTOM_BAUDRATE={baudrate}", sudo_password),
                (f"make run_rx CUSTOM_BAUDRATE={baudrate}", None),
                (f"make run_tx CUSTOM_BAUDRATE={baudrate}", None),
            ]

            for cmd, pwd in commands:
                print(f"Running: {cmd}")
                proc = open_terminal_with_command(cmd, password=pwd)
                procs.append(proc)
                time.sleep(2)
                if cmd.startswith("make run_tx"):
                    start_time = time.time()
                    timer_started = True

            if timer_started:
                last_size = None
                size = None
                last_change_time = time.time()
                while True:
                    if os.path.exists(gif_path):
                        size = os.path.getsize(gif_path)
                    if size == target_size:
                        elapsed_time = time.time() - start_time
                        print(
                            f"penguin-received.gif reached {target_size} bytes. Elapsed time: {elapsed_time:.4f} seconds"
                        )
                        break
                    if size != last_size:
                        last_size = size
                        last_change_time = time.time()
                    elif time.time() - last_change_time > 30:
                        elapsed_time = -1
                        print(
                            f"File size stuck at {size} bytes for 30 seconds. Skipping test."
                        )
                        break
                    time.sleep(0.1)
                    print(time.time() - last_change_time)

            with open(baudrate_csv, mode="a", newline="", encoding="utf-8") as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow(
                    [
                        baudrate,
                        test_num + 1,
                        f"{elapsed_time:.4f}" if elapsed_time else "N/A",
                    ]
                )

            for proc in procs:
                proc.terminate()
            print("Closed all terminals. Restarting loop...\n")

    # --- Max Payload Size Test ---
    payload_csv = "test_max_payload_size.csv"
    with open(payload_csv, mode="w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(["MaxPayloadSize", "Test Number", "Elapsed Time (seconds)"])

    for payload_size in max_payload_sizes:
        for test_num in range(n_tests):
            print(
                f"Testing max_payload_size={payload_size} (Test {test_num + 1}/{n_tests})"
            )
            procs = []
            timer_started = False
            start_time = None
            elapsed_time = None

            if os.path.exists(gif_path):
                os.remove(gif_path)

            commands = [
                ("make run_custom_cable", sudo_password),
                (f"make run_rx CUSTOM_MAX_PAYLOAD_SIZE={payload_size}", None),
                (f"make run_tx CUSTOM_MAX_PAYLOAD_SIZE={payload_size}", None),
            ]

            for cmd, pwd in commands:
                print(f"Running: {cmd}")
                proc = open_terminal_with_command(cmd, password=pwd)
                procs.append(proc)
                time.sleep(2)
                if cmd.startswith("make run_tx"):
                    start_time = time.time()
                    timer_started = True

            if timer_started:
                last_size = None
                size = None
                last_change_time = time.time()
                while True:
                    if os.path.exists(gif_path):
                        size = os.path.getsize(gif_path)
                    if size == target_size:
                        elapsed_time = time.time() - start_time
                        print(
                            f"penguin-received.gif reached {target_size} bytes. Elapsed time: {elapsed_time:.4f} seconds"
                        )
                        break
                    if size != last_size:
                        last_size = size
                        last_change_time = time.time()
                    elif time.time() - last_change_time > 30:
                        elapsed_time = -1
                        print(
                            f"File size stuck at {size} bytes for 30 seconds. Skipping test."
                        )
                        break
                    time.sleep(0.1)
                    print(time.time() - last_change_time)

            with open(payload_csv, mode="a", newline="", encoding="utf-8") as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow(
                    [
                        payload_size,
                        test_num + 1,
                        f"{elapsed_time:.4f}" if elapsed_time else "N/A",
                    ]
                )

            for proc in procs:
                proc.terminate()
            print("Closed all terminals. Restarting loop...\n")


if __name__ == "__main__":
    main()
