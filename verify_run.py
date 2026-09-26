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

import zlib

def bmp_to_png(bmp_path, png_path):
    with open(bmp_path, 'rb') as f:
        data = f.read()
    pixel_offset = struct.unpack('<I', data[10:14])[0]
    width = struct.unpack('<i', data[18:22])[0]
    height = struct.unpack('<i', data[22:26])[0]
    row_bytes = width * 3
    pad = (4 - (row_bytes % 4)) % 4
    
    raw_scanlines = bytearray()
    for y in range(height):
        bmp_y = height - 1 - y
        row_start = pixel_offset + bmp_y * (row_bytes + pad)
        row = data[row_start : row_start + row_bytes]
        raw_scanlines.append(0)
        for x in range(width):
            b = row[x*3]
            g = row[x*3+1]
            r = row[x*3+2]
            raw_scanlines.extend((r, g, b))
            
    png = bytearray(b'\x89PNG\r\n\x1a\n')
    ihdr_data = struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)
    ihdr_crc = zlib.crc32(b'IHDR' + ihdr_data)
    png.extend(struct.pack('>I', len(ihdr_data)))
    png.extend(b'IHDR')
    png.extend(ihdr_data)
    png.extend(struct.pack('>I', ihdr_crc))
    
    compressed = zlib.compress(bytes(raw_scanlines), 9)
    idat_crc = zlib.crc32(b'IDAT' + compressed)
    png.extend(struct.pack('>I', len(compressed)))
    png.extend(b'IDAT')
    png.extend(compressed)
    png.extend(struct.pack('>I', idat_crc))
    
    iend_crc = zlib.crc32(b'IEND')
    png.extend(struct.pack('>I', 0))
    png.extend(b'IEND')
    png.extend(struct.pack('>I', iend_crc))
    
    with open(png_path, 'wb') as f:
        f.write(png)
    print(f"[+] Saved screenshot PNG: {png_path} ({width}x{height})")
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
        png_path = os.path.join(PROJECT_ROOT, "screenshot.png")
        bmp_to_png(BMP_PATH, png_path)
        print("[+] SUCCESS: AuraOS booted and screenshot captured!")
    else:
        print("[!] Error: No screendump produced!")

if __name__ == '__main__':
    main()
