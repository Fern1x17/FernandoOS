#include "string.h"

void* memset(void* dst, int val, size_t n) {
    uint8_t* p = (uint8_t*)dst;
    while (n--) *p++ = (uint8_t)val;
    return dst;
}

void* memcpy(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    while (n--) *d++ = *s++;
    return dst;
}

void* memmove(void* dst, const void* src, size_t n) {
    uint8_t* d = (uint8_t*)dst;
    const uint8_t* s = (const uint8_t*)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

int memcmp(const void* a, const void* b, size_t n) {
    const uint8_t* p = (const uint8_t*)a;
    const uint8_t* q = (const uint8_t*)b;
    while (n--) {
        if (*p != *q) return *p - *q;
        p++; q++;
    }
    return 0;
}

size_t strlen(const char* s) {
    size_t n = 0;
    while (*s++) n++;
    return n;
}

char* strcpy(char* dst, const char* src) {
    char* d = dst;
    while ((*d++ = *src++));
    return dst;
}

char* strncpy(char* dst, const char* src, size_t n) {
    char* d = dst;
    while (n && (*d++ = *src++)) n--;
    while (n--) *d++ = '\0';
    return dst;
}

int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int strncmp(const char* a, const char* b, size_t n) {
    while (n && *a && *a == *b) { a++; b++; n--; }
    if (!n) return 0;
    return (unsigned char)*a - (unsigned char)*b;
}

char* strcat(char* dst, const char* src) {
    char* d = dst;
    while (*d) d++;
    while ((*d++ = *src++));
    return dst;
}

char* strchr(const char* s, int c) {
    while (*s) {
        if (*s == (char)c) return (char*)s;
        s++;
    }
    return (c == '\0') ? (char*)s : (char*)0;
}

static char* strtok_ptr = (char*)0;

char* strtok(char* str, const char* delim) {
    if (str) strtok_ptr = str;
    if (!strtok_ptr) return (char*)0;

    /* Saltar delimitadores al inicio */
    while (*strtok_ptr && strchr(delim, *strtok_ptr))
        strtok_ptr++;

    if (!*strtok_ptr) { strtok_ptr = (char*)0; return (char*)0; }

    char* start = strtok_ptr;

    while (*strtok_ptr && !strchr(delim, *strtok_ptr))
        strtok_ptr++;

    if (*strtok_ptr) *strtok_ptr++ = '\0';
    else strtok_ptr = (char*)0;

    return start;
}

void itoa(int val, char* buf, int base) {
    if (base < 2 || base > 16) { *buf = '\0'; return; }
    char tmp[33];
    int i = 0;
    int neg = 0;

    if (val < 0 && base == 10) { neg = 1; val = -val; }
    if (val == 0) { tmp[i++] = '0'; }
    while (val) {
        int r = val % base;
        tmp[i++] = (char)(r < 10 ? '0' + r : 'a' + r - 10);
        val /= base;
    }
    if (neg) tmp[i++] = '-';
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
}

void utoa(uint32_t val, char* buf, int base) {
    if (base < 2 || base > 16) { *buf = '\0'; return; }
    char tmp[33];
    int i = 0;
    if (val == 0) { tmp[i++] = '0'; }
    while (val) {
        uint32_t r = val % (uint32_t)base;
        tmp[i++] = (char)(r < 10 ? '0' + r : 'a' + r - 10);
        val /= (uint32_t)base;
    }
    int j = 0;
    while (i--) buf[j++] = tmp[i];
    buf[j] = '\0';
}
