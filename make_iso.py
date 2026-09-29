import subprocess
import os
import shutil
import struct

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

def create_efi_fat16_image(bootx64_bytes, bootia32_bytes=None, total_sectors=5760):
    """
    Creates a clean 2.88 MB FAT16 disk image containing /EFI/BOOT/BOOTX64.EFI
    and /EFI/BOOT/BOOTIA32.EFI for UEFI El Torito boot.
    """
    BYTES_PER_SECTOR = 512
    SECTORS_PER_CLUSTER = 1
    CLUSTER_SIZE = 512
    RESERVED_SECTORS = 1
    NUM_FATS = 2
    ROOT_ENTRIES = 512
    ROOT_DIR_SECTORS = (ROOT_ENTRIES * 32) // BYTES_PER_SECTOR # 32 sectors
    SECTORS_PER_FAT = 24

    img = bytearray(total_sectors * BYTES_PER_SECTOR)

    # 1. Volume Boot Record
    vbr = bytearray(BYTES_PER_SECTOR)
    vbr[0:3] = b'\xEB\x3C\x90'
    vbr[3:11] = b'MSWIN4.1'
    struct.pack_into('<H', vbr, 11, BYTES_PER_SECTOR)
    vbr[13] = SECTORS_PER_CLUSTER
    struct.pack_into('<H', vbr, 14, RESERVED_SECTORS)
    vbr[16] = NUM_FATS
    struct.pack_into('<H', vbr, 17, ROOT_ENTRIES)
    struct.pack_into('<H', vbr, 19, total_sectors)
    vbr[21] = 0xF8 # Media descriptor
    struct.pack_into('<H', vbr, 22, SECTORS_PER_FAT)
    struct.pack_into('<H', vbr, 24, 32)
    struct.pack_into('<H', vbr, 26, 16)
    struct.pack_into('<I', vbr, 28, 0)
    struct.pack_into('<I', vbr, 32, 0)
    vbr[36] = 0x80
    vbr[38] = 0x29
    struct.pack_into('<I', vbr, 39, 0x12345678)
    vbr[43:54] = b'EFIBOOT    '
    vbr[54:62] = b'FAT16   '
    vbr[510] = 0x55
    vbr[511] = 0xAA
    img[0:512] = vbr

    fat = bytearray(SECTORS_PER_FAT * BYTES_PER_SECTOR)
    struct.pack_into('<H', fat, 0, 0xFFF8)
    struct.pack_into('<H', fat, 2, 0xFFFF)

    def make_entry(name8_3, attr, cluster, size):
        e = bytearray(32)
        e[0:11] = name8_3.encode('ascii')
        e[11] = attr
        struct.pack_into('<H', e, 26, cluster)
        struct.pack_into('<I', e, 28, size)
        return e

    cluster_efi = 2
    struct.pack_into('<H', fat, cluster_efi * 2, 0xFFFF)

    cluster_boot = 3
    struct.pack_into('<H', fat, cluster_boot * 2, 0xFFFF)

    curr = 4
    x64_start = curr
    x64_clusters = (len(bootx64_bytes) + CLUSTER_SIZE - 1) // CLUSTER_SIZE
    for i in range(x64_clusters):
        nxt = 0xFFFF if (i == x64_clusters - 1) else (curr + 1)
        struct.pack_into('<H', fat, curr * 2, nxt)
        curr += 1

    ia32_start = curr
    if bootia32_bytes:
        ia32_clusters = (len(bootia32_bytes) + CLUSTER_SIZE - 1) // CLUSTER_SIZE
        for i in range(ia32_clusters):
            nxt = 0xFFFF if (i == ia32_clusters - 1) else (curr + 1)
            struct.pack_into('<H', fat, curr * 2, nxt)
            curr += 1
    else:
        ia32_start = 0

    fat1_offset = RESERVED_SECTORS * BYTES_PER_SECTOR
    fat2_offset = (RESERVED_SECTORS + SECTORS_PER_FAT) * BYTES_PER_SECTOR
    img[fat1_offset:fat1_offset + len(fat)] = fat
    img[fat2_offset:fat2_offset + len(fat)] = fat

    # Root Directory
    root_offset = (RESERVED_SECTORS + NUM_FATS * SECTORS_PER_FAT) * BYTES_PER_SECTOR
    img[root_offset:root_offset + 32] = make_entry('EFIBOOT    ', 0x08, 0, 0)
    img[root_offset + 32:root_offset + 64] = make_entry('EFI        ', 0x10, cluster_efi, 0)

    data_offset = root_offset + (ROOT_DIR_SECTORS * BYTES_PER_SECTOR)

    def write_cluster(cluster_num, data):
        off = data_offset + (cluster_num - 2) * CLUSTER_SIZE
        img[off:off + len(data)] = data

    efi_dir = bytearray(CLUSTER_SIZE)
    efi_dir[0:32] = make_entry('.          ', 0x10, cluster_efi, 0)
    efi_dir[32:64] = make_entry('..         ', 0x10, 0, 0)
    efi_dir[64:96] = make_entry('BOOT       ', 0x10, cluster_boot, 0)
    write_cluster(cluster_efi, efi_dir)

    boot_dir = bytearray(CLUSTER_SIZE)
    boot_dir[0:32] = make_entry('.          ', 0x10, cluster_boot, 0)
    boot_dir[32:64] = make_entry('..         ', 0x10, cluster_efi, 0)
    boot_dir[64:96] = make_entry('BOOTX64 EFI', 0x20, x64_start, len(bootx64_bytes))
    if bootia32_bytes:
        boot_dir[96:128] = make_entry('BOOTIA32EFI', 0x20, ia32_start, len(bootia32_bytes))
    write_cluster(cluster_boot, boot_dir)

    for i in range(x64_clusters):
        chunk = bootx64_bytes[i*CLUSTER_SIZE:(i+1)*CLUSTER_SIZE]
        write_cluster(x64_start + i, chunk)

    if bootia32_bytes:
        for i in range(ia32_clusters):
            chunk = bootia32_bytes[i*CLUSTER_SIZE:(i+1)*CLUSTER_SIZE]
            write_cluster(ia32_start + i, chunk)

    return img

def make_bootable_iso(boot_bin, kernel_bin, iso_path, bootx64_efi=None, bootia32_efi=None):
    project_dir = os.path.dirname(os.path.abspath(iso_path))
    iso_root = os.path.join(project_dir, "iso_root")
    if os.path.exists(iso_root):
        shutil.rmtree(iso_root)
    os.makedirs(iso_root, exist_ok=True)

    # 1. Read BIOS bootloader and kernel
    with open(boot_bin, 'rb') as f:
        boot_data = f.read() # 512 bytes
    with open(kernel_bin, 'rb') as f:
        kernel_data = f.read() # ~496 KB

    # 2. Create the BIOS boot payload: boot.bin (512 bytes) + kernel.bin
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

    # 4. Prepare UEFI binaries and FAT image
    has_uefi = False
    efiboot_img_bytes = None
    if bootx64_efi and os.path.exists(bootx64_efi):
        has_uefi = True
        with open(bootx64_efi, "rb") as f:
            x64_bytes = f.read()
        ia32_bytes = None
        if bootia32_efi and os.path.exists(bootia32_efi):
            with open(bootia32_efi, "rb") as f:
                ia32_bytes = f.read()

        # Copy EFI binaries to ISO9660 root directory structure (/EFI/BOOT/) for Ventoy & UEFI
        efi_boot_dir = os.path.join(iso_root, "EFI", "BOOT")
        os.makedirs(efi_boot_dir, exist_ok=True)
        with open(os.path.join(efi_boot_dir, "BOOTX64.EFI"), "wb") as f:
            f.write(x64_bytes)
        if ia32_bytes:
            with open(os.path.join(efi_boot_dir, "BOOTIA32.EFI"), "wb") as f:
                f.write(ia32_bytes)

        # Generate efiboot.img (2.88 MB FAT16 partition image)
        efiboot_img_bytes = create_efi_fat16_image(x64_bytes, ia32_bytes)
        efiboot_path = os.path.join(iso_root, "efiboot.img")
        with open(efiboot_path, "wb") as f:
            f.write(efiboot_img_bytes)

    readme_content = """==================================================
           AuraOS Operating System Live Media
==================================================
Version     : 1.2.0 (32-bit x86 Protected Mode)
Graphics    : Dynamic Multi-Resolution TrueColor (GOP / VBE 2.0+)
              - 1920x1080 (1080p Full HD)
              - 1600x900  (HD+)
              - 1366x768  (Wide Laptop)
              - 1280x1024 (SXGA Desktop)
              - 1280x800  (16:10 Laptop)
              - 1280x720  (720p HD)
              - 1024x768  (Standard Desktop)
              - 800x600   (SVGA Safe Mode)

Compatible with:
  - Ventoy USB Boot (UEFI x86_64, UEFI IA32, and Legacy BIOS)
  - Rufus USB Boot (UEFI and BIOS / DD Mode)
  - Real Hardware PC / Laptop (UEFI and CSM/Legacy)
  - VMware Workstation / Player / Fusion / ESXi (UEFI & BIOS)
  - Oracle VirtualBox (UEFI & BIOS)
  - QEMU / KVM / Bochs (OVMF UEFI & SeaBIOS)

Boot Modes Supported:
  1. UEFI 64-bit (BOOTX64.EFI)
  2. UEFI 32-bit (BOOTIA32.EFI)
  3. Legacy BIOS / MBR / El Torito
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

    # 5. Run mkisofs with Dual El Torito (BIOS + UEFI)
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
        "-b", "boot.bin",        # BIOS Boot image
        "-no-emul-boot",         # El Torito No Emulation
        "-boot-load-size", "1024",# Load 1024 sectors (512 KB)
    ]

    if has_uefi:
        cmd.extend([
            "-eltorito-alt-boot",
            "-eltorito-platform", "efi",
            "-b", "efiboot.img",
            "-no-emul-boot",
        ])

    cmd.extend([
        "-o", os.path.basename(iso_path),
        "iso_root"
    ])

    res = subprocess.run(cmd, env=env, cwd=project_dir, capture_output=True, text=True)
    if res.returncode != 0:
        print("[!] mkisofs error:\n", res.stderr)
        raise RuntimeError(f"mkisofs failed with code {res.returncode}")

    # Clean up temporary iso_root directory
    if os.path.exists(iso_root):
        shutil.rmtree(iso_root)

    # 6. ISOHybrid Post-Processing: Inject MBR into Sector 0 for Ventoy, Rufus, & Bare-Metal PCs
    print("[*] Processing Universal ISOHybrid MBR (BIOS + UEFI + Ventoy)...")
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

        # Default entry in Boot Catalog (bytes 32..63) -> BIOS boot.bin
        def_entry = boot_cat[32:64]
        boot_iso_lba = int.from_bytes(def_entry[8:12], "little")
        boot_hdd_lba = boot_iso_lba * 4
        kernel_hdd_lba = boot_hdd_lba + 1

        print(f"    - El Torito BIOS Boot Image : ISO Sector {boot_iso_lba} (HDD LBA {boot_hdd_lba})")
        print(f"    - Protected Mode Kernel     : HDD LBA {kernel_hdd_lba}")

        # Build Sector 0 MBR
        mbr = bytearray(boot_data[:512])

        # Patch DAP to load kernel from kernel_hdd_lba
        dap_magic = bytes([0x10, 0x00, 0x40, 0x00])
        dap_idx = mbr.find(dap_magic)
        if dap_idx != -1:
            mbr[dap_idx + 8 : dap_idx + 16] = kernel_hdd_lba.to_bytes(8, "little")
            print(f"    - Patched MBR DAP LBA       : {kernel_hdd_lba} (offset {dap_idx})")
        else:
            print("    [!] Warning: DAP magic not found in boot.bin!")

        # Patch is_live_default = 1 for Live USB boot
        a0_idx = mbr.find(b"\xa0")
        if a0_idx != -1 and a0_idx < 50:
            target_addr = int.from_bytes(mbr[a0_idx+1:a0_idx+3], "little")
            var_offset = target_addr - 0x7C00
            if 0 <= var_offset < 446:
                mbr[var_offset] = 1
                print(f"    - Patched is_live_boot      : 1 (offset {var_offset})")

        # Active MBR Partition 1: Standard ISOHybrid partition covering entire disc
        part1 = bytearray(16)
        part1[0] = 0x80 # Bootable / Active for Legacy BIOS
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

        # MBR Partition 2: EFI System Partition (ESP) for UEFI Motherboards and Rufus
        if has_uefi and efiboot_img_bytes:
            # Section entry in Boot Catalog (bytes 96..127) -> UEFI efiboot.img
            efi_entry = boot_cat[96:128]
            efi_iso_lba = int.from_bytes(efi_entry[8:12], "little")
            efi_hdd_lba = efi_iso_lba * 4
            efi_sectors = len(efiboot_img_bytes) // 512

            print(f"    - El Torito UEFI Boot Image : ISO Sector {efi_iso_lba} (HDD LBA {efi_hdd_lba})")
            print(f"    - EFI System Partition (ESP): LBA {efi_hdd_lba}..{efi_hdd_lba + efi_sectors - 1} ({efi_sectors} sectors)")

            part2 = bytearray(16)
            part2[0] = 0x00 # Non-active in BIOS (UEFI ignores active bit)
            part2[1] = 0xFE
            part2[2] = 0xFF
            part2[3] = 0xFF
            part2[4] = 0xEF # Partition Type 0xEF = EFI System Partition (ESP)
            part2[5] = 0xFE
            part2[6] = 0xFF
            part2[7] = 0xFF
            part2[8:12] = efi_hdd_lba.to_bytes(4, "little")
            part2[12:16] = efi_sectors.to_bytes(4, "little")
            mbr[462:478] = part2
        else:
            mbr[462:478] = b"\x00" * 16

        mbr[478:494] = b"\x00" * 16 # Partition 3
        mbr[494:510] = b"\x00" * 16 # Partition 4
        mbr[510:512] = b"\x55\xaa"  # MBR Boot Signature

        # Write Hybrid MBR directly to Sector 0 of the ISO
        f.seek(0)
        f.write(mbr)
        f.flush()

    print(f"[+] Successfully built Universal Multi-Boot ISO (UEFI + Ventoy + BIOS): {iso_path} ({os.path.getsize(iso_path)} bytes)")

if __name__ == '__main__':
    project_dir = os.path.dirname(os.path.abspath(__file__))
    boot_bin = os.path.join(project_dir, "bin", "boot.bin")
    kernel_bin = os.path.join(project_dir, "bin", "kernel.bin")
    bootx64_efi = os.path.join(project_dir, "bin", "BOOTX64.EFI")
    bootia32_efi = os.path.join(project_dir, "bin", "BOOTIA32.EFI")
    iso_path = os.path.join(project_dir, "auraos.iso")
    make_bootable_iso(boot_bin, kernel_bin, iso_path, bootx64_efi, bootia32_efi)
