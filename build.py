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
        os.path.join(KERNEL_DIR, "gfx", "font.c"),
        os.path.join(KERNEL_DIR, "gfx", "gfx.c"),
        os.path.join(KERNEL_DIR, "wm", "wm.c"),
        os.path.join(KERNEL_DIR, "apps", "app_term.c"),
        os.path.join(KERNEL_DIR, "apps", "app_calc.c"),
        os.path.join(KERNEL_DIR, "apps", "app_paint.c"),
        os.path.join(KERNEL_DIR, "apps", "app_sysinfo.c"),
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
        "-T", linker_script,
        k_entry_o
    ] + c_objects + ["-o", kernel_bin]
    run_cmd(link_cmd, "Linking Kernel Binary")

    # 5. Package Disk Image (Raw HDD)
    disk_img = os.path.join(PROJECT_ROOT, "auraos.img")
    print(f"[*] Creating Disk Image: {disk_img}...")
    with open(boot_bin, "rb") as fb, open(kernel_bin, "rb") as fk, open(disk_img, "wb") as fout:
        boot_data = fb.read()
        kernel_data = fk.read()
        fout.write(boot_data)
        fout.write(kernel_data)
        cur_pos = fout.tell()
        # Pad to 10MB
        target_size = 10 * 1024 * 1024
        if cur_pos < target_size:
            fout.write(b'\x00' * (target_size - cur_pos))

    # 6. Package VMware / Universal Bootable ISO (El Torito CD-ROM)
    iso_path = os.path.join(PROJECT_ROOT, "auraos.iso")
    print(f"[*] Creating Bootable ISO for VMware / QEMU: {iso_path}...")
    try:
        from make_iso import make_no_emulation_iso
        make_no_emulation_iso(boot_bin, kernel_bin, iso_path)
    except Exception as e:
        print("[!] ISO creation warning:", e)

    # 7. Generate VMware Virtual Disk Descriptor (.vmdk)
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
ddb.geometry.sectors = "63"
ddb.geometry.heads = "16"
ddb.geometry.cylinders = "20"
ddb.virtualHWVersion = "4"
"""
    with open(vmdk_path, "w", encoding="ascii") as f:
        f.write(vmdk_content)

    print(f"\n[+] BUILD COMPLETE!")
    print(f"    - Bootloader: {len(boot_data)} bytes")
    print(f"    - Kernel:     {len(kernel_data)} bytes")
    print(f"    - Raw Disk:   {os.path.getsize(disk_img)} bytes ({disk_img})")
    if os.path.exists(iso_path):
        print(f"    - CD-ROM ISO: {os.path.getsize(iso_path)} bytes ({iso_path})")
    print(f"    - VMware VMDK:{vmdk_path}")

if __name__ == "__main__":
    main()
