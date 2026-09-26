import os
import struct
import subprocess
import time
import socket

def make_no_emulation_iso(boot_bin, kernel_bin, iso_path):
    SECTOR_SIZE = 2048

    # Read bootloader and kernel
    with open(boot_bin, 'rb') as f:
        boot_data = f.read() # 512 bytes
    with open(kernel_bin, 'rb') as f:
        kernel_data = f.read() # ~28 KB

    # Create combined contiguous boot payload: boot.bin (512 bytes) + kernel.bin
    payload = bytearray(boot_data)
    payload.extend(kernel_data)
    # Pad payload to multiple of 2048
    if len(payload) % SECTOR_SIZE != 0:
        payload.extend(b'\x00' * (SECTOR_SIZE - (len(payload) % SECTOR_SIZE)))

    sectors_to_load = 64 # 64 virtual 512-byte sectors = 32 KB

    # Build ISO
    iso = bytearray(16 * SECTOR_SIZE) # 0..15 System Area

    # Sector 16: PVD
    pvd = bytearray(SECTOR_SIZE)
    pvd[0] = 0x01
    pvd[1:6] = b"CD001"
    pvd[6] = 0x01
    pvd[40:72] = b"AURA_OS".ljust(32, b' ')
    struct.pack_into('<H', pvd, 128, 2048)
    struct.pack_into('>H', pvd, 130, 2048)
    iso.extend(pvd)

    # Sector 17: BRVD
    brvd = bytearray(SECTOR_SIZE)
    brvd[0] = 0x00
    brvd[1:6] = b"CD001"
    brvd[6] = 0x01
    brvd[7:39] = b"EL TORITO SPECIFICATION".ljust(32, b'\x00')
    struct.pack_into('<I', brvd, 71, 19) # Catalog at sector 19
    iso.extend(brvd)

    # Sector 18: Terminator
    vdt = bytearray(SECTOR_SIZE)
    vdt[0] = 0xFF
    vdt[1:6] = b"CD001"
    vdt[6] = 0x01
    iso.extend(vdt)

    # Sector 19: Catalog
    cat = bytearray(SECTOR_SIZE)
    # Validation Entry
    cat[0] = 0x01
    cat[1] = 0x00
    cat[4:28] = b"AURA_OS".ljust(24, b'\x00')
    cat[30] = 0x55
    cat[31] = 0xAA
    words_sum = sum(struct.unpack('<16H', cat[:32])) & 0xFFFF
    checksum = (-words_sum) & 0xFFFF
    struct.pack_into('<H', cat, 28, checksum)

    # Initial Entry
    cat[32] = 0x88 # Bootable
    cat[33] = 0x00 # No Emulation
    struct.pack_into('<H', cat, 34, 0x07C0) # Load Segment 0x7C0 -> 0x7C00
    struct.pack_into('<H', cat, 38, sectors_to_load) # Load 64 sectors (32 KB)
    struct.pack_into('<I', cat, 40, 20)     # LBA 20
    iso.extend(cat)

    # Sector 20+: Payload
    iso.extend(payload)

    # Total sectors in PVD
    total = len(iso) // SECTOR_SIZE
    struct.pack_into('<I', iso, 16 * SECTOR_SIZE + 80, total)
    struct.pack_into('>I', iso, 16 * SECTOR_SIZE + 84, total)

    with open(iso_path, 'wb') as f:
        f.write(iso)

    print(f"[+] Built No-Emulation ISO: {iso_path} ({len(iso)} bytes, {total} sectors)")

if __name__ == '__main__':
    project_dir = r"C:\Users\runneradmin\.gemini\antigravity\scratch\AuraOS"
    boot_bin = os.path.join(project_dir, "bin", "boot.bin")
    kernel_bin = os.path.join(project_dir, "bin", "kernel.bin")
    iso_path = os.path.join(project_dir, "auraos.iso")
    make_no_emulation_iso(boot_bin, kernel_bin, iso_path)
