import struct
import os

def create_fat16_partition(kernel_bytes, readme_bytes, total_partition_sectors=18432):
    """
    Creates a valid, formatted FAT16 partition of 18,432 sectors (9 MB).
    Contains KERNEL.BIN and README.TXT.
    """
    BYTES_PER_SECTOR = 512
    SECTORS_PER_CLUSTER = 4 # 2 KB cluster
    CLUSTER_SIZE = BYTES_PER_SECTOR * SECTORS_PER_CLUSTER
    RESERVED_SECTORS = 1
    NUM_FATS = 2
    ROOT_ENTRIES = 512 # 512 * 32 bytes = 16384 bytes = 32 sectors
    ROOT_DIR_SECTORS = (ROOT_ENTRIES * 32) // BYTES_PER_SECTOR # 32 sectors
    SECTORS_PER_FAT = 18

    part = bytearray(total_partition_sectors * BYTES_PER_SECTOR)

    # 1. Volume Boot Record (VBR) at sector 0 of partition
    vbr = bytearray(BYTES_PER_SECTOR)
    vbr[0:3] = b'\xEB\x3C\x90' # JMP SHORT 0x3C; NOP
    vbr[3:11] = b'MSWIN4.1'
    struct.pack_into('<H', vbr, 11, BYTES_PER_SECTOR)
    vbr[13] = SECTORS_PER_CLUSTER
    struct.pack_into('<H', vbr, 14, RESERVED_SECTORS)
    vbr[16] = NUM_FATS
    struct.pack_into('<H', vbr, 17, ROOT_ENTRIES)
    struct.pack_into('<H', vbr, 19, total_partition_sectors) # 16-bit total sectors
    vbr[21] = 0xF8 # Media descriptor (hard disk)
    struct.pack_into('<H', vbr, 22, SECTORS_PER_FAT)
    struct.pack_into('<H', vbr, 24, 32) # Sectors per track
    struct.pack_into('<H', vbr, 26, 16) # Heads
    struct.pack_into('<I', vbr, 28, 2048) # Hidden sectors (LBA start of partition)
    struct.pack_into('<I', vbr, 32, 0) # Total sectors 32-bit (0 since <= 65535)
    vbr[36] = 0x80 # Drive number (0x80 = hard disk)
    vbr[38] = 0x29 # Extended boot signature
    struct.pack_into('<I', vbr, 39, 0x12345678) # Volume serial number
    vbr[43:54] = b'AURAOS     ' # Volume label (11 chars)
    vbr[54:62] = b'FAT16   ' # System ID
    vbr[510] = 0x55
    vbr[511] = 0xAA
    part[0:512] = vbr

    # 2. File Allocation Tables (FAT 1 & FAT 2)
    fat1_offset = RESERVED_SECTORS * BYTES_PER_SECTOR
    fat2_offset = (RESERVED_SECTORS + SECTORS_PER_FAT) * BYTES_PER_SECTOR

    # Calculate clusters for KERNEL.BIN
    kernel_clusters_needed = (len(kernel_bytes) + CLUSTER_SIZE - 1) // CLUSTER_SIZE
    readme_clusters_needed = (len(readme_bytes) + CLUSTER_SIZE - 1) // CLUSTER_SIZE

    fat = bytearray(SECTORS_PER_FAT * BYTES_PER_SECTOR)
    struct.pack_into('<H', fat, 0, 0xFFF8) # Cluster 0: Media descriptor
    struct.pack_into('<H', fat, 2, 0xFFFF) # Cluster 1: End of cluster mark

    current_cluster = 2
    # Chain for KERNEL.BIN
    kernel_start_cluster = current_cluster
    for i in range(kernel_clusters_needed):
        if i == kernel_clusters_needed - 1:
            struct.pack_into('<H', fat, current_cluster * 2, 0xFFFF) # EOF
        else:
            struct.pack_into('<H', fat, current_cluster * 2, current_cluster + 1)
        current_cluster += 1

    # Chain for README.TXT
    readme_start_cluster = current_cluster
    for i in range(readme_clusters_needed):
        if i == readme_clusters_needed - 1:
            struct.pack_into('<H', fat, current_cluster * 2, 0xFFFF) # EOF
        else:
            struct.pack_into('<H', fat, current_cluster * 2, current_cluster + 1)
        current_cluster += 1

    part[fat1_offset:fat1_offset + len(fat)] = fat
    part[fat2_offset:fat2_offset + len(fat)] = fat

    # 3. Root Directory
    root_offset = (RESERVED_SECTORS + NUM_FATS * SECTORS_PER_FAT) * BYTES_PER_SECTOR

    def make_dir_entry(name8_3, attr, cluster, size):
        entry = bytearray(32)
        entry[0:11] = name8_3.encode('ascii')
        entry[11] = attr
        struct.pack_into('<H', entry, 26, cluster)
        struct.pack_into('<I', entry, 28, size)
        return entry

    # Entry 0: Volume label
    entry_label = make_dir_entry('AURAOS     ', 0x08, 0, 0)
    # Entry 1: KERNEL.BIN
    entry_kernel = make_dir_entry('KERNEL  BIN', 0x20, kernel_start_cluster, len(kernel_bytes))
    # Entry 2: README.TXT
    entry_readme = make_dir_entry('README  TXT', 0x20, readme_start_cluster, len(readme_bytes))

    part[root_offset + 0:root_offset + 32] = entry_label
    part[root_offset + 32:root_offset + 64] = entry_kernel
    part[root_offset + 64:root_offset + 96] = entry_readme

    # 4. Data Region
    data_offset = root_offset + (ROOT_DIR_SECTORS * BYTES_PER_SECTOR)

    # Write KERNEL.BIN data
    k_offset = data_offset + (kernel_start_cluster - 2) * CLUSTER_SIZE
    part[k_offset:k_offset + len(kernel_bytes)] = kernel_bytes

    # Write README.TXT data
    r_offset = data_offset + (readme_start_cluster - 2) * CLUSTER_SIZE
    part[r_offset:r_offset + len(readme_bytes)] = readme_bytes

    return part

if __name__ == '__main__':
    k_data = b"TEST_KERNEL_PAYLOAD" * 100
    r_data = b"Welcome to AuraOS!\n"
    part = create_fat16_partition(k_data, r_data)
    print("FAT16 partition created successfully! Size:", len(part))
