# test2_fixed.py
# Fixed version: avoids calling bool as a function and uses stable notify wrapper
# Requirements: bleak, pycryptodome
# pip install bleak pycryptodome

import asyncio
from bleak import BleakClient, BleakScanner
from Crypto.Cipher import AES
from binascii import hexlify
import struct
import sys
import time
import traceback

# ========== CONFIG ==========
DEVICE_ADDRESS = "XX:XX:XX:XX:XX:XX"   # TODO your device address (Windows)
DEVICE_NAME = None                     # optional fallback if address not set

CHAR_NOTIFY_UUID = "0000fff4-0000-1000-8000-00805f9b34fb"
CHAR_WRITE_UUID  = "0000fff3-0000-1000-8000-00805f9b34fb"

# AES key bytes exactly as present in the app (16 bytes)
AES_KEY_BYTES = bytes([
    108, 101, 97, 103, 101, 110, 100, 0xFF, 0xFE,
    49, 56, 56, 50, 52, 54, 54
])
ZERO_IV = bytes(16)

# Internal state variables matching g1.j static fields (used in crank parsing)
f4571a = 0   # timeB
f4572b = 0   # timeC
f4573c = 0
f4574d = False

# Helper: convert Java signed byte values to python 0-255 bytes for building commands
def java_signed_to_byte(b):
    return b & 0xFF

def int_list_to_signed_bytes(lst):
    return bytes([java_signed_to_byte(x) for x in lst])

# ===== Crypto helpers
def pad16_zero(data: bytes) -> bytes:
    if len(data) % 16 == 0:
        return data
    pad_len = 16 - (len(data) % 16)
    return data + b'\x00' * pad_len

def decrypt_blocks(data: bytes) -> bytes:
    if len(data) % 16 != 0:
        data = pad16_zero(data)
    cipher = AES.new(AES_KEY_BYTES, AES.MODE_CBC, ZERO_IV)
    dec = cipher.decrypt(data)
    return dec

def encrypt_cmd(plain: bytes) -> bytes:
    p = pad16_zero(plain)
    cipher = AES.new(AES_KEY_BYTES, AES.MODE_CBC, ZERO_IV)
    return cipher.encrypt(p)

def hex_str_from_bytes(b: bytes) -> str:
    return hexlify(b).decode('ascii')

def m(hexsub: str) -> int:
    return int(hexsub, 16)

# Parsing functions mirror g1.j logic (abridged)
def parse_charge(hexstr: str):
    if hexstr.startswith("fefefe"):
        print("[u] special fefefe message -> BMMessage(20003,302)")
        return {"msg": "BMMessage", "code": 302}
    try:
        status = m(hexstr[2:4])
        idleVolt = m(hexstr[4:7]) / 100.0
        highVolt = m(hexstr[7:10]) / 100.0
        print(f"[u] ChargeTestResult: status={status}, idleVolt={idleVolt:.2f}V, highVolt={highVolt:.2f}V")
        return {"status": status, "idleVolt": idleVolt, "highVolt": highVolt}
    except Exception as e:
        print("[u] parse error:", e)
        traceback.print_exc()
        return None

def parse_crank(hexstr: str):
    global f4571a, f4572b
    try:
        iM = m(hexstr[4:8])
        voltage = m(hexstr[8:11]) / 100.0
        status = m(hexstr[11:12])
        idx = hexstr.find("fffefe")
        tail = hexstr[12:idx] if idx != -1 else hexstr[12:]
        volt_list = []
        for i in range(0, len(tail), 3):
            seg = tail[i:i+3]
            if len(seg) < 3:
                break
            volt_list.append(m(seg) / 100.0)
        i4 = f4571a
        i3 = f4572b
        if i4 > 120 or i3 > 120:
            i5 = iM * 120
        else:
            i5 = ((iM * 120) - i3) + i4
        testTime = int(round(time.time() * 1000)) - max(0, i5) * 1000
        print(f"[v] CrankTest: base={iM}, voltage={voltage:.2f}V, status={status}, samples={len(volt_list)}, testTime(ms)={testTime}")
        return {"iM": iM, "voltage": voltage, "status": status, "voltages": volt_list, "testTime": testTime}
    except Exception as e:
        print("[v] parse error:", e)
        traceback.print_exc()
        return None

def parse_history(hexstr: str, ref_time_ms: int):
    recs = []
    length = len(hexstr) // 8
    for i in range(length):
        i8 = length - i - 1
        chunk = hexstr[i8*8:(i8+1)*8]
        if len(chunk) != 8:
            continue
        hv = {}
        hv['voltage'] = m(chunk[0:3]) / 100.0
        hv['type'] = m(chunk[7:8])
        hv['time'] = ref_time_ms - ((length - 1 - i) * 120000)
        recs.append(hv)
    print(f"[w] Parsed {len(recs)} historical samples (ref_time={ref_time_ms})")
    return recs

def parse_volt_message(hexstr: str):
    global f4571a, f4572b
    try:
        if len(hexstr) >= 16:
            voltage_raw = hexstr[2:5]
            status_raw  = hexstr[5:6]
            power_raw   = hexstr[6:8]
            timeB_raw   = hexstr[8:12]
            timeC_raw   = hexstr[12:16]
            voltage = m(voltage_raw) / 100.0
            status = m(status_raw)
            batteryPower = m(power_raw)
            timeB = m(timeB_raw)
            timeC = m(timeC_raw)
            f4571a = timeB
            f4572b = timeC
            print(f"[volt] voltage={voltage:.2f}V status={status} batteryPower={batteryPower} timeB={timeB} timeC={timeC}")
            return {"voltage": voltage, "status": status, "batteryPower": batteryPower, "timeB": timeB, "timeC": timeC}
        else:
            print("[volt] decrypted hex too short for volt parse:", hexstr)
            return None
    except Exception as e:
        print("[volt] parse error:", e)
        traceback.print_exc()
        return None

def route_decrypted_hex(hexstr: str):
    if hexstr.startswith("fefefe"):
        return parse_charge(hexstr)
    if "fffefe" in hexstr:
        return parse_crank(hexstr)
    if len(hexstr) >= 16:
        return parse_volt_message(hexstr)
    print("[router] no parser matched; hex:", hexstr)
    return None

# ========== BLE client with robust notify wrapper ==========
class BM2BleClient:
    def __init__(self, address=None, name=None):
        self.address = address
        self.name = name
        self.client = None
        self._notif_cb = None

    async def find_address_by_name(self, timeout=5.0):
        devs = await BleakScanner.discover(timeout=timeout)
        for d in devs:
            if d.name and self.name and self.name in d.name:
                print("Found device by name:", d.name, d.address)
                return d.address
        return None

    async def connect(self):
        addr = self.address
        if not addr and self.name:
            addr = await self.find_address_by_name()
            if not addr:
                raise RuntimeError("Device name not found")
        if not addr:
            raise RuntimeError("No device address provided")
        self.client = BleakClient(addr)
        await self.client.connect()
        # Note: is_connected is a boolean property on BleakClient; do not call it.
        try:
            print("Connected:", self.client.is_connected)
        except Exception:
            # defensive fallback
            print("Connected (status unknown)")

        # synchronous wrapper that schedules the async handler, stored so stop_notify can use same object
        def wrapper(sender, data):
            try:
                asyncio.create_task(self.handle_notify(sender, data))
            except Exception as e:
                print("Error scheduling notification handler:", e)
                traceback.print_exc()

        self._notif_cb = wrapper
        await self.client.start_notify(CHAR_NOTIFY_UUID, self._notif_cb)
        print("Notifications enabled on", CHAR_NOTIFY_UUID)

    async def disconnect(self):
        if not self.client:
            print("No client to disconnect.")
            return
        try:
            if self._notif_cb is not None:
                # stop_notify may raise if not registered; capture exceptions
                try:
                    await self.client.stop_notify(CHAR_NOTIFY_UUID)
                except KeyError as ke:
                    # Bleak backend may not find token; ignore at disconnect
                    print("Warning: stop_notify KeyError (callback token not found) - ignoring.", ke)
                except Exception as e:
                    print("Warning: exception stopping notify:", e)
                    traceback.print_exc()
        finally:
            try:
                await self.client.disconnect()
                print("Disconnected")
            except Exception as e:
                print("Disconnect error (ignored):", e)
                traceback.print_exc()

    async def handle_notify(self, sender, data: bytearray):
        try:
            dec = decrypt_blocks(bytes(data))
            hexstr = hex_str_from_bytes(dec)
            print("DECRYPTED HEX:", hexstr)
            route_decrypted_hex(hexstr)
        except Exception as e:
            print("notify handler error:", e)
            traceback.print_exc()

    async def send_raw_signed(self, signed_bytes_list):
        if isinstance(signed_bytes_list, bytes):
            raw = signed_bytes_list
        else:
            raw = int_list_to_signed_bytes(signed_bytes_list)
        enc = encrypt_cmd(raw)
        print("Sending encrypted:", hexlify(enc).decode())
        await self.client.write_gatt_char(CHAR_WRITE_UUID, enc, response=True)

    async def send_P(self, mode_index: int, voltage_float: float):
        base = [-23, 2]
        mapping = {0:10,1:9,2:8,3:7,4:6,5:5,6:4,7:3,8:2,9:1,10:0}
        mode_byte = mapping.get(mode_index, 0)
        base.append(mode_byte)
        val = int(round(voltage_float * 100))
        if val == 0:
            vb = [0]
        else:
            tmp = []
            v = val
            while v > 0:
                tmp.append(v & 0xFF)
                v >>= 8
            vb = list(reversed(tmp))
        payload = base + vb
        await self.send_raw_signed(payload)

    async def send_z(self, i3: int, i4: int):
        b0 = -24
        b1 = 2
        if i3 == 2:
            b2, b3 = 2, 0
        elif i3 == 3:
            b2 = 3
            if i4 == 0:
                b3 = 0
            elif i4 == 1:
                b3 = 1
            elif i4 == 2:
                b3 = 2
            else:
                b3 = 0
        else:
            b2, b3 = 1, 0
        await self.send_raw_signed([b0, b1, b2, b3])

# ========== Main ==========
async def main():
    client = BM2BleClient(address=DEVICE_ADDRESS, name=DEVICE_NAME)
    try:
        await client.connect()
        print("Client connected and listening. Press Ctrl+C to exit.")
        while True:
            await asyncio.sleep(1.0)
    except KeyboardInterrupt:
        print("Keyboard interrupt, disconnecting.")
    except Exception as e:
        print("Error:", e)
        traceback.print_exc()
    finally:
        await client.disconnect()

if __name__ == "__main__":
    asyncio.run(main())
