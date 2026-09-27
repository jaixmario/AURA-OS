#ifndef VFS_H
#define VFS_H

#define VFS_MAX_FILENAME 32
#define VFS_MAX_FOLDER   32
#define VFS_MAX_FILES    128
#define VFS_MAX_FILESIZE 32768

#define FS_ATTR_READONLY 0x01
#define FS_ATTR_SYSTEM   0x02
#define FS_ATTR_USER     0x04

typedef struct {
    char name[VFS_MAX_FILENAME];
    char folder[VFS_MAX_FOLDER]; // "Documents", "Storage", "System", or custom folder path
    unsigned int size;
    unsigned char attr;
    char data[VFS_MAX_FILESIZE];
    unsigned int created_year;
    unsigned int created_month;
    unsigned int created_day;
    unsigned int disk_lba;
} vfs_file_t;

void vfs_init(void);
int  vfs_create_file(const char *name, const char *folder, const char *content, unsigned int size, unsigned char attr);
int  vfs_write_file(const char *name, const char *content, unsigned int size);
int  vfs_delete_file(const char *name);
int  vfs_move_file(const char *name, const char *new_folder);
vfs_file_t *vfs_find(const char *name);
vfs_file_t *vfs_get_at(int index);
int  vfs_get_count(void);
unsigned int vfs_get_total_used(void);
unsigned int vfs_get_total_capacity(void);
int  vfs_sync_disk(void);
int  vfs_is_disk_backed(void);
void vfs_get_disk_size_string(char *buf, int max_len);
const char *vfs_get_disk_model(void);
unsigned int vfs_get_disk_sectors(void);

#endif
