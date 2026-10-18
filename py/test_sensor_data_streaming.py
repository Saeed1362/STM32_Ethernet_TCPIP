import socket
import csv
import time

STM32_IP = "192.168.10.2"
PORT = 6000

with socket.create_connection((STM32_IP, PORT), timeout=5) as sock:
    sock.settimeout(3)

    print("Connected to STM32 sensor server")

    buffer = ""

    with open("sensor_data.csv", "w", newline="") as file:
        writer = csv.writer(file)
        writer.writerow(["Time_ms", "ADC_raw"])

        try:
            while True:
                data = sock.recv(1024)

                if not data:
                    print("STM32 disconnected")
                    break

                buffer += data.decode("ascii")

                while "\n" in buffer:
                    line, buffer = buffer.split("\n", 1)

                    try:
                        timestamp, value = line.strip().split(",")
                        timestamp = int(timestamp)
                        value = int(value)
                    except ValueError:
                        continue

                    writer.writerow([timestamp, value])
                    file.flush()

                    print(f"Time: {timestamp} ms | ADC: {value}")

        except socket.timeout:
            print("No data received for 3 seconds")

        except KeyboardInterrupt:
            print("\nStreaming stopped")


            