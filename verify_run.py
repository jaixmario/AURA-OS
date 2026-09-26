import os
import time
import socket
import struct
import subprocess

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
DISK_IMG = os.path.join(PROJECT_ROOT, "auraos.img")
QEMU = r"C:\Program Files\qemu\qemu-system-x86_64.exe"
PPM_PATH = os.path.join(PROJECT_ROOT, "screenshot.ppm")
BMP_PATH = os.path.join(PROJECT_ROOT, "screenshot.bmp")

def ppm_to_bmp(ppm_path, bmp_path):
    with open(ppm_path, 'rb') as f:
        header = f.readline().strip()
        if header != b'P6':
            print("Not P6 PPM:", header)
            return False
        dims = f.readline().strip()
        while dims.startswith(b'#'):
            dims = f.readline().strip()
        width, height = map(int, dims.split())
        max_val = int(f.readline().strip())
        raw_data = f.read()

    row_bytes = width * 3
    pad_bytes = (4 - (row_bytes % 4)) % 4
    bmp_size = 54 + (row_bytes + pad_bytes) * height

    bmp = bytearray()
    bmp.extend(b'BM')
    bmp.extend(struct.pack('<I', bmp_size))
    bmp.extend(b'\x00\x00\x00\x00')
    bmp.extend(struct.pack('<I', 54))
    bmp.extend(struct.pack('<I', 40))
    bmp.extend(struct.pack('<i', width))
    bmp.extend(struct.pack('<i', height))
    bmp.extend(struct.pack('<H', 1))
    bmp.extend(struct.pack('<H', 24))
    bmp.extend(struct.pack('<I', 0))
    bmp.extend(struct.pack('<I', (row_bytes + pad_bytes) * height))
    bmp.extend(struct.pack('<i', 2835))
    bmp.extend(struct.pack('<i', 2835))
    bmp.extend(struct.pack('<I', 0))
    bmp.extend(struct.pack('<I', 0))

    for y in range(height - 1, -1, -1):
        row_offset = y * width * 3
        row = raw_data[row_offset:row_offset + width * 3]
        for x in range(width):
            r = row[x * 3]
            g = row[x * 3 + 1]
            b = row[x * 3 + 2]
            bmp.extend(bytes([b, g, r]))
        bmp.extend(b'\x00' * pad_bytes)

    with open(bmp_path, 'wb') as f:
        f.write(bmp)
    print(f"[+] Saved screenshot BMP: {bmp_path} ({width}x{height})")
    return True

def main():
    print("[*] Launching QEMU to capture screenshot of AuraOS...")
    port = 5590
    proc = subprocess.Popen([
        QEMU,
        "-drive", f"format=raw,file={DISK_IMG}",
        "-display", "none",
        "-vga", "std",
        "-m", "256M",
        "-monitor", f"tcp:127.0.0.1:{port},server,nowait"
    ])

    time.sleep(2.5)

    try:
        s = socket.socket()
        s.connect(('127.0.0.1', port))
        time.sleep(0.5)
        s.recv(1024)

        cmd = f"screendump {PPM_PATH}\n"
        s.sendall(cmd.encode())
        time.sleep(1.0)

        s.sendall(b"quit\n")
        time.sleep(0.5)
        s.close()
    except Exception as e:
        print("[!] Monitor socket error:", e)

    proc.wait()

    if os.path.exists(PPM_PATH):
        ppm_to_bmp(PPM_PATH, BMP_PATH)
        print("[+] SUCCESS: AuraOS booted and screenshot captured!")
    else:
        print("[!] Error: No screendump produced!")

if __name__ == '__main__':
    main()
