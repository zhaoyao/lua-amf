#ifndef AMF_BUF_H

#define AMF_BUF_H

#include <stdlib.h>
#include <stdint.h>

/*
 * len:  current buf length
 * free: current free space
 */
typedef struct amf_buf {
    char *b;
    size_t len, free;
    void *(*alloc)(void *ud, void *ptr, size_t osize, size_t nsize);
} amf_buf;

amf_buf *amf_buf_init(amf_buf *buf, void *(*alloc)(void *ud, void *ptr, size_t osize, size_t nsize));
void amf_buf_free(amf_buf *buf);

void amf_buf_append(amf_buf *buf, const char *b, size_t len);
void amf_buf_append_char(amf_buf *buf, char c);
void amf_buf_append_double(amf_buf *buf, double d);
void amf_buf_append_u16(amf_buf *buf, uint16_t u);
void amf_buf_append_u32(amf_buf *buf, uint32_t u);
void amf_buf_append_u29(amf_buf *buf, int i);

#endif /* end of include guard: AMF_BUF_H */
