import subprocess
import os
import shutil

def find_mkisofs():
    known_paths = [
        r"C:\Program Files (x86)\cdrtfe\tools\cdrtools\mkisofs.exe",
        r"C:\Program Files\cdrtfe\tools\cdrtools\mkisofs.exe",
    ]
    for p in known_paths:
        if os.path.exists(p):
            return p

    p = shutil.which("mkisofs")
    if p:
        return p

    p = shutil.which("xorrisofs")
    if p:
        return p

    return known_paths[0]

def make_bootable_iso(boot_bin, kernel_bin, iso_path):
    project_dir = os.path.dirname(os.path.abspath(iso_path))
    iso_root = os.path.join(project_dir, "iso_root")
    if os.path.exists(iso_root):
        shutil.rmtree(iso_root)
    os.makedirs(iso_root, exist_ok=True)

    # 1. Read bootloader and kernel
    with open(boot_bin, 'rb') as f:
        boot_data = f.read() # 512 bytes
    with open(kernel_bin, 'rb') as f:
        kernel_data = f.read() # ~493 KB

    # 2. Create the boot payload: boot.bin (512 bytes) + kernel.bin
    # The El Torito BIOS loads this 512 KB payload to 0x7C00.
    payload = bytearray(boot_data)
    payload.extend(kernel_data)
    target_payload_len = 524288 # 1024 sectors (512 KB)
    if len(payload) < target_payload_len:
        payload.extend(b'\x00' * (target_payload_len - len(payload)))

    payload_file = os.path.join(iso_root, "boot.bin")
    with open(payload_file, "wb") as f:
        f.write(payload)

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

    # 4. Run mkisofs with El Torito No-Emulation (-no-emul-boot)
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
        "-b", "boot.bin",        # Boot image
        "-no-emul-boot",         # El Torito No Emulation
        "-boot-load-size", "1024",# Load 1024 sectors (512 KB)
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
        # boot.bin is at boot_iso_lba * 4
        # kernel.bin starts at boot_iso_lba * 4 + 1
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

        # Create Active MBR Partition 1 covering the entire ISO image
        part1 = bytearray(16)
        part1[0] = 0x80 # Bootable / Active
        part1[1] = 0x00 # Head
        part1[2] = 0x01 # Sector 1, Cylinder 0
        part1[3] = 0x00 # Cylinder 0
        part1[4] = 0x17 # Partition Type: Hidden ISO/HPFS/NTFS (Standard ISOHybrid type)
        part1[5] = 0xFE # End Head
        part1[6] = 0xFF # End Sector
        part1[7] = 0xFF # End Cylinder
        part1[8:12] = (0).to_bytes(4, "little") # Starting LBA = 0
        part1[12:16] = total_sectors.to_bytes(4, "little") # Total sectors

        mbr[446:462] = part1
        mbr[462:478] = b"\x00" * 16 # Partition 2
        mbr[478:494] = b"\x00" * 16 # Partition 3
        mbr[494:510] = b"\x00" * 16 # Partition 4
        mbr[510:512] = b"\x55\xaa"  # MBR Boot Signature

        # Write Hybrid MBR directly to Sector 0 of the ISO
        f.seek(0)
        f.write(mbr)
        f.flush()

    print(f"[+] Successfully built ISOHybrid for Rufus & Real Devices: {iso_path} ({os.path.getsize(iso_path)} bytes)")

if __name__ == '__main__':
    project_dir = os.path.dirname(os.path.abspath(__file__))
    boot_bin = os.path.join(project_dir, "bin", "boot.bin")
    kernel_bin = os.path.join(project_dir, "bin", "kernel.bin")
    iso_path = os.path.join(project_dir, "auraos.iso")
    make_bootable_iso(boot_bin, kernel_bin, iso_path)
