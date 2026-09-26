#ifndef STRING_H
#define STRING_H

typedef unsigned int size_t;

void *memset(void *dest, int c, size_t n);
void *memcpy(void *dest, const void *src, size_t n);
void *memmove(void *dest, const void *src, size_t n);
int memcmp(const void *s1, const void *s2, size_t n);

size_t strlen(const char *s);
char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, size_t n);
int strcmp(const char *s1, const char *s2);
int strncmp(const char *s1, const char *s2, size_t n);
char *strcat(char *dest, const char *src);
char *strstr(const char *haystack, const char *needle);

void itoa(int n, char *str, int base);
void uitoa(unsigned int n, char *str, int base);
int snprintf(char *buf, size_t size, const char *fmt, ...);

#endif
