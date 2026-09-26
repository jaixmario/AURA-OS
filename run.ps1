Write-Host "===================================================================" -ForegroundColor Cyan
Write-Host "              Starting AuraOS in QEMU (x86_64)..." -ForegroundColor White
Write-Host "===================================================================" -ForegroundColor Cyan
Write-Host "Video Resolution: 1024x768 @ 32bpp TrueColor" -ForegroundColor Gray
Write-Host "Display Adapter : VESA VBE 2.0+ (Standard VGA)" -ForegroundColor Gray
Write-Host "Memory Allocated: 256 MB RAM" -ForegroundColor Gray
Write-Host ""

$qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$disk = Join-Path $PSScriptRoot "auraos.img"

& $qemu -drive format=raw,file=$disk -m 256M -vga std
