#include "amf_buf.h"

#include "endiness.h"

#include <string.h>
#include <stdio.h>

static void *
_default_alloc(void *ud, void *ptr, size_t osize, size_t nsize)
{
    (void)ud;
    (void)osize;

    return realloc(ptr, nsize);
}

amf_buf *
amf_buf_init(amf_buf *b, void *(*alloc)(void *ud, void *ptr, size_t osize, size_t nsize))
{
    if (alloc == NULL) {
        alloc = _default_alloc;
    }

    if (b == NULL) {
        b = alloc(NULL, NULL, 0, sizeof(amf_buf));
        if (b == NULL) return NULL;
    }

    b->b = NULL;
    b->len = 0;
    b->free = 0;


    b->alloc = alloc;

    return b;
}

void
amf_buf_append(amf_buf *buf, const char *b, size_t len)
{
    if (buf->free < len) {
        size_t nlen = buf->len + len;
        //buf->b = realloc(buf->b, nlen * 2);
        buf->b = buf->alloc(NULL, buf->b, buf->len, nlen * 2);
        buf->free = nlen;
    }

    memcpy(buf->b + buf->len, b, len);
    buf->len += len;
    buf->free -= len;
}

void
amf_buf_free(amf_buf *buf)
{
    free(buf->b);
    free(buf);
}

static void
_append_be(amf_buf *buf, char *p, size_t len)
{
    reverse_if_little_endian(p, len);
    amf_buf_append(buf, p, len);
}

void
amf_buf_append_char(amf_buf *buf, char c)
{
    amf_buf_append(buf, &c, 1);
}

void
amf_buf_append_double(amf_buf *buf, double d)
{
    _append_be(buf, (char *)&d, 8);
}

void amf_buf_append_u16(amf_buf *buf, uint16_t u)
{
    _append_be(buf, (char *)&u, 2);
}

void amf_buf_append_u32(amf_buf *buf, uint32_t u)
{
    _append_be(buf, (char *)&u, 4);
}

void amf_buf_append_u29(amf_buf *buf, int val)
{
    char b[4];
    int size;
    val &= 0x1fffffff;
    if (val <= 0x7f) {
        b[0] = val;
        size = 1;
    } else if (val <= 0x3fff) {
        b[1] = val & 0x7f;
        val >>= 7;
        b[0] = val | 0x80;
        size = 2;
    } else if (val <= 0x1fffff) {
        b[2] = val & 0x7f;
        val >>= 7;
        b[1] = val | 0x80;
        val >>= 7;
        b[0] = val | 0x80;
        size = 3;
    } else {
        b[3] = val;
        val >>= 8;
        b[2] = val | 0x80;
        val >>= 7;
        b[1] = val | 0x80;
        val >>= 7;
        b[0] = val | 0x80;
        size = 4;
    }
    amf_buf_append(buf, b, size);
}
