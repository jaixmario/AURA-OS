@echo off
title AuraOS - Modern x86 GUI Operating System
echo ===================================================================
echo               Starting AuraOS in QEMU (x86_64)...
echo ===================================================================
echo.
echo Video Resolution: 1024x768 @ 32bpp TrueColor
echo Display Adapter : VESA VBE 2.0+ (Standard VGA)
echo Memory Allocated: 256 MB RAM
echo.

"C:\Program Files\qemu\qemu-system-x86_64.exe" -drive format=raw,file="%~dp0auraos.img" -m 256M -vga std

if %ERRORLEVEL% NEQ 0 (
    echo.
    echo [!] QEMU exited with error code %ERRORLEVEL%.
    pause
)
