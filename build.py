import os
import sys
import subprocess

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
BIN_DIR = os.path.join(PROJECT_ROOT, "bin")
BOOT_DIR = os.path.join(PROJECT_ROOT, "boot")
KERNEL_DIR = os.path.join(PROJECT_ROOT, "kernel")

NASM = "nasm"
CLANG = "clang"
LLD = r"C:\Program Files\LLVM\bin\ld.lld.exe"
if not os.path.exists(LLD):
    LLD = "ld.lld"

LLD_LINK = r"C:\Program Files\LLVM\bin\lld-link.exe"
if not os.path.exists(LLD_LINK):
    LLD_LINK = "lld-link"

def run_cmd(cmd, desc):
    print(f"[*] {desc}...")
    res = subprocess.run(cmd, capture_output=True, text=True)
    if res.returncode != 0:
        print(f"[!] FAILED: {desc}")
        print("STDOUT:\n", res.stdout)
        print("STDERR:\n", res.stderr)
        sys.exit(1)

def main():
    os.makedirs(BIN_DIR, exist_ok=True)

    # 1. Assemble MBR bootloader
    boot_asm = os.path.join(BOOT_DIR, "boot.asm")
    boot_bin = os.path.join(BIN_DIR, "boot.bin")
    run_cmd([NASM, "-f", "bin", boot_asm, "-o", boot_bin], "Assembling MBR Bootloader")

    # 2. Assemble Kernel entry stubs
    k_entry_asm = os.path.join(KERNEL_DIR, "k_entry.asm")
    k_entry_o = os.path.join(BIN_DIR, "k_entry.o")
    run_cmd([NASM, "-f", "elf32", k_entry_asm, "-o", k_entry_o], "Assembling Kernel Entry Stubs")

    # 3. Compile C sources
    c_sources = [
        os.path.join(KERNEL_DIR, "libc", "string.c"),
        os.path.join(KERNEL_DIR, "arch", "pic.c"),
        os.path.join(KERNEL_DIR, "arch", "idt.c"),
        os.path.join(KERNEL_DIR, "arch", "pit.c"),
        os.path.join(KERNEL_DIR, "arch", "kbd.c"),
        os.path.join(KERNEL_DIR, "arch", "mouse.c"),
        os.path.join(KERNEL_DIR, "arch", "rtc.c"),
        os.path.join(KERNEL_DIR, "arch", "ata.c"),
        os.path.join(KERNEL_DIR, "arch", "display.c"),
        os.path.join(KERNEL_DIR, "gfx", "font.c"),
        os.path.join(KERNEL_DIR, "gfx", "gfx.c"),
        os.path.join(KERNEL_DIR, "gfx", "picojpeg.c"),
        os.path.join(KERNEL_DIR, "gfx", "wallpaper.c"),
        os.path.join(KERNEL_DIR, "wm", "wm.c"),
        os.path.join(KERNEL_DIR, "fs", "vfs.c"),
        os.path.join(KERNEL_DIR, "apps", "app_term.c"),
        os.path.join(KERNEL_DIR, "apps", "app_calc.c"),
        os.path.join(KERNEL_DIR, "apps", "app_paint.c"),
        os.path.join(KERNEL_DIR, "apps", "app_notes.c"),
        os.path.join(KERNEL_DIR, "apps", "app_sysinfo.c"),
        os.path.join(KERNEL_DIR, "apps", "app_settings.c"),
        os.path.join(KERNEL_DIR, "apps", "app_files.c"),
        os.path.join(KERNEL_DIR, "apps", "app_installer.c"),
        os.path.join(KERNEL_DIR, "kernel.c")
    ]

    c_objects = []
    clang_flags = [
        "-target", "i686-none-elf",
        "-O2",
        "-ffreestanding",
        "-fno-pie",
        "-fno-stack-protector",
        "-mno-sse",
        "-mno-80387",
        "-Wall",
        "-Wextra",
        "-Wno-shift-negative-value",
        "-Wno-unsequenced",
        "-I" + KERNEL_DIR
    ]

    for src in c_sources:
        base_name = os.path.splitext(os.path.basename(src))[0]
        obj = os.path.join(BIN_DIR, f"{base_name}.o")
        c_objects.append(obj)
        run_cmd([CLANG] + clang_flags + ["-c", src, "-o", obj], f"Compiling {os.path.basename(src)}")

    # 4. Link Kernel binary
    kernel_bin = os.path.join(BIN_DIR, "kernel.bin")
    linker_script = os.path.join(PROJECT_ROOT, "linker.ld")
    link_cmd = [
        LLD,
        "-m", "elf_i386",
        "--oformat", "binary",
        "-Map", os.path.join(BIN_DIR, "kernel.map"),
        "-T", linker_script,
        k_entry_o
    ] + c_objects + ["-o", kernel_bin]
    run_cmd(link_cmd, "Linking Kernel Binary")

    # 5. Build Dual-Mode UEFI Bootloaders (BOOTX64.EFI and BOOTIA32.EFI)
    efi_dir = os.path.join(BOOT_DIR, "efi")
    tramp_asm = os.path.join(efi_dir, "trampoline.asm")
    tramp_bin = os.path.join(BIN_DIR, "trampoline.bin")
    run_cmd([NASM, "-f", "bin", tramp_asm, "-o", tramp_bin], "Assembling UEFI Mode-Switch Trampoline")

    embedded_asm = os.path.join(efi_dir, "embedded.asm")
    emb64_obj = os.path.join(BIN_DIR, "embedded64.obj")
    run_cmd([NASM, "-f", "win64", embedded_asm, "-o", emb64_obj], "Packaging x86_64 Embedded Payload")

    emb32_obj = os.path.join(BIN_DIR, "embedded32.obj")
    run_cmd([NASM, "-f", "win32", embedded_asm, "-o", emb32_obj], "Packaging IA32 Embedded Payload")

    # 64-bit UEFI
    bootx64_c = os.path.join(efi_dir, "bootx64.c")
    bootx64_obj = os.path.join(BIN_DIR, "bootx64.obj")
    bootx64_efi = os.path.join(BIN_DIR, "BOOTX64.EFI")
    run_cmd([CLANG, "-target", "x86_64-unknown-windows", "-ffreestanding",
             "-fno-stack-protector", "-mno-stack-arg-probe", "-fshort-wchar", "-mno-red-zone",
             "-O2", "-c", bootx64_c, "-o", bootx64_obj], "Compiling 64-bit UEFI Bootloader (BOOTX64.EFI)")
    run_cmd([LLD_LINK, "/subsystem:efi_application", "/entry:EfiMain", "/nodefaultlib",
             bootx64_obj, emb64_obj, f"/out:{bootx64_efi}"], "Linking 64-bit UEFI Executable (BOOTX64.EFI)")

    # 32-bit UEFI
    bootia32_c = os.path.join(efi_dir, "bootia32.c")
    bootia32_obj = os.path.join(BIN_DIR, "bootia32.obj")
    bootia32_efi = os.path.join(BIN_DIR, "BOOTIA32.EFI")
    run_cmd([CLANG, "-target", "i686-unknown-windows", "-ffreestanding",
             "-fno-stack-protector", "-mno-stack-arg-probe", "-fshort-wchar",
             "-O2", "-c", bootia32_c, "-o", bootia32_obj], "Compiling 32-bit UEFI Bootloader (BOOTIA32.EFI)")
    run_cmd([LLD_LINK, "/subsystem:efi_application", "/entry:EfiMain", "/nodefaultlib",
             bootia32_obj, emb32_obj, f"/out:{bootia32_efi}"], "Linking 32-bit UEFI Executable (BOOTIA32.EFI)")

    # 6. Package Partitioned Disk Image (10 MB MBR Hard Disk)
    disk_img = os.path.join(PROJECT_ROOT, "auraos.img")
    print(f"[*] Creating Partitioned Disk Image: {disk_img}...")
    from format_fat import create_fat16_partition

    with open(boot_bin, "rb") as fb, open(kernel_bin, "rb") as fk:
        boot_data = fb.read() # 512 bytes
        kernel_data = fk.read() # ~496 KB

    readme_data = b"""==================================================
           AuraOS Graphical Operating System
==================================================
Version     : 1.2.0 (32-bit x86 Protected Mode)
Graphics    : Dynamic TrueColor Framebuffer (GOP & VBE 2.0+)
Architecture: Bare-metal custom microkernel
Features    : Floating Window Manager, Terminal,
              Calculator, Paint Canvas, System Info.

Installed on: Primary MBR Hard Disk (FAT16 Partition)
==================================================
"""

    TOTAL_DISK_SECTORS = 20480
    PARTITION_START_SECTOR = 2048
    PARTITION_SECTORS = TOTAL_DISK_SECTORS - PARTITION_START_SECTOR

    fat_partition = create_fat16_partition(kernel_data, readme_data, PARTITION_SECTORS)

    disk = bytearray(TOTAL_DISK_SECTORS * 512)
    disk[0:512] = boot_data
    disk[512:512 + len(kernel_data)] = kernel_data
    part_byte_offset = PARTITION_START_SECTOR * 512
    disk[part_byte_offset:part_byte_offset + len(fat_partition)] = fat_partition

    with open(disk_img, "wb") as fout:
        fout.write(disk)

    # 7. Package Universal Multi-Boot ISO (UEFI + Ventoy + Rufus + Legacy BIOS)
    iso_path = os.path.join(PROJECT_ROOT, "auraos.iso")
    print(f"[*] Creating Universal Multi-Boot ISO for UEFI / Ventoy / VMware / QEMU: {iso_path}...")
    try:
        from make_iso import make_bootable_iso
        make_bootable_iso(boot_bin, kernel_bin, iso_path, bootx64_efi, bootia32_efi)
    except Exception as e:
        print("[!] ISO creation warning:", e)

    # 8. Generate VMware Virtual Disk Descriptor (.vmdk) with exact matching geometry
    vmdk_path = os.path.join(PROJECT_ROOT, "auraos.vmdk")
    print(f"[*] Creating VMware VMDK descriptor: {vmdk_path}...")
    vmdk_content = """# Disk DescriptorFile
version=1
encoding="UTF-8"
CID=fffffffe
parentCID=ffffffff
isNativeSnapshot="no"
createType="monolithicFlat"

# Extent description (10 MB = 20480 sectors of 512 bytes)
RW 20480 FLAT "auraos.img" 0

# The Disk Data Base 
#DDB

ddb.adapterType = "ide"
ddb.geometry.cylinders = "40"
ddb.geometry.heads = "16"
ddb.geometry.sectors = "32"
ddb.virtualHWVersion = "4"
"""
    with open(vmdk_path, "w", encoding="ascii") as f:
        f.write(vmdk_content)

    print(f"\n[+] BUILD COMPLETE!")
    print(f"    - BIOS Bootloader: {len(boot_data)} bytes")
    print(f"    - Kernel Binary:   {len(kernel_data)} bytes")
    print(f"    - UEFI 64-bit:     {os.path.getsize(bootx64_efi)} bytes ({bootx64_efi})")
    print(f"    - UEFI 32-bit:     {os.path.getsize(bootia32_efi)} bytes ({bootia32_efi})")
    print(f"    - Hard Disk Image: {os.path.getsize(disk_img)} bytes ({disk_img}) [MBR + FAT16]")
    if os.path.exists(iso_path):
        print(f"    - Universal ISO:   {os.path.getsize(iso_path)} bytes ({iso_path}) [Dual El Torito + ISOHybrid ESP]")
    print(f"    - VMware VMDK:     {vmdk_path}")

if __name__ == "__main__":
    main()
