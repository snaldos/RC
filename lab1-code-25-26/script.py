import csv
import os
import subprocess
import time

# Default experiment parameters
DEFAULT_PROP_DELAY = 0
DEFAULT_BYTE_ERR = 0
DEFAULT_BAUDRATE = 9600
DEFAULT_MAX_PAYLOAD_SIZE = 1000
DEFAULT_TARGET_SIZE = 10968
DEFAULT_GIF_PATH = "penguin-received.gif"
DEFAULT_N_TESTS = 1
DEFAULT_GIVE_UP_TIME = 30  # seconds
DEFAULT_SUDO_PASSWORD = (
    "your_sudo_password_here"  # Replace with your actual sudo password
)


# Calculate Frame Error Rate (FER) from Byte Error Rate (BER) and frame size
def calculate_fer_from_ber(ber, frame_size_bytes):
    # FER = 1 - (1 - BER) ** frame_size_bytes
    if ber is not None and frame_size_bytes > 0:
        return 1 - (1 - ber) ** frame_size_bytes
    return None


# Calculate theoretical efficiency for Stop-and-Wait ARQ
def calculate_theoretical_efficiency(ber, prop_delay, frame_size_bytes, baudrate):
    fer = calculate_fer_from_ber(ber, frame_size_bytes)
    L = frame_size_bytes * 8
    T_frame = L / baudrate if baudrate else None
    a = (prop_delay / 1_000_000) / T_frame if T_frame else None
    if fer is not None and a is not None:
        return (1 - fer) / (1 + 2 * a)
    return None

    # Calculate R


def calculate_throughput(file_size_bytes, elapsed_time):
    if elapsed_time and elapsed_time > 0:
        return (file_size_bytes * 8) / elapsed_time
    return None

    # Calculate S_measured


def calculate_measured_efficiency(throughput, link_rate):
    if throughput and link_rate and throughput > 0 and link_rate > 0:
        efficiency = throughput / link_rate
        if efficiency > 1.0:
            return 1.0
        return efficiency
    return None


def calculate_theoretical_frame_transmission_time(frame_size_bytes, baudrate):
    if baudrate and baudrate > 0:
        return (frame_size_bytes * 8) / baudrate
    return None

    # Calculate "a"


def calculate_theoretical_delay_ratio(frame_size_bytes, baudrate, prop_delay):
    transmission_time = calculate_theoretical_frame_transmission_time(
        frame_size_bytes, baudrate
    )
    if transmission_time is not None:
        return (prop_delay / 1_000_000) / transmission_time
    return None

    # Calculate S_theoretical wo errors


def calculate_theoretical_efficiency_without_errors(
    frame_size_bytes, baudrate, prop_delay
):
    a = calculate_theoretical_delay_ratio(frame_size_bytes, baudrate, prop_delay)
    if a is not None:
        return 1 / (1 + 2 * a)
    return None


def open_terminal_with_command(command, password=None):
    """
    Opens a new terminal window and runs the specified command.
    If a password is provided, it is used for sudo commands.
    """
    terminal_emulator = "kitty"  # Change this to your preferred terminal emulator
    if password:
        command = f"echo {password} | sudo -S {command}"
    return subprocess.Popen([terminal_emulator, "bash", "-c", f"{command}; exec bash"])


def main():
    # Sudo password (replace with your actual password)
    sudo_password = DEFAULT_SUDO_PASSWORD

    prop_delays = [
        DEFAULT_PROP_DELAY,
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
        DEFAULT_BYTE_ERR,
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
    baudrates = [1200, 1800, 2400, 4800, DEFAULT_BAUDRATE, 19200, 38400, 57600, 115200]
    max_payload_sizes = [
        50,
        100,
        200,
        500,
        600,
        700,
        800,
        900,
        DEFAULT_MAX_PAYLOAD_SIZE,
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
    n_tests = DEFAULT_N_TESTS
    gif_path = DEFAULT_GIF_PATH
    target_size = DEFAULT_TARGET_SIZE

    run_prop_delay_tests(prop_delays, n_tests, gif_path, target_size, sudo_password)
    run_byte_err_tests(byte_errs, n_tests, gif_path, target_size, sudo_password)
    run_baudrate_tests(baudrates, n_tests, gif_path, target_size, sudo_password)
    run_max_payload_size_tests(
        max_payload_sizes, n_tests, gif_path, target_size, sudo_password
    )


def run_prop_delay_tests(prop_delays, n_tests, gif_path, target_size, sudo_password):

    csv_filename = "test_prop_delay.csv"
    with open(csv_filename, mode="w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(
            [
                "PropDelay (us)",
                "Test Number",
                "Elapsed Time (seconds)",
                "Throughput R (bps)",
                "Measured Efficiency S",
                "Theoretical Efficiency S_theoretical",
            ]
        )
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
                            f"penguin-received.gif reached {target_size} bytes. Elapsed time: {elapsed_time:.6f} seconds"
                        )
                        break
                    if size != last_size:
                        last_size = size
                        last_change_time = time.time()
                    elif time.time() - last_change_time > DEFAULT_GIVE_UP_TIME:
                        elapsed_time = -1
                        print(
                            f"File size stuck at {size} bytes for {DEFAULT_GIVE_UP_TIME} seconds. Skipping test."
                        )
                        break
                    time.sleep(0.1)
                    print(time.time() - last_change_time)
            throughput = calculate_throughput(target_size, elapsed_time)
            measured_efficiency = calculate_measured_efficiency(
                throughput, DEFAULT_BAUDRATE
            )
            theoretical_efficiency = calculate_theoretical_efficiency(
                DEFAULT_BYTE_ERR, prop_delay, DEFAULT_MAX_PAYLOAD_SIZE, DEFAULT_BAUDRATE
            )
            with open(csv_filename, mode="a", newline="", encoding="utf-8") as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow(
                    [
                        prop_delay,
                        test_num + 1,
                        f"{elapsed_time:.6f}" if elapsed_time >= 0 else "N/A",
                        f"{throughput:.2f}" if throughput is not None else "N/A",
                        (
                            f"{measured_efficiency:.4f}"
                            if measured_efficiency is not None
                            else "N/A"
                        ),
                        (
                            f"{theoretical_efficiency:.4f}"
                            if theoretical_efficiency is not None
                            else "N/A"
                        ),
                    ]
                )
            for proc in procs:
                proc.terminate()
            print("Closed all terminals. Restarting loop...\n")


def run_byte_err_tests(byte_errs, n_tests, gif_path, target_size, sudo_password):

    byte_err_csv = "test_byte_err.csv"
    with open(byte_err_csv, mode="w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(
            [
                "ByteErr",
                "Test Number",
                "Elapsed Time (seconds)",
                "Throughput R (bps)",
                "Measured Efficiency S",
                "Theoretical Efficiency S_theoretical",
            ]
        )
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
                            f"penguin-received.gif reached {target_size} bytes. Elapsed time: {elapsed_time:.6f} seconds"
                        )
                        break
                    if size != last_size:
                        last_size = size
                        last_change_time = time.time()
                    elif time.time() - last_change_time > DEFAULT_GIVE_UP_TIME:
                        elapsed_time = -1
                        print(
                            f"File size stuck at {size} bytes for {DEFAULT_GIVE_UP_TIME} seconds. Skipping test."
                        )
                        break
                    time.sleep(0.1)
                    print(time.time() - last_change_time)
            throughput = calculate_throughput(target_size, elapsed_time)
            measured_efficiency = calculate_measured_efficiency(
                throughput, DEFAULT_BAUDRATE
            )
            theoretical_efficiency = calculate_theoretical_efficiency(
                byte_err, DEFAULT_PROP_DELAY, DEFAULT_MAX_PAYLOAD_SIZE, DEFAULT_BAUDRATE
            )
            with open(byte_err_csv, mode="a", newline="", encoding="utf-8") as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow(
                    [
                        byte_err,
                        test_num + 1,
                        f"{elapsed_time:.6f}" if elapsed_time >= 0 else "N/A",
                        f"{throughput:.2f}" if throughput is not None else "N/A",
                        (
                            f"{measured_efficiency:.4f}"
                            if measured_efficiency is not None
                            else "N/A"
                        ),
                        (
                            f"{theoretical_efficiency:.4f}"
                            if theoretical_efficiency is not None
                            else "N/A"
                        ),
                    ]
                )
            for proc in procs:
                proc.terminate()
            print("Closed all terminals. Restarting loop...\n")


def run_baudrate_tests(baudrates, n_tests, gif_path, target_size, sudo_password):

    baudrate_csv = "test_baudrate.csv"
    with open(baudrate_csv, mode="w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(
            [
                "Baudrate",
                "Test Number",
                "Elapsed Time (seconds)",
                "Throughput R (bps)",
                "Measured Efficiency S",
                "Theoretical Efficiency S_theoretical",
            ]
        )
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
                            f"penguin-received.gif reached {target_size} bytes. Elapsed time: {elapsed_time:.6f} seconds"
                        )
                        break
                    if size != last_size:
                        last_size = size
                        last_change_time = time.time()
                    elif time.time() - last_change_time > DEFAULT_GIVE_UP_TIME:
                        elapsed_time = -1
                        print(
                            f"File size stuck at {size} bytes for {DEFAULT_GIVE_UP_TIME} seconds. Skipping test."
                        )
                        break
                    time.sleep(0.1)
                    print(time.time() - last_change_time)
            throughput = calculate_throughput(target_size, elapsed_time)
            measured_efficiency = calculate_measured_efficiency(throughput, baudrate)
            theoretical_efficiency = calculate_theoretical_efficiency(
                DEFAULT_BYTE_ERR, DEFAULT_PROP_DELAY, DEFAULT_MAX_PAYLOAD_SIZE, baudrate
            )
            with open(baudrate_csv, mode="a", newline="", encoding="utf-8") as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow(
                    [
                        baudrate,
                        test_num + 1,
                        f"{elapsed_time:.6f}" if elapsed_time >= 0 else "N/A",
                        f"{throughput:.2f}" if throughput is not None else "N/A",
                        (
                            f"{measured_efficiency:.4f}"
                            if measured_efficiency is not None
                            else "N/A"
                        ),
                        (
                            f"{theoretical_efficiency:.4f}"
                            if theoretical_efficiency is not None
                            else "N/A"
                        ),
                    ]
                )
            for proc in procs:
                proc.terminate()
            print("Closed all terminals. Restarting loop...\n")


def run_max_payload_size_tests(
    max_payload_sizes, n_tests, gif_path, target_size, sudo_password
):

    payload_csv = "test_max_payload_size.csv"
    with open(payload_csv, mode="w", newline="", encoding="utf-8") as csvfile:
        writer = csv.writer(csvfile)
        writer.writerow(
            [
                "MaxPayloadSize",
                "Test Number",
                "Elapsed Time (seconds)",
                "Throughput R (bps)",
                "Measured Efficiency S",
                "Theoretical Efficiency S_theoretical",
            ]
        )
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
                            f"penguin-received.gif reached {target_size} bytes. Elapsed time: {elapsed_time:.6f} seconds"
                        )
                        break
                    if size != last_size:
                        last_size = size
                        last_change_time = time.time()
                    elif time.time() - last_change_time > DEFAULT_GIVE_UP_TIME:
                        elapsed_time = -1
                        print(
                            f"File size stuck at {size} bytes for {DEFAULT_GIVE_UP_TIME} seconds. Skipping test."
                        )
                        break
                    time.sleep(0.1)
                    print(time.time() - last_change_time)
            throughput = calculate_throughput(target_size, elapsed_time)
            measured_efficiency = calculate_measured_efficiency(
                throughput, DEFAULT_BAUDRATE
            )
            theoretical_efficiency = calculate_theoretical_efficiency(
                DEFAULT_BYTE_ERR, DEFAULT_PROP_DELAY, payload_size, DEFAULT_BAUDRATE
            )
            with open(payload_csv, mode="a", newline="", encoding="utf-8") as csvfile:
                writer = csv.writer(csvfile)
                writer.writerow(
                    [
                        payload_size,
                        test_num + 1,
                        f"{elapsed_time:.6f}" if elapsed_time >= 0 else "N/A",
                        f"{throughput:.2f}" if throughput is not None else "N/A",
                        (
                            f"{measured_efficiency:.4f}"
                            if measured_efficiency is not None
                            else "N/A"
                        ),
                        (
                            f"{theoretical_efficiency:.4f}"
                            if theoretical_efficiency is not None
                            else "N/A"
                        ),
                    ]
                )
            for proc in procs:
                proc.terminate()
            print("Closed all terminals. Restarting loop...\n")


if __name__ == "__main__":
    main()
