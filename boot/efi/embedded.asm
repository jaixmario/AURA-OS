%ifidn __OUTPUT_FORMAT__, win32
%define kernel_bin_data _kernel_bin_data
%define kernel_bin_size _kernel_bin_size
%define trampoline_bin_data _trampoline_bin_data
%define trampoline_bin_size _trampoline_bin_size
%endif

section .rdata
global kernel_bin_data
global kernel_bin_size
global trampoline_bin_data
global trampoline_bin_size

kernel_bin_data:
    incbin "bin/kernel.bin"
kernel_bin_data_end:

kernel_bin_size:
    dd (kernel_bin_data_end - kernel_bin_data)

trampoline_bin_data:
    incbin "bin/trampoline.bin"
trampoline_bin_data_end:

trampoline_bin_size:
    dd (trampoline_bin_data_end - trampoline_bin_data)
