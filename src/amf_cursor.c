#include "amf_cursor.h"

#include <stdio.h>

amf_cursor *
amf_cursor_new(const char *p, size_t len)
{
    amf_cursor *cur = malloc(sizeof(*cur));
    if (!cur) return NULL;

    cur->p = p;
    cur->left = len;
    cur->err = AMF_CUR_NO_ERR;
    cur->err_msg = NULL;

    return cur;
}

void
amf_cursor_free(amf_cursor *cur)
{
    free(cur);
}

void
amf_cursor_read_u8(amf_cursor *c, uint8_t *i)
{
    amf_cursor_need(c, 1);
    *i = (c->p[0] & 0xff);
    amf_cursor_consume(c, 1);
}

void
amf_cursor_read_u16(amf_cursor *c, uint16_t *i)
{
    amf_cursor_need(c, 2);
    *i = ((c->p[0] & 0xff) << 8 | (c->p[1] & 0xff));
    amf_cursor_consume(c, 2);
}

void
amf_cursor_read_u32(amf_cursor *c, uint32_t *i)
{
    amf_cursor_need(c, 4);
    *i = (c->p[0] & 0xff) << 24
          | ((c->p[1] & 0xff) << 16)
          | ((c->p[2] & 0xff) << 8)
          | ((c->p[3] & 0xff));
    amf_cursor_consume(c, 4);
}

void
amf_cursor_read_u29(amf_cursor *c, unsigned int *i)
{
    int ofs = 0, res = 0, tmp;

    do {
        amf_cursor_need(c, 1);
		
        tmp = c->p[0];
        if (ofs == 3) {
            res <<= 8;
            res |= tmp & 0xff;
        } else {
            res <<= 7;
            res |= tmp & 0x7f;
        }

        amf_cursor_consume(c, 1);
    } while ((++ofs < 4) && (tmp & 0x80));

	if(i != NULL)
		*i = res;
}

void
amf_cursor_read_str(amf_cursor *c, const char **s, size_t *len)
{
    uint16_t slen;
    amf_cursor_read_u16(c, &slen);
    if (c->err) return;

    amf_cursor_need(c, slen);
    *s = c->p;
    amf_cursor_consume(c, slen);
    *len = slen;
}
