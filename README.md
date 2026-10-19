# STM32F7508-DK Ethernet & TCP/IP Labs

Hands-on embedded networking labs using the **STM32F7508-DK Discovery Kit**, its onboard **LAN8742 Ethernet PHY**, and **LwIP (Raw API, standalone/no RTOS)**. A Windows laptop communicates with the board over a direct Ethernet cable—**no router or Internet access required**.

## Labs

| Lab | Git branch | Service | Port | What to test |
| --- | --- | --- | ---: | --- |
| 0. Ethernet bring-up | Any configured lab branch | ARP / ICMP | — | Ping the board |
| 1. TCP LED controller | `tcp_server` | Custom line-based TCP | 5000 | Send `LED_ON` / `LED_OFF` |
| 2. HTTP web server | `http_web_server` | HTTP/1.1 | 80 | Control LED from browser |
| 3. TCP sensor stream | `tcp_data_stream` | Custom line-based TCP | 6000 | Receive periodic samples and save CSV |
| 4. EtherNet/IP explicit messaging | `EtherNetIP` | EtherNet/IP encapsulation + CIP over TCP | 44818 | Read identity, register a session, and control/read back LED state |

> **Scope:** The four application labs above have been tested during development. Lab 4 implements a limited educational EtherNet/IP explicit-messaging subset; it is not a complete or certified adapter and does not implement cyclic I/O. A separate binary TCP echo server / advanced Ethernet porting lab is **not** included unless added to the firmware. The `ADC_raw` field in the sensor lab is initially **simulated**; it is not a physical ADC measurement until the firmware is updated to sample an ADC channel.

## Select a lab branch

Each application lab has its own Git branch. Branch names are case-sensitive; use `EtherNetIP` exactly as shown. `master` is also present, but do not assume it contains every lab; use the named lab branch for each test.

From the repository folder in PowerShell, check your working tree and list branches:

```powershell
git status
git branch --all
```

Commit any changes you want to keep, or stash unfinished tracked changes with `git stash push -m "Work before switching labs"`. If you also need to stash untracked source files, use `git stash push -u` instead. Then switch to the lab you want to test:

| Lab | Switch command |
| --- | --- |
| TCP LED controller | `git switch tcp_server` |
| HTTP web server | `git switch http_web_server` |
| TCP sensor stream | `git switch tcp_data_stream` |
| EtherNet/IP explicit messaging | `git switch EtherNetIP` |
| Return to master | `git switch master` |

If a fresh clone lists a lab only under `remotes/origin/`, create its local tracking branch. For example:

```powershell
git fetch origin
git switch --track origin/EtherNetIP
```

Use this tracking command only when the local branch does not already exist. The lab branches must have been pushed to the remote to be available in another clone.

**After every branch switch:**

1. Stop the active debug session and refresh the project in STM32CubeIDE (**F5**).
2. Clean the project (**Project → Clean**) and rebuild it so the executable matches the selected branch.
3. Flash the newly built firmware and resume execution.
4. Run Lab 0's connectivity check, then the selected lab's test instructions below. Run its Python script from the folder containing that script on the selected branch.

**Switching Git branches changes source files on the laptop; it does not change firmware already running on the board.** A missing port may simply mean the board still runs another lab's firmware. A branch may contain earlier labs too, but only expect services explicitly initialized by that branch's `main.c`.

## Hardware and software

- STM32F7508-DK Discovery Kit, powered/programmed via ST-LINK USB
- Ethernet cable from the board's RJ45 connector to the Windows laptop's Ethernet port or USB-to-Ethernet adapter
- STM32CubeIDE (with the project's HAL, LwIP and LAN8742 support files)
- Windows PowerShell; Python 3 for the TCP client tests
- Optional: Wireshark with Npcap for packet capture

### Network layout

```text
Windows laptop / Ethernet 2                 STM32F7508-DK
IP 192.168.10.3/24                          IP 192.168.10.2/24
TCP client + Web browser  <--- cable --->   LwIP TCP/HTTP servers
```

Configure the laptop's **physical Ethernet adapter** (not VMware VMnet1/VMnet8) with:

| Setting | Laptop | STM32 |
| --- | --- | --- |
| IPv4 address | `192.168.10.3` | `192.168.10.2` |
| Subnet mask | `255.255.255.0` | `255.255.255.0` |
| Gateway | Blank | `0.0.0.0` |
| DHCP | Disabled/manual | Disabled |

On the STM32, configure the static IPv4 settings in STM32CubeMX / LwIP and build and flash the firmware. These addresses are examples: change **both the documentation and test commands** if you use another subnet. Wi-Fi may stay on for Internet access; the Ethernet cable is used for board traffic.

<<<<<<< HEAD
### Firmware startup

The standalone LwIP build must initialize the network interface and **continue processing packets**. Initialize the server(s) included in the selected branch. The example below shows a combined build for reference; it is not a requirement for every lab branch:

```c
// In the USER CODE Includes section:
#include "eip_server.h"

MX_GPIO_Init();
MX_LWIP_Init();

TCP_Server_Init();     // LED commands, port 5000
HTTP_Server_Init();    // Web server, port 80
Sensor_Server_Init();  // Streaming, port 6000

// In USER CODE BEGIN 2, after MX_LWIP_Init():
if (EIP_Server_Init() != ERR_OK) {  // EtherNet/IP, port 44818
    Error_Handler();
}

while (1) {
    MX_LWIP_Process();
    Sensor_Server_Process();
}
```

Only include `eip_server.h` and call `EIP_Server_Init()` where the EtherNet/IP module is present. Similarly, only call `Sensor_Server_Process()` if the sensor module is present and initialized. Keep custom includes and initialization inside CubeMX USER CODE sections. The function names assume the corresponding modules are included in your build. Do not add `MX_ETH_Init()` unless your particular CubeMX-generated project actually defines it; Ethernet HAL initialization may occur in `ethernetif.c`. Avoid long blocking `HAL_Delay()` calls in the processing loop. For your onboard LED, confirm whether the GPIO output is active-high or active-low.

=======
>>>>>>> 78d97d9b84a1f78f1aa2267fba0e3beefcddc6a0
## Lab 0 — Ethernet bring-up / Ping

**Purpose:** Check Ethernet link, ARP, IPv4 and ICMP before testing TCP. Run this check after flashing any configured lab branch; no separate bring-up branch is shown in this repository.

1. Connect and power the board; flash firmware with LwIP initialized and serviced.
2. On Windows PowerShell, identify the physical adapter:

   ```powershell
   Get-NetAdapter | Format-Table Name, InterfaceDescription, Status, LinkSpeed, ifIndex
   Get-NetIPAddress -AddressFamily IPv4 | Format-Table InterfaceAlias, IPAddress, PrefixLength
   ```

3. Ping from the laptop's static Ethernet IP:

   ```powershell
   ping -S 192.168.10.3 192.168.10.2
   ```

4. Optionally inspect ARP:

   ```powershell
   arp -a -N 192.168.10.3
   ```

**Pass:** Ping receives replies with no packet loss under normal conditions. The ARP table can show `192.168.10.2` mapped to an Ethernet MAC address. A link showing `100 Mbps` is normal for the 10/100 Mbps PHY.

**Wireshark:** Capture on the actual USB/physical Ethernet interface, with display filter `arp || icmp`. To capture a fresh ARP resolution, clear the cached entry from an elevated PowerShell (`arp -d 192.168.10.2`) before pinging again.

## Lab 1 — TCP LED controller (port 5000)

**Branch:** `tcp_server` — run `git switch tcp_server`, then clean, rebuild, and flash before testing.

**Purpose:** Send newline-delimited commands to LwIP's TCP Raw API server and control an LED.

1. Verify ping works (Lab 0).
2. Confirm that the STM32 firmware starts the LED TCP server on port `5000`.
3. Check the listening port from PowerShell:

   ```powershell
   Test-NetConnection 192.168.10.2 -Port 5000
   ```

   **Pass:** `TcpTestSucceeded : True`.
4. Run your Python TCP client (use your existing client script, or the example below). Submit each command by pressing Enter:

   ```text
   LED_ON
   LED_OFF
   ```

   **Expected:** The LED changes state, and the STM32 responds with `OK LED ON` / `OK LED OFF` (or the equivalent messages implemented in your firmware).

### Minimal interactive Python client

Run with `test_tcp_server.py.py`, or execute the equivalent original Python script. **Note:** TCP is a byte stream; commands are terminated by `\n` in this simple protocol. A full production client should buffer complete response lines.

**Wireshark filter:** `tcp.port == 5000`. Restart the capture before connecting to see SYN → SYN/ACK → ACK and the command/response bytes. Try disconnecting and reconnecting the client.

## Lab 2 — HTTP web server (port 80)

**Branch:** `http_web_server` — run `git switch http_web_server`, then clean, rebuild, and flash before testing.

**Purpose:** Serve an HTML page and handle LED control requests over HTTP.

1. Verify ping, then test the TCP port:

   ```powershell
   Test-NetConnection 192.168.10.2 -Port 80
   ```

   **Pass:** `TcpTestSucceeded : True` (this alone doesn't prove HTTP works).
2. Open **`http://192.168.10.2/`** in Chrome or Edge. Use **HTTP**, not HTTPS.
3. Click **Turn ON** and **Turn OFF**. The current implementation serves these paths:

   | URL | Expected behavior |
   | --- | --- |
   | `http://192.168.10.2/` | Display HTML and LED status |
   | `http://192.168.10.2/on` | Turn LED on and return HTML |
   | `http://192.168.10.2/off` | Turn LED off and return HTML |

4. To verify HTTP independently of the browser, use:

   ```powershell
   curl.exe -v --max-time 5 http://192.168.10.2/
   curl.exe -v --max-time 5 http://192.168.10.2/on
   curl.exe -v --max-time 5 http://192.168.10.2/off
   ```

**Pass:** The response begins with `HTTP/1.1 200 OK`, includes `Content-Type: text/html`, returns an HTML document, and the LED responds to `/on` and `/off`.

**Wireshark filter:** `tcp.port == 80` or `http`. Inspect HTTP `GET /on`, response headers and HTML content.

**Browser-specific caveat:** Browsers may send more HTTP headers than `curl`. An initial 256-byte request buffer proved too small during this project; a 1024-byte buffer allowed browser testing. A production server should instead parse headers incrementally and handle oversized requests explicitly.

## Lab 3 — TCP sensor streaming (port 6000)

**Branch:** `tcp_data_stream` — run `git switch tcp_data_stream`, then clean, rebuild, and flash before testing.

**Purpose:** Stream samples from STM32 to a Python client approximately every `100 ms` without blocking the LwIP processing loop.

1. Confirm the STM32 firmware calls `Sensor_Server_Init()` and `Sensor_Server_Process()`.
2. Verify connectivity:

   ```powershell
   Test-NetConnection 192.168.10.2 -Port 6000
   ```

3. Run your existing Python sensor receiver (the version that writes the readings to CSV). Example:

   ```powershell
   python test_sensor_data_streaming.py
   ```

4. Watch incoming lines such as:

   ```text
   10200,1000
   10300,1010
   10400,1020
   ```

   The fields are `time_ms,adc_raw` in the initial firmware. **`adc_raw` is a simulated value** until the ADC integration is implemented.
5. Verify the CSV contains a header and multiple samples, e.g.:

   ```csv
   Time_ms,ADC_raw
   10200,1000
   10300,1010
   10400,1020
   ```

6. Stop the Python client with **Ctrl+C**; reconnect and confirm streaming resumes.

**Pass:** Samples arrive continuously at roughly the configured interval, are decoded into separate records and saved to CSV. Network scheduling means the laptop's observed arrival intervals may vary, even when the firmware generates samples every 100 ms.

**Wireshark filter:** `tcp.port == 6000`. Observe data packets, sequence/ACK numbers, TCP receive window and whether samples are combined into segments.

**Extension:** Enable an ADC channel in CubeMX, initialize the peripheral, replace the simulated value with `HAL_ADC_GetValue()` readings, then consider ADC + DMA for nonblocking sampling. Do not label simulated data as real measurements.

## Lab 4 — EtherNet/IP explicit messaging (TCP port 44818)

**Branch:** `EtherNetIP` — run `git switch EtherNetIP`, then clean, rebuild, and flash before testing.

**Purpose:** Use a Python client to communicate with a small EtherNet/IP implementation on STM32. Read the device identity, register a session, and use CIP requests to control and read back the onboard LED.

### Firmware and client prerequisites

- Include `eip_server.c` and `eip_protocol.c` in `Core/Src`, and their matching headers in `Core/Inc`.
- Include `eip_server.h` in `main.c` and call `EIP_Server_Init()` after `MX_LWIP_Init()`, as shown in **Firmware startup**.
- Continue calling `MX_LWIP_Process()` in the main loop. No RTOS or additional polling function for this server is needed.
- Keep the supplied `test_eip.py` in the repository and open PowerShell in the folder containing it. Python 3's standard library is sufficient; no `pip install` is required.
- The lab uses the same LED as Labs 1 and 2. Close other LED clients/browser controls during this test so they do not change its state.

### Test procedure

1. Build and flash the firmware, resume execution if using the debugger, and verify ping as in Lab 0.
2. Check the TCP listener:

   ```powershell
   Test-NetConnection 192.168.10.2 -Port 44818
   ```

   **Expected:** `TcpTestSucceeded : True`. This checks only the TCP connection; the next step verifies EtherNet/IP and CIP.
3. Run the supplied test:

   ```powershell
   py test_eip.py 192.168.10.2
   ```

   If the Windows Python launcher is unavailable, use:

   ```powershell
   python test_eip.py 192.168.10.2
   ```

4. Verify output resembling:

   ```text
   ListIdentity: STM32 EtherNetIP Lab
   RegisterSession: OK (0x12340001)
   CIP product name: STM32 EtherNetIP Lab
   LED: ON (CIP readback confirmed)
   LED: OFF (CIP readback confirmed)
   LED: ON (CIP readback confirmed)
   LED: OFF (CIP readback confirmed)
   PASS: identity, session, CIP reads/writes, and LED readback
   ```

   The session handle can differ. The physical LED should switch on/off twice, about one second between changes, and finish off. Readback checks the GPIO state; also observe the physical LED.
5. Exercise incremental request reception:

   ```powershell
   py test_eip.py 192.168.10.2 --fragment
   ```

   **Expected:** The same successful result. This option sends requests in small pieces. TCP may coalesce writes, so it does not guarantee a particular packet or receive-callback boundary.
6. Run the normal test again to check session cleanup and reconnection.

**Pass:** Device identity is returned, session registration succeeds, CIP product-name reads work, LED set/get requests agree with the requested state, and the physical LED follows the commands.

### What the test exercises

| Operation | Command / address | Expected behavior |
| --- | --- | --- |
| Identify the board | TCP `ListIdentity` (`0x0063`) | Return the lab device identity |
| Register a session | `RegisterSession` (`0x0065`) | Return a nonzero session handle |
| Read product name | `SendRRData` (`0x006F`), CIP `Get_Attribute_Single` (`0x0E`); class `0x01`, instance `1`, attribute `7` | Return `STM32 EtherNetIP Lab` |
| Set LED | CIP `Set_Attribute_Single` (`0x10`); custom class `0x64`, instance `1`, attribute `1` | Write one byte: `1` for on, `0` for off |
| Read LED | CIP `Get_Attribute_Single` (`0x0E`); custom class `0x64`, instance `1`, attribute `1` | Return one byte matching the GPIO state |
| End session | `UnregisterSession` (`0x0066`) | Release the session; this command has no reply |

The CIP requests use **unconnected explicit messaging** carried over a TCP connection. “Unconnected” here describes CIP messaging, not the absence of a TCP connection. The LED class is a lab-specific object, not a standard LED object.

### Wireshark inspection

Start a capture on the laptop's physical Ethernet adapter **before** running the script. Apply:

```text
tcp.port == 44818
```

Inspect the TCP handshake, `ListIdentity`, `RegisterSession`, and `SendRRData` requests/responses. Expand the EtherNet/IP encapsulation header, Common Packet Format (CPF), and CIP service/path fields. Compare the product-name read with the LED set/get requests. If the protocol is not decoded automatically, try **Decode As → EtherNet/IP**, where available.

### Scope and troubleshooting

This is a small, original educational implementation using LwIP's raw API, not an OpENer port. It supports selected explicit-messaging commands and 8-bit logical CIP paths. It does **not** implement UDP discovery, `ForwardOpen`, connected CIP messaging, cyclic UDP I/O, a full standard object set, or conformance certification. Device identity values are lab placeholders.

If allocation fails with all labs enabled, temporarily disable `TCP_Server_Init()`, `HTTP_Server_Init()`, `Sensor_Server_Init()`, and `Sensor_Server_Process()` to isolate Lab 4. Keep `MX_LWIP_Init()`, `EIP_Server_Init()`, and `MX_LWIP_Process()`. If that resolves it, review the effective `MEMP_NUM_TCP_PCB_LISTEN` and `MEMP_NUM_TCP_PCB` settings in LwIP. A combined build running all four application servers requires four listener PCBs, plus sufficient active connection PCBs. A branch running only EtherNet/IP does not need four listeners. Rebuild and flash after changing configuration.

If port 44818 opens but the script fails later, capture the traffic and note the last successful operation and the reported encapsulation/CIP status. If readback succeeds but the physical LED is inverted, check the active-high/active-low mappings in both `EIP_LED_Set()` and `EIP_LED_Get()`.

## Troubleshooting

| Symptom | Likely cause / next check |
| --- | --- |
| `Destination host unreachable` or ping timeout | Check the correct physical Ethernet adapter, static IP/subnet, cable/link and `MX_LWIP_Process()` |
| Ping works, TCP port test fails | Check the relevant `*_Server_Init()` call; inspect `tcp_new()`, `tcp_bind()` and `tcp_listen()` results |
| EtherNet/IP connection refused on port 44818 | Switch to `EtherNetIP`, rebuild and flash; check `EIP_Server_Init()` result and listener PCB availability; see Lab 4 |
| Port from a different lab is closed after switching branches | Only services included and initialized in the flashed branch are expected to listen |
| Python connects but hangs after sending command | Confirm newline terminator, TCP receive callback, `tcp_write()` / `tcp_output()` and return values |
| `curl` gets HTML but browser won't load | Browser request headers may exceed fixed request buffer; inspect Wireshark and request parser |
| Build error involving `tcp_accepted` | That identifier is an LwIP compatibility macro; give your accept callback another name |
| TCP stops after disconnect/reconnect | Check `p == NULL` close handling, LwIP error callbacks and PCB state cleanup |
| Packet drops or unreliable operation | Inspect Ethernet DMA buffer placement and STM32F7 D-cache coherency; verify `ethernetif.c` |
| Link succeeds but no application response | Confirm application callbacks run; compare packets in Wireshark |

## Recommended capture filters

```text
arp || icmp
# TCP LED command server
tcp.port == 5000
# HTTP server
tcp.port == 80
# Sensor stream
tcp.port == 6000
# EtherNet/IP explicit messaging
tcp.port == 44818
```

Enter **one filter expression at a time** in Wireshark's display filter bar. Lines starting with `#` above are labels, not filters.

## Repository notes

- Keep this overview README available on `master`; carry the relevant instructions and client scripts into each lab branch as needed. Uncommitted files do not become available in every branch automatically.
- Commit source code, `.ioc`, CubeIDE project settings required to rebuild, linker scripts, HAL/BSP/LwIP dependencies and test scripts.
- Ignore generated `/Debug/`, `/Release/`, object files, executables, map files, Python `__pycache__/` and temporary editor artifacts.
- The STM32F750N8's relatively small internal Flash may require build-size optimization or an external-memory layout depending on your firmware configuration. Preserve the working linker scripts and memory configuration.
- These services are **test/demo interfaces without authentication or TLS**. Use them on a controlled, trusted test network, not on a publicly exposed network.

---

**Platform:** STM32F7508-DK · STM32CubeIDE · LwIP Raw API · LAN8742 PHY · Direct Ethernet with Windows
<<<<<<< HEAD
=======

### Lab 0 — Ethernet bring-up / Ping demo

https://github.com/user-attachments/assets/15ddb0ae-a346-4596-b974-2bd5a73daa5a


### Lab 1 — TCP LED controller demo

https://github.com/user-attachments/assets/6b9c869b-6b42-4142-9f06-7a2b071fae91


### Lab 2 — HTTP web server demo

https://github.com/user-attachments/assets/c947aecf-aa2e-46b0-a8a5-6239fca365cb


### Lab 3 — TCP sensor streaming demo

https://github.com/user-attachments/assets/ac675e46-139d-455e-821f-a708115450b3


### Lab 4 — EtherNet/IP explicit messaging demo

https://github.com/user-attachments/assets/7e0bd3a6-0a08-4ca2-8cc4-67bbce2f0652









>>>>>>> 78d97d9b84a1f78f1aa2267fba0e3beefcddc6a0
