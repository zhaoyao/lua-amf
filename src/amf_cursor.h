#ifndef AMF_CURSOR_H

#define AMF_CURSOR_H

#include <stdlib.h>
#include <stdint.h>

#define AMF_CUR_NO_ERR     0
#define AMF_CUR_ERR_EOF    1
#define AMF_CUR_ERR_BADFMT 2

typedef struct amf_cursor {
    const char *p;
    size_t left;

    int err;
    const char *err_msg;
} amf_cursor;

#define amf_cursor_consume(c, len) do { \
    c->p += len;                        \
    c->left -= len;                     \
} while(0)

#define amf_cursor_need(c, len) do {    \
    if (c->left < len) {                \
        c->err = AMF_CUR_ERR_EOF;       \
        c->err_msg = "eof";             \
        return;                         \
    }                                   \
} while(0)

#define amf_cursor_skip(c, len) do {    \
    amf_cursor_need(c, len);            \
    amf_cursor_consume(c, len);         \
} while(0)

#define amf_cursor_checkerr(c) do { if (c->err) return; } while(0)

amf_cursor *amf_cursor_new(const char *p, size_t len);
void amf_cursor_free(amf_cursor *c);

void amf_cursor_read_u8(amf_cursor *c, uint8_t *i);
void amf_cursor_read_u16(amf_cursor *c, uint16_t *i);
void amf_cursor_read_u32(amf_cursor *c, uint32_t *i);


void amf_cursor_read_u29(amf_cursor *c, unsigned int *i);
void amf_cursor_read_str(amf_cursor *c, const char **s, size_t *len);


#endif /* end of include guard: AMF_CURSOR_H */
