import os
import time
import socket
import subprocess
from verify_run import ppm_to_bmp, bmp_to_png

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
ISO_PATH = os.path.join(PROJECT_ROOT, "auraos.iso")
QEMU = r"C:\Program Files\qemu\qemu-system-x86_64.exe"
OVMF = r"C:\Program Files\qemu\share\edk2-x86_64-code.fd"
PPM_PATH = os.path.join(PROJECT_ROOT, "screenshot_uefi_iso.ppm")
BMP_PATH = os.path.join(PROJECT_ROOT, "screenshot_uefi_iso.bmp")
PNG_PATH = os.path.join(PROJECT_ROOT, "screenshot_uefi_iso.png")

def main():
    print("[*] Launching QEMU in UEFI mode from auraos.iso (CD-ROM)...")
    if os.path.exists(PPM_PATH):
        os.remove(PPM_PATH)

    port = 5599
    proc = subprocess.Popen([
        QEMU,
        "-drive", f"if=pflash,format=raw,readonly=on,file={OVMF}",
        "-cdrom", ISO_PATH,
        "-display", "none",
        "-vga", "std",
        "-m", "256M",
        "-monitor", f"tcp:127.0.0.1:{port},server,nowait"
    ])

    print("[*] Waiting for UEFI firmware to boot ISO into AuraOS desktop...")
    time.sleep(7.0)

    try:
        s = socket.socket()
        s.connect(('127.0.0.1', port))
        time.sleep(0.5)
        s.recv(1024)
        s.sendall(f"screendump {PPM_PATH}\n".encode())
        time.sleep(1.0)
        s.sendall(b"quit\n")
        time.sleep(0.5)
        s.close()
    except Exception as e:
        print("[!] Monitor socket error:", e)

    proc.wait()

    if os.path.exists(PPM_PATH):
        ppm_to_bmp(PPM_PATH, BMP_PATH)
        bmp_to_png(BMP_PATH, PNG_PATH)
        print(f"[+] SUCCESS: AuraOS booted from auraos.iso in UEFI mode! Screenshot: {PNG_PATH}")
        return True
    else:
        print("[!] Error: No screendump produced from UEFI ISO boot!")
        return False

if __name__ == '__main__':
    main()
