# ✦ AuraOS - Modern x86 Graphical Operating System

A custom 32-bit graphical operating system built from scratch, featuring a high-resolution linear framebuffer desktop, floating window manager, hardware interrupt-driven mouse and keyboard drivers, and interactive built-in applications.

---

## 📸 Preview

![AuraOS Desktop](screenshot.png)

---

## 🚀 Quick Start & Emulators

### 1. Running in QEMU
Double-click `run.bat` in this folder, or run in PowerShell:
```powershell
.\run.ps1
```
Or manually:
```powershell
& "C:\Program Files\qemu\qemu-system-x86_64.exe" -drive format=raw,file=auraos.img -m 256M -vga std
```

### 2. Running in VMware (Workstation / Player / Fusion)
You can boot AuraOS in VMware using either the **Bootable CD-ROM ISO** or the **Virtual Disk (VMDK)**:

#### Method A: Using Bootable CD-ROM ISO (`auraos.iso`)
1. Open VMware and click **Create a New Virtual Machine**.
2. Select **Typical (recommended)** -> Next.
3. Select **Installer disc image file (iso)** and browse to:
   ```
   auraos.iso
   ```
4. For Guest Operating System, choose **Other** -> Version: **Other** (In VMware, **Other** is the standard 32-bit x86 option, while **Other 64-bit** is also supported).
5. Name your virtual machine (e.g. `AuraOS`) and allocate at least **128 MB RAM**.
6. Power on the virtual machine. AuraOS will boot directly from the CD into the desktop!

#### Method B: Using Virtual Disk (`auraos.vmdk`)
1. Create a new virtual machine -> Select **I will install the operating system later**.
2. Guest OS: **Other** -> Version: **Other** (32-bit).
3. When prompted for Disk, select **Use an existing virtual disk** -> Browse to `auraos.vmdk`.
4. Power on the VM.

### 3. Running in VirtualBox
1. Click **New** -> Name: `AuraOS` -> Type: `Other` -> Version: `Other/Unknown (32-bit)`.
2. RAM: `256 MB` -> Storage: Choose `auraos.iso` as Optical Disk or `auraos.img` as Hard Disk.
3. Start the virtual machine.

### 4. Flashing to USB with Rufus & Booting on Real Devices

`auraos.iso` is built as an **ISOHybrid** image with a Master Boot Record (MBR) and partition table at Sector 0 alongside standard El Torito CD-ROM boot records. This enables writing the ISO directly to USB flash drives to boot bare-metal PCs!

#### Writing with Rufus:
1. Insert your USB flash drive (any size, 1 GB+ is plenty) and open **Rufus**.
2. Under **Device**, select your USB flash drive.
3. Under **Boot selection**, click **SELECT** and choose `auraos.iso`.
4. Rufus will display the notification:
   > **"ISOHybrid image detected: ... As a result, DD image writing mode will be enforced."**
5. Click **OK**. *(This is expected and normal for hybrid ISOs: Rufus writes the raw sector image directly to the USB drive, preserving the MBR and boot sector bit-for-bit).*
6. Click **START**. Rufus will write the image to the USB drive.

#### Booting on Real PC Hardware:
1. Insert the USB drive into your target PC or laptop.
2. Power on the PC and press the **Boot Menu key** (typically `F12`, `F8`, `F11`, `F9`, or `Esc` depending on your motherboard/laptop brand: Dell/Lenovo: `F12`, Asus: `F8`/`F9`, HP: `F9`, Acer: `F12`).
3. Select your USB drive under **Legacy Boot** / **CSM** (AuraOS uses x86 32-bit BIOS/VBE architecture; if your PC is UEFI-only, ensure **CSM (Compatibility Support Module)** or **Legacy Boot** is enabled in BIOS setup).
4. The AuraOS bootloader will execute:
   - **Automatic Hardware Detection**: The bootloader automatically queries your monitor over the VBE DDC bus, extracts its native resolution (e.g. 1920x1080 Full HD, 1600x900, 1366x768, 1280x800), and boots into it automatically with zero user intervention!
   - You can also optionally press `1` to force 1920x1080 Full HD.
5. You will land on the **Live Installation Desktop** (`* AURA OS Live Installation Media`).
6. Double-click the **Install AuraOS** icon on the desktop to launch the permanent installation wizard and install AuraOS to your PC's internal hard drive!

---

## 🛠️ Building from Scratch

Prerequisites available on your machine:
- **NASM** (for assembling the MBR bootloader and interrupt stubs)
- **Clang / LLVM** (targeting `i686-none-elf` freestanding)
- **LLD Linker** (`ld.lld`)
- **Python 3** (for the automated build orchestrator)

To recompile:
```powershell
python build.py
```
Or double-click `build.bat`.

---

## 🏛️ System Architecture

```mermaid
flowchart TD
    MBR["MBR Boot Sector (boot.asm)<br/>• Loads kernel via LBA INT 13h<br/>• Sets VBE 1024x768 Mode<br/>• Enables A20 & PM Switch"]
    KERNEL["Protected Mode Kernel (0x10000)<br/>• 32-bit Flat Model<br/>• IDT (256 Gates)<br/>• PIC 8259 Remap"]
    DRIVERS["Hardware Drivers<br/>• PS/2 Mouse (IRQ12)<br/>• PS/2 Keyboard (IRQ1)<br/>• PIT 100Hz Timer (IRQ0)"]
    GFX["Graphics Engine<br/>• Double-Buffered VRAM<br/>• 2D Primitives & Gradients<br/>• 8x16 Bitmap Typography"]
    WM["Window Manager<br/>• Draggable Windows<br/>• Z-Order Stacking<br/>• Close/Min/Max Controls"]
    APPS["Desktop & Apps<br/>• Terminal Shell (13+ commands)<br/>• Settings Control Panel (Themes & Mouse)<br/>• Calculator (4-function GUI)<br/>• Canvas Paint (16 Colors & Tools)<br/>• Notes Editor (Text Editor)<br/>• System Info"]

    MBR --> KERNEL
    KERNEL --> DRIVERS
    KERNEL --> GFX
    GFX --> WM
    DRIVERS --> WM
    WM --> APPS
```

### 1. Bootloader (`boot/boot.asm`)
- **Size**: Exactly 512 bytes (fits in MBR sector 0).
- **Disk I/O**: Reads 128 sectors (64 KB) of kernel image into RAM at `0x10000` using BIOS INT 0x13 Extension (LBA Packet).
- **Display Setup**: Dynamically queries VBE 2.0+ controller info (`AX=4F00h`) and scans available video modes for Linear Frame Buffer TrueColor modes (1024x768 32-bit / 24-bit with fallbacks for VMware SVGA, VirtualBox, and QEMU).
- **Hardware Transition**: Enables Fast A20 gate, sets up Global Descriptor Table (GDT), enables protected mode (CR0 PE bit), sets stack to safe `0x1FFFF0` (1MB stack space), and executes far jump to 32-bit kernel.

### 2. Kernel Core & Drivers (`kernel/arch/`)
- **IDT (Interrupt Descriptor Table)**: 256 gates with CPU exception handlers (0..31) and hardware IRQs (32..47).
- **8259 PIC**: Remaps Master PIC to INT 0x20-0x27 and Slave PIC to INT 0x28-0x2F.
- **8254 PIT Timer**: Configured to 100 Hz (10ms resolution). Provides live uptime counter and sub-second timing.
- **CMOS Real-Time Clock (RTC)**: Interrogates hardware CMOS registers via I/O ports 0x70/0x71, decodes BCD/binary time and date, and provides live synchronization with host hardware.
- **ATA PIO Hard Disk Controller**: Low-level hard disk driver operating on Primary ATA bus ports (`0x1F0`–`0x1F7`, `0x3F6`). Implements 28-bit LBA mode sector reading, writing, and cache flushing (`0xE7`), providing real disk persistence for `auraos.img` and VMware `auraos.vmdk`.
- **PS/2 Keyboard**: Decodes scancodes, tracks modifier keys (Shift, Caps Lock), and buffers keystrokes for GUI apps.
- **PS/2 Mouse**: Configured to 200 Hz sample rate and 8 counts/mm resolution on IRQ12 with adaptive acceleration and selectable sensitivity (1.0x, 1.5x, 2.0x).

### 3. Graphics & Window Manager (`kernel/gfx/`, `kernel/wm/`)
- **Double Buffering**: 3.2 MB backbuffer in physical RAM at `0x200000` accelerated with x86 32-bit dword block transfers (`rep movsl` and `rep stosl`). Guarantees 100% flicker-free 60+ FPS rendering.
- **2D Primitives**: Anti-aliased circles, gradient fills, drop shadows, clipping rectangles, and crisp 8x16 typography.
- **Taskbar & Shell**: Live CMOS system date and time widget (`YYYY-MM-DD HH:MM:SS`) prominently positioned on the right of the taskbar next to the system tray, dynamic multi-window tabs starting on the left, and Start menu.
- **Window Management**:
  - Overlapping floating windows with z-order focus.
  - Smooth mouse title bar grabbing & dragging.
  - Full client drag and release callbacks (`on_drag`, `on_release`) for interactive apps.
  - Window control buttons: Close (red), Minimize (yellow), Maximize/Restore (green).
  - Active window header accent highlighting.

### 4. Interactive Applications (`kernel/apps/`)
- 💿 **Live OS Installer** (`app_installer.c`): Permanent hard disk installation wizard for real PC hardware and virtual machines. Formats the target drive, creates a bootable Master Boot Record (MBR) partition table, copies the AuraOS kernel binary and system files, configures user credentials, and equips the target hard disk with an independent boot sector to boot directly into user mode without live media.
- 📁 **File Explorer**: Modern user-friendly storage manager featuring an interactive, editable location / breadcrumb bar (`C:\<path>`) with a `[ Go ]` button to navigate to any folder path anywhere on disk, dynamic physical hard drive capacity detection displaying true disk sizes up to **8.0 GB** (`ATA (8.0 GB)`), top action toolbar (`Open in Notes`, `+ New`, `Delete`, `Refresh`), file `[ Move ]` capability to relocate files to any folder path, interactive "Store New File on Disk" dialog modal asking where to store files with custom path support, sidebar category filters with live badge counts, drive usage progress bar, color-coded file badges (`[TXT]`, `[CFG]`, `[SYS]`, `[SH]`, `[LOG]`), live monospace code preview card, double-click to open, and keyboard navigation (W/S, Enter, D, N, P).
- ⚙️ **Settings Control Panel**: Multi-tab control center:
  - **Personalize (Tab 0)**: Live photographic wallpaper switcher with smooth dynamic scaling.
  - **Date & Time (Tab 1)**: Hardware CMOS Real-Time Clock adjustments with bidirectional CMOS sync.
  - **Mouse & Speed (Tab 2)**: 3-tier cursor sensitivity curve (1.0x, 1.5x, 2.0x) and live coordinate tracker.
  - **Display & Res (Tab 3)**: **Interactive Display Size Switcher** with live mode switching across 7 resolutions (1920x1080 Full HD down to 800x600 SVGA), aspect ratio badges (`[16:9]`, `[4:3]`, `[5:4]`), adapter identification, VRAM address mapping, one-click `[ Apply ]`, and persistent configuration to `SYSTEM.CFG`.
  - **About System (Tab 4)**: Operating system specifications, kernel version, and live uptime telemetry.
- 🎨 **Canvas Paint**: Advanced creative drawing studio featuring a dedicated 10MB extended memory buffer, 16-color palette (2 rows), 4 brush sizes (1px Pencil, 3px Brush, 6px Marker, Eraser), smooth Bresenham continuous stroke interpolation, Clear button, color preview, and real-time status bar.
- 📝 **Notes Editor**: Multiline document editor with top action toolbar (`+ New`, `Open File`, `Save`, `Save As`), interactive "Save File to Disk" dialog modal allowing custom folder/path input anywhere on disk (`Folder / Path: ` input box alongside quick presets `Documents`, `Storage`, `System`), real-time hard disk device summary, active file indicator badge (`[ok]` / `* (Mod)`), interactive in-window Open File picker dialog, direct ATA hard disk sector persistence, gutter line numbering, and seamless inter-app opening from File Explorer and Terminal (`edit <file>`).
- 💻 **Terminal**: Interactive shell with command prompt (`aura@C:\<cwd>> `). Supports `res` / `display` (live resolution switching and mode query), `cd <path>`, `pwd`, `disk` (physical drive specs and sector readout), `mv <file> <folder>`, `files`, `edit <f>`, `ls`/`dir`, `cat`, `touch`, `rm`, `help`, `settings`, `paint`, `notes`, `calc`, `sysinfo`, `date`/`time`, `sync`, `theme`, `mem`, `ver`, `uptime`, `clear`/`cls`, `echo`, and `reboot`.
- 🧮 **Calculator**: Functional 16-button clickable desktop calculator supporting addition, subtraction, multiplication, and division.
- ℹ️ **System Info**: Displays OS architecture, display specs, live memory allocation, uptime, and animated CPU activity bar.

---

## 🖥️ Display Resolutions & Automatic Screen Size Detection

AuraOS features an **Automatic Monitor Display Size Detection engine** powered by real-mode **VBE DDC (Display Data Channel) & EDID (Extended Display Identification Data)** alongside a **Bochs/QEMU BGA (Bochs Graphics Adaptor)** hardware driver and **multi-resolution VBE linear framebuffer engine**:

### ✦ Automatic Display Size Detection:
- **Zero-Configuration Native Boot**: When booting on real PC hardware, laptops, or virtual machines, AuraOS queries the monitor over the I2C/DDC bus (`INT 10h, AX=4F15h, BL=01h`), extracts the monitor's 128-byte EDID block, parses the Detailed Timing Descriptor, and **automatically boots in the monitor's native resolution** (e.g. 1920×1080 Full HD on 1080p monitors, 1366×768 on widescreen laptops, 1280×800 on portable screens, 1600×900 on desktop displays)!
- **Automatic Fallback**: If an older legacy monitor or virtual adapter does not report DDC/EDID, AuraOS safely defaults to universal 1024×768 TrueColor.

| Mode ID | Resolution | Aspect Ratio | Category | Purpose |
|:---:|:---:|:---:|:---:|:---|
| **0** | **1920 × 1080** | **16:9** | **Full HD (1080p)** | Modern widescreen monitors, TVs, and high-DPI displays |
| **1** | **1600 × 900** | 16:9 | HD+ | Mid-size desktop screens and 15" laptops |
| **2** | **1366 × 768** | 16:9 | WXGA | Standard laptop and portable display native resolution |
| **3** | **1280 × 1024** | 5:4 | SXGA | Traditional 17" and 19" LCD desktop monitors |
| **4** | **1280 × 800** | 16:10 | WXGA | 16:10 widescreen laptops and virtual machines |
| **5** | **1280 × 720** | 16:9 | HD (720p) | 720p widescreen format |
| **6** | **1024 × 768** | 4:3 | XGA (Default) | Universal VESA VBE standard compatibility |
| **7** | **800 × 600** | 4:3 | SVGA | Legacy fallback display mode |

### How to Use & Switch Display Sizes:

1. **Automatic Detection at Boot (Default)**:
   - Just turn on your computer or launch the VM. AuraOS queries the monitor's EDID and automatically sets the native display size!
   - Optional: If you wish to manually force 1080p without EDID, press **`1`** during boot.

2. **Via Settings Control Panel**:
   - Open **Settings** from the desktop icon or taskbar Start Menu.
   - Switch to **Display & Res** (Tab 3).
   - The top hero card displays your connected monitor model and detected native resolution (e.g. `Monitor: QEMU Monitor (1280x800 16:10)` or `Monitor: DELL U2415 (1920x1080 16:9)`).
   - Click **`[ ✦ Auto-Detect Size ]`** to automatically identify and apply the native monitor resolution at any time.
   - Or click **`[ Apply ]`** next to any specific resolution to switch manually.
   - The desktop immediately adapts: the backbuffer reallocates, the photographic wallpaper scales dynamically, the mouse cursor boundaries re-calibrate, and all open windows are clamped into the new viewable desktop geometry!

3. **Via Terminal Shell**:
   - Launch **Terminal** and run:
     ```bash
     res auto          # Automatically detects connected monitor & applies native resolution
     res               # Shows monitor name, native resolution, and all supported modes
     res 1920 1080     # Instantly switches to 1080p Full HD
     res 0             # Switches using Mode ID 0 (1920x1080)
     ```


---

## 📂 Source Code Structure

```
AuraOS/
├── boot/
│   └── boot.asm            # 512-byte MBR bootloader with VBE & LBA read
├── kernel/
│   ├── arch/
│   │   ├── io.h            # In/Out port assembly macros
│   │   ├── idt.h / idt.c   # 256-entry Interrupt Descriptor Table
│   │   ├── pic.h / pic.c   # 8259 PIC initialization & EOI
│   │   ├── pit.h / pit.c   # 8254 Timer & Uptime counter
│   │   ├── rtc.h / rtc.c   # CMOS Real-Time Clock & Date driver
│   │   ├── ata.h / ata.c   # ATA PIO Hard Disk driver & Sector I/O
│   │   ├── display.h / display.c # Multi-resolution BGA & VBE display manager
│   │   ├── kbd.h / kbd.c   # PS/2 Keyboard scancode decoder
│   │   └── mouse.h / mouse.c # PS/2 Mouse packet decoder & coordinates
│   ├── fs/
│   │   └── vfs.h / vfs.c   # Virtual File System & storage manager
│   ├── gfx/
│   │   ├── font.h / font.c # 8x16 crisp bitmap font
│   │   ├── wallpaper.h / wallpaper.c # Dynamic scaling wallpaper engine
│   │   └── gfx.h / gfx.c   # Double-buffered graphics engine
│   ├── wm/
│   │   └── wm.h / wm.c     # Floating Window Manager & controls
│   ├── apps/
│   │   ├── apps.h
│   │   ├── app_installer.c # Hard disk installation wizard
│   │   ├── app_files.c     # File Explorer GUI application
│   │   ├── app_term.c      # Interactive Terminal application
│   │   ├── app_calc.c      # Calculator application
│   │   ├── app_paint.c     # Canvas Paint application
│   │   ├── app_notes.c     # Multiline Notes text editor
│   │   ├── app_settings.c  # Settings Control Panel & Display Switcher
│   │   └── app_sysinfo.c   # System Information dashboard
│   ├── libc/
│   │   └── string.h / string.c # Freestanding string & formatting library
│   ├── k_entry.asm         # 32-bit kernel entry & ISR/IRQ assembly stubs
│   ├── kernel.h
│   └── kernel.c            # Desktop shell, taskbar, start menu & event loop
├── linker.ld               # Linker script targeting 0x10000
├── build.py                # Master Python build orchestrator
├── make_iso.py             # Bootable CD-ROM ISO generator
├── build.bat               # Windows double-click build script
├── run.bat                 # Windows double-click QEMU launcher
├── run.ps1                 # PowerShell QEMU launcher
├── README.md               # Documentation & technical specs
├── auraos.img              # Bootable raw disk image (10 MB)
├── auraos.iso              # Bootable CD-ROM ISO for VMware / VirtualBox / QEMU
└── auraos.vmdk             # VMware Virtual Disk descriptor
```
