#ifndef VFS_H
#define VFS_H

#define VFS_MAX_FILENAME 32
#define VFS_MAX_FILES    24
#define VFS_MAX_FILESIZE 2048

#define FS_ATTR_READONLY 0x01
#define FS_ATTR_SYSTEM   0x02
#define FS_ATTR_USER     0x04

typedef struct {
    char name[VFS_MAX_FILENAME];
    unsigned int size;
    unsigned char attr;
    char data[VFS_MAX_FILESIZE];
    unsigned int created_year;
    unsigned int created_month;
    unsigned int created_day;
} vfs_file_t;

void vfs_init(void);
int  vfs_create_file(const char *name, const char *content, unsigned int size, unsigned char attr);
int  vfs_write_file(const char *name, const char *content, unsigned int size);
int  vfs_delete_file(const char *name);
vfs_file_t *vfs_find(const char *name);
vfs_file_t *vfs_get_at(int index);
int  vfs_get_count(void);
unsigned int vfs_get_total_used(void);
unsigned int vfs_get_total_capacity(void);

#endif
