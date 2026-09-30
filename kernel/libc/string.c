#include "string.h"
#include <stdarg.h>

void *memset(void *dest, int c, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    unsigned char val = (unsigned char)c;

    if (n >= 16) {
        while (((unsigned int)d & 3) && n > 0) {
            *d++ = val;
            n--;
        }
        unsigned int val32 = (val << 24) | (val << 16) | (val << 8) | val;
        size_t dwords = n >> 2;
        unsigned int *d32 = (unsigned int *)d;
        while (dwords--) {
            *d32++ = val32;
        }
        d = (unsigned char *)d32;
        n &= 3;
    }
    while (n--) {
        *d++ = val;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;

    if (n >= 16 && (((unsigned int)d & 3) == ((unsigned int)s & 3))) {
        while (((unsigned int)d & 3) && n > 0) {
            *d++ = *s++;
            n--;
        }
        size_t dwords = n >> 2;
        unsigned int *d32 = (unsigned int *)d;
        const unsigned int *s32 = (const unsigned int *)s;
        while (dwords--) {
            *d32++ = *s32++;
        }
        d = (unsigned char *)d32;
        s = (const unsigned char *)s32;
        n &= 3;
    }
    while (n--) {
        *d++ = *s++;
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        for (size_t i = 0; i < n; i++) d[i] = s[i];
    } else if (d > s) {
        for (size_t i = n; i > 0; i--) d[i - 1] = s[i - 1];
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const unsigned char *p1 = (const unsigned char *)s1;
    const unsigned char *p2 = (const unsigned char *)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) return p1[i] - p2[i];
    }
    return 0;
}

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len]) len++;
    return len;
}

char *strcpy(char *dest, const char *src) {
    size_t i = 0;
    while (src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
    return dest;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == '\0') {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

char *strcat(char *dest, const char *src) {
    size_t len = strlen(dest);
    size_t i = 0;
    while (src[i]) {
        dest[len + i] = src[i];
        i++;
    }
    dest[len + i] = '\0';
    return dest;
}

char *strstr(const char *haystack, const char *needle) {
    if (!*needle) return (char *)haystack;
    while (*haystack) {
        const char *h = haystack;
        const char *n = needle;
        while (*h && *n && (*h == *n)) {
            h++;
            n++;
        }
        if (!*n) return (char *)haystack;
        haystack++;
    }
    return 0;
}

void itoa(int n, char *str, int base) {
    int i = 0;
    int is_negative = 0;
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    if (n < 0 && base == 10) {
        is_negative = 1;
        n = -n;
    }
    while (n != 0) {
        int rem = n % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        n = n / base;
    }
    if (is_negative) str[i++] = '-';
    str[i] = '\0';
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}

void uitoa(unsigned int n, char *str, int base) {
    int i = 0;
    if (n == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return;
    }
    while (n != 0) {
        unsigned int rem = n % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        n = n / base;
    }
    str[i] = '\0';
    for (int j = 0, k = i - 1; j < k; j++, k--) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
    }
}

int snprintf(char *buf, size_t size, const char *fmt, ...) {
    if (!buf || size == 0) return 0;
    va_list args;
    va_start(args, fmt);
    size_t idx = 0;

    for (size_t i = 0; fmt[i] != '\0' && idx + 1 < size; i++) {
        if (fmt[i] == '%' && fmt[i+1] != '\0') {
            i++;
            int pad_zero = 0;
            int width = 0;

            if (fmt[i] == '0') {
                pad_zero = 1;
                i++;
            }
            while (fmt[i] >= '0' && fmt[i] <= '9') {
                width = width * 10 + (fmt[i] - '0');
                i++;
            }

            if (fmt[i] == 'd') {
                int val = va_arg(args, int);
                char tmp[32];
                itoa(val, tmp, 10);
                int len = (int)strlen(tmp);
                while (len < width && idx + 1 < size) {
                    buf[idx++] = pad_zero ? '0' : ' ';
                    width--;
                }
                for (size_t k = 0; tmp[k] && idx + 1 < size; k++) buf[idx++] = tmp[k];
            } else if (fmt[i] == 'u') {
                unsigned int val = va_arg(args, unsigned int);
                char tmp[32];
                uitoa(val, tmp, 10);
                int len = (int)strlen(tmp);
                while (len < width && idx + 1 < size) {
                    buf[idx++] = pad_zero ? '0' : ' ';
                    width--;
                }
                for (size_t k = 0; tmp[k] && idx + 1 < size; k++) buf[idx++] = tmp[k];
            } else if (fmt[i] == 'x' || fmt[i] == 'X') {
                unsigned int val = va_arg(args, unsigned int);
                char tmp[32];
                uitoa(val, tmp, 16);
                int len = (int)strlen(tmp);
                while (len < width && idx + 1 < size) {
                    buf[idx++] = pad_zero ? '0' : ' ';
                    width--;
                }
                for (size_t k = 0; tmp[k] && idx + 1 < size; k++) buf[idx++] = tmp[k];
            } else if (fmt[i] == 's') {
                const char *s = va_arg(args, const char *);
                if (!s) s = "(null)";
                for (size_t k = 0; s[k] && idx + 1 < size; k++) buf[idx++] = s[k];
            } else if (fmt[i] == 'c') {
                char c = (char)va_arg(args, int);
                if (idx + 1 < size) buf[idx++] = c;
            } else if (fmt[i] == '%') {
                if (idx + 1 < size) buf[idx++] = '%';
            }
        } else {
            buf[idx++] = fmt[i];
        }
    }
    buf[idx] = '\0';
    va_end(args);
    return (int)idx;
}

// 64-bit unsigned division & modulo helpers for 32-bit freestanding target (compiler-rt)
unsigned long long __udivdi3(unsigned long long a, unsigned long long b) {
    if (b > a || b == 0) return 0;
    if ((b >> 32) == 0) {
        unsigned int d = (unsigned int)b;
        unsigned int n_hi = (unsigned int)(a >> 32);
        unsigned int n_lo = (unsigned int)a;
        unsigned int q_hi = 0, q_lo = 0;
        if (n_hi >= d) {
            q_hi = n_hi / d;
            n_hi %= d;
        }
        __asm__ volatile ("divl %4"
                          : "=a"(q_lo), "=d"(n_hi)
                          : "a"(n_lo), "d"(n_hi), "r"(d));
        return ((unsigned long long)q_hi << 32) | q_lo;
    }
    // Shift-subtract bitwise binary division
    unsigned long long q = 0;
    unsigned long long r = 0;
    for (int i = 63; i >= 0; i--) {
        r = (r << 1) | ((a >> i) & 1);
        if (r >= b) {
            r -= b;
            q |= (1ULL << i);
        }
    }
    return q;
}

unsigned long long __umoddi3(unsigned long long a, unsigned long long b) {
    if (b > a || b == 0) return a;
    unsigned long long q = __udivdi3(a, b);
    return a - (q * b);
}
