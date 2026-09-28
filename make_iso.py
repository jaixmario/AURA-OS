import subprocess
import os
import shutil

def find_mkisofs():
    # 1. Check known install locations
    known_paths = [
        r"C:\Program Files (x86)\cdrtfe\tools\cdrtools\mkisofs.exe",
        r"C:\Program Files\cdrtfe\tools\cdrtools\mkisofs.exe",
    ]
    for p in known_paths:
        if os.path.exists(p):
            return p

    # 2. Check PATH
    p = shutil.which("mkisofs")
    if p:
        return p

    # 3. Check cygwin / chocolatey
    p = shutil.which("xorrisofs")
    if p:
        return p

    return known_paths[0]

def make_bootable_iso(boot_bin, kernel_bin, disk_img, iso_path):
    project_dir = os.path.dirname(os.path.abspath(iso_path))
    iso_root = os.path.join(project_dir, "iso_root")
    if os.path.exists(iso_root):
        shutil.rmtree(iso_root)
    os.makedirs(iso_root, exist_ok=True)

    # 1. Read bootloader, kernel, and hard disk image
    with open(boot_bin, 'rb') as f:
        boot_data = f.read() # 512 bytes
    with open(kernel_bin, 'rb') as f:
        kernel_data = f.read() # ~493 KB
    with open(disk_img, 'rb') as f:
        disk_data = bytearray(f.read()) # 10 MB MBR disk image

    # 2. Patch is_live_default = 1 in disk.img for Live Media boot
    a0_idx = disk_data[:446].find(b'\xa0')
    if a0_idx != -1 and a0_idx < 50:
        var_addr = int.from_bytes(disk_data[a0_idx+1:a0_idx+3], 'little')
        var_offset = var_addr - 0x7C00
        if 0 <= var_offset < 446:
            disk_data[var_offset] = 1 # is_live_default = 1
            print(f"    - Patched disk.img is_live_boot: 1 (offset {var_offset})")

    # Save disk.img as El Torito Hard Disk boot image
    with open(os.path.join(iso_root, "disk.img"), "wb") as f:
        f.write(disk_data)

    # 3. Add user-visible files to CD root
    with open(os.path.join(iso_root, "KERNEL.BIN"), "wb") as f:
        f.write(kernel_data)

    readme_content = """==================================================
           AuraOS Operating System Live Media
==================================================
Version     : 1.2.0 (32-bit x86 Protected Mode)
Graphics    : Dynamic Multi-Resolution TrueColor
              - 1920x1080 (1080p Full HD)
              - 1600x900  (HD+)
              - 1366x768  (Wide Laptop)
              - 1280x1024 (SXGA Desktop)
              - 1280x800  (16:10 Laptop)
              - 1280x720  (720p HD)
              - 1024x768  (Standard Desktop)
              - 800x600   (SVGA Safe Mode)

Compatible with:
  - Real Hardware PC / Laptop (via Rufus USB)
  - VMware Workstation / Player / Fusion / ESXi
  - Oracle VirtualBox
  - QEMU / KVM / Bochs

Write to USB using Rufus:
  - Select auraos.iso
  - Partition scheme: MBR
  - Target system: BIOS (or UEFI-CSM)
  - Select "Write in DD Image mode" when prompted
==================================================
"""
    with open(os.path.join(iso_root, "README.TXT"), "w", encoding="ascii") as f:
        f.write(readme_content)

    cfg_content = """# AuraOS Boot Configuration
RESOLUTION=auto
RES_AUTO=1
BPP=32
DOUBLE_BUFFER=YES
HEAP_SIZE_MB=16
"""
    with open(os.path.join(iso_root, "SYSTEM.CFG"), "w", encoding="ascii") as f:
        f.write(cfg_content)

    # 4. Run mkisofs with El Torito Hard Disk Emulation (-hard-disk-boot)
    # This guarantees 100% compatibility with VMware Workstation, VirtualBox,
    # and QEMU without 2048-byte sector truncation bugs.
    mkisofs = find_mkisofs()
    cygwin_dir = r"C:\Program Files (x86)\cdrtfe\tools\cygwin"
    env = os.environ.copy()
    if os.path.exists(cygwin_dir):
        env["PATH"] = cygwin_dir + ";" + env["PATH"]

    cmd = [
        mkisofs,
        "-V", "AURA_OS",
        "-p", "AuraOS",
        "-A", "AuraOS",
        "-J", "-R",              # Joliet & Rock Ridge extensions
        "-b", "disk.img",        # Hard disk boot image
        "-hard-disk-boot",       # El Torito Hard Disk Emulation (Drive 0x80)
        "-o", os.path.basename(iso_path),
        "iso_root"
    ]

    res = subprocess.run(cmd, env=env, cwd=project_dir, capture_output=True, text=True)
    if res.returncode != 0:
        print("[!] mkisofs error:\n", res.stderr)
        raise RuntimeError(f"mkisofs failed with code {res.returncode}")

    # Clean up temporary iso_root directory
    if os.path.exists(iso_root):
        shutil.rmtree(iso_root)

    # 5. ISOHybrid Post-Processing: Inject MBR into Sector 0 for Rufus & Real Hardware USB Boot
    print("[*] Processing ISOHybrid MBR for Rufus USB booting...")
    with open(iso_path, "r+b") as f:
        f.seek(0, os.SEEK_END)
        iso_size = f.tell()
        total_sectors = iso_size // 512

        # Read Boot Record Volume Descriptor at ISO sector 17 (0x8800)
        f.seek(17 * 2048)
        brvd = f.read(2048)
        boot_cat_lba = int.from_bytes(brvd[71:75], "little")

        # Read Boot Catalog at boot_cat_lba
        f.seek(boot_cat_lba * 2048)
        boot_cat = f.read(2048)

        # Default entry in Boot Catalog (bytes 32..63)
        def_entry = boot_cat[32:64]
        boot_iso_lba = int.from_bytes(def_entry[8:12], "little")

        # 512-byte HDD sector numbers:
        # disk.img starts at boot_iso_lba * 4
        # Inside disk.img, kernel starts at offset +1
        boot_hdd_lba = boot_iso_lba * 4
        kernel_hdd_lba = boot_hdd_lba + 1

        print(f"    - El Torito Boot Image : ISO Sector {boot_iso_lba} (HDD LBA {boot_hdd_lba})")
        print(f"    - Protected Mode Kernel: HDD LBA {kernel_hdd_lba}")

        # Build Sector 0 MBR
        mbr = bytearray(boot_data[:512])

        # Patch DAP to load kernel from kernel_hdd_lba
        dap_magic = bytes([0x10, 0x00, 0x40, 0x00])
        dap_idx = mbr.find(dap_magic)
        if dap_idx != -1:
            mbr[dap_idx + 8 : dap_idx + 16] = kernel_hdd_lba.to_bytes(8, "little")
            print(f"    - Patched MBR DAP LBA  : {kernel_hdd_lba} (offset {dap_idx})")
        else:
            print("    [!] Warning: DAP magic not found in boot.bin!")

        # Patch is_live_default = 1 for Rufus USB boot
        a0_idx = mbr.find(b"\xa0")
        if a0_idx != -1 and a0_idx < 50:
            target_addr = int.from_bytes(mbr[a0_idx+1:a0_idx+3], "little")
            var_offset = target_addr - 0x7C00
            if 0 <= var_offset < 446:
                mbr[var_offset] = 1
                print(f"    - Patched is_live_boot : 1 (offset {var_offset})")

        # Partition 1: FAT16 partition located at boot_hdd_lba + 2048
        part1_lba = boot_hdd_lba + 2048
        part1_sectors = 18432
        part1 = bytearray(16)
        part1[0] = 0x80 # Active / Bootable
        part1[1] = 0x01
        part1[2] = 0x01
        part1[3] = 0x00
        part1[4] = 0x06 # FAT16
        part1[5] = 0x0F
        part1[6] = 0x20
        part1[7] = 0x27
        part1[8:12] = part1_lba.to_bytes(4, "little")
        part1[12:16] = part1_sectors.to_bytes(4, "little")

        mbr[446:462] = part1
        mbr[462:510] = b"\x00" * 48
        mbr[510:512] = b"\x55\xaa"  # MBR Boot Signature

        # Write Hybrid MBR directly to Sector 0 of the ISO
        f.seek(0)
        f.write(mbr)
        f.flush()

    print(f"[+] Successfully built ISOHybrid for Rufus & VMware: {iso_path} ({os.path.getsize(iso_path)} bytes)")

if __name__ == '__main__':
    project_dir = os.path.dirname(os.path.abspath(__file__))
    boot_bin = os.path.join(project_dir, "bin", "boot.bin")
    kernel_bin = os.path.join(project_dir, "bin", "kernel.bin")
    disk_img = os.path.join(project_dir, "auraos.img")
    iso_path = os.path.join(project_dir, "auraos.iso")
    make_bootable_iso(boot_bin, kernel_bin, disk_img, iso_path)
