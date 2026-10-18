import socket

SERVER_IP = "192.168.10.2"
SERVER_PORT = 5000

with socket.create_connection((SERVER_IP, SERVER_PORT),
                              timeout=5) as sock:

    while True:
        command = input("Enter command: ")

        if command.lower() == "exit":
            break

        sock.sendall((command + "\n").encode())

        response = sock.recv(1024)
        print("STM32:", response.decode().strip())