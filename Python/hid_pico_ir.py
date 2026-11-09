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

    if cmd.strip() == "G" and resp.startswith("{") and resp.endswith("}"):
        return "0", resp

    if len(resp) < 2 or resp[1] != " " or resp[0] not in ("0", "1"):
        raise RuntimeError(f"Malformed status response: {resp!r}")

    return resp[0], resp[2:]


def decode_ir_payload(payload: str) -> str:
    values = payload[1:-1].split(",")
    characters = []
    for value in values:
        byte = int(value.strip())
        if byte == 4:
            characters.append("\n")
        elif 32 <= byte <= 126:
            characters.append(chr(byte))
        else:
            characters.append(" ")
    return "".join(characters)


def uart_send(text: str):
    write_cmd(f"S {text}".encode("utf-8"))
    read_resp()
    time.sleep(0.05)


def uart_read() -> bytes:
    write_cmd(b"R")
    return read_resp()

idle_timeout = 2.0
last_data_at = None
while True:
    status, payload = send_status_cmd("G")
    if status == "1":
        raise RuntimeError(f"IR read failed: {payload}")
    if payload == "{}":
        if last_data_at is not None and time.monotonic() - last_data_at >= idle_timeout:
            break
        time.sleep(0.1)
        continue
    print(decode_ir_payload(payload), end="", flush=True)
    last_data_at = time.monotonic()
    time.sleep(0.05)

device.close()