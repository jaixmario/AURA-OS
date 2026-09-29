Write-Host "===================================================================" -ForegroundColor Cyan
Write-Host "         Starting AuraOS in UEFI Mode (OVMF + GOP)..." -ForegroundColor White
Write-Host "===================================================================" -ForegroundColor Cyan
Write-Host "Boot Media       : Universal Multi-Boot ISO (auraos.iso)" -ForegroundColor Gray
Write-Host "Firmware         : EDK2 / OVMF UEFI x86_64" -ForegroundColor Gray
Write-Host "Graphics Protocol: UEFI Graphics Output Protocol (GOP)" -ForegroundColor Gray
Write-Host "Memory Allocated : 256 MB RAM" -ForegroundColor Gray
Write-Host ""

$qemu = "C:\Program Files\qemu\qemu-system-x86_64.exe"
$ovmf = "C:\Program Files\qemu\share\edk2-x86_64-code.fd"
$iso  = Join-Path $PSScriptRoot "auraos.iso"

& $qemu -drive "if=pflash,format=raw,readonly=on,file=$ovmf" -cdrom $iso -m 256M -vga std
