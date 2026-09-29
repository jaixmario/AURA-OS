@echo off
set "QEMU=C:\Program Files\qemu\qemu-system-x86_64.exe"
set "OVMF=C:\Program Files\qemu\share\edk2-x86_64-code.fd"
"%QEMU%" -drive "if=pflash,format=raw,readonly=on,file=%OVMF%" -cdrom auraos.iso -m 256M -vga std
