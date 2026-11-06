import hid
import time

VID = 0xCAFE
PID = 0x4001
REPORT_SIZE = 64

device = hid.device()
device.open(VID, PID)


def write_cmd(data: bytes):
    raw = data + b"\x00"
    if len(raw) > REPORT_SIZE:
        raise ValueError("Command too long for 64-byte HID report")
    device.write(raw + b"\x00" * (REPORT_SIZE - len(raw)))


def read_resp(timeout_ms: int = 200) -> bytes:
    data = bytes(device.read(REPORT_SIZE, timeout_ms))
    nul = data.find(b"\x00")
    return data[:nul] if nul >= 0 else data


def send_status_cmd(cmd: str):
    write_cmd(cmd.encode("ascii"))
    resp = read_resp().decode("ascii", errors="replace")

    if len(resp) < 2 or resp[1] != " " or resp[0] not in ("0", "1"):
        raise RuntimeError(f"Malformed status response: {resp!r}")

    return resp[0], resp[2:]


def uart_send(text: str):
    write_cmd(f"S {text}".encode("utf-8"))
    read_resp()
    time.sleep(0.05)


def uart_read() -> bytes:
    write_cmd(b"R")
    return read_resp()

# ---- examples ----

status, payload = send_status_cmd("V")
print("Version:", status, payload)

status, payload = send_status_cmd("J adxl343 {0}")
print("ADXL343 ID:", status, payload)

status, payload = send_status_cmd("I tsl2591 {0, 3, 1, 0}")
print("TSL2591 setup:", status, payload)
time.sleep(0.5)

status, payload = send_status_cmd("J tsl2591 {20,21,22,23}")
print("TSL2591 channels:", status, payload)

uart_send("hello from Python\r\n")
print("UART bytes:", uart_read())

device.close()