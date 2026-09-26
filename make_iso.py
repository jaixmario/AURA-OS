import subprocess
import os
import shutil

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
        kernel_data = f.read() # ~28 KB

    # 2. Create the boot payload: boot.bin (512 bytes) + kernel.bin (28 KB)
    # The El Torito BIOS loads this 32 KB payload to 0x7C00.
    # boot.bin runs at 0x7C00 and finds kernel.bin already at 0x7E00.
    payload = bytearray(boot_data)
    payload.extend(kernel_data)
    # Pad payload to multiple of 2048 bytes (at least 64 sectors = 32768 bytes)
    target_payload_len = 32768
    if len(payload) < target_payload_len:
        payload.extend(b'\x00' * (target_payload_len - len(payload)))

    payload_file = os.path.join(iso_root, "boot.bin")
    with open(payload_file, "wb") as f:
        f.write(payload)

    # 3. Add user-visible files to CD root so disk is not empty
    with open(os.path.join(iso_root, "KERNEL.BIN"), "wb") as f:
        f.write(kernel_data)

    readme_content = """==================================================
           AuraOS Operating System Live CD
==================================================
Version     : 1.0.0 (32-bit x86 Protected Mode)
Graphics    : VESA VBE 2.0+ (1024x768 TrueColor)
Architecture: Bare-metal custom microkernel
Features    : Floating Window Manager, Terminal,
              Calculator, Paint Canvas, System Info.

Compatible with:
  - VMware Workstation / Player / Fusion / ESXi
  - Oracle VirtualBox
  - QEMU / KVM / Bochs
==================================================
"""
    with open(os.path.join(iso_root, "README.TXT"), "w", encoding="ascii") as f:
        f.write(readme_content)

    cfg_content = """# AuraOS Boot Configuration
RESOLUTION=1024x768
BPP=32
VBE_MODE=0x4118
DOUBLE_BUFFER=YES
HEAP_SIZE_MB=16
"""
    with open(os.path.join(iso_root, "SYSTEM.CFG"), "w", encoding="ascii") as f:
        f.write(cfg_content)

    # 4. Run mkisofs
    mkisofs = r"C:\Program Files (x86)\cdrtfe\tools\cdrtools\mkisofs.exe"
    cygwin_dir = r"C:\Program Files (x86)\cdrtfe\tools\cygwin"
    env = os.environ.copy()
    if os.path.exists(cygwin_dir):
        env["PATH"] = cygwin_dir + ";" + env["PATH"]

    cmd = [
        mkisofs,
        "-V", "AURA_OS",
        "-J", "-R", # Joliet & Rock Ridge extensions
        "-b", "boot.bin", # Boot image
        "-no-emul-boot",  # El Torito No Emulation
        "-boot-load-size", "64", # Load 64 sectors (32 KB)
        "-o", iso_path,
        iso_root
    ]

    res = subprocess.run(cmd, env=env, capture_output=True, text=True)
    if res.returncode != 0:
        print("[!] mkisofs error:\n", res.stderr)
        raise RuntimeError(f"mkisofs failed with code {res.returncode}")

    print(f"[+] Successfully built bootable ISO with mkisofs: {iso_path} ({os.path.getsize(iso_path)} bytes)")

    # Clean up temporary iso_root directory
    if os.path.exists(iso_root):
        shutil.rmtree(iso_root)

if __name__ == '__main__':
    project_dir = os.path.dirname(os.path.abspath(__file__))
    boot_bin = os.path.join(project_dir, "bin", "boot.bin")
    kernel_bin = os.path.join(project_dir, "bin", "kernel.bin")
    iso_path = os.path.join(project_dir, "auraos.iso")
    make_bootable_iso(boot_bin, kernel_bin, iso_path)
