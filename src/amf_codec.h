#ifndef AMF_H

#define AMF_H

#include <lua.h>

#include "amf_buf.h"
#include "amf_cursor.h"

#define AMF_VER0            0
#define AMF_VER3            3

#define AMF0_NUMBER         0
#define AMF0_BOOLEAN        1
#define AMF0_STRING         2
#define AMF0_OBJECT         3
#define AMF0_NULL           5
#define AMF0_UNDEFINED      6
#define AMF0_REFERENCE      7
#define AMF0_ECMA_ARRAY     8
#define AMF0_END_OF_OBJECT  9
#define AMF0_STRICT_ARRAY   10
#define AMF0_L_STRING       12
#define AMF0_TYPED_OBJECT   16
#define AMF0_AVMPLUS        17

#define AMF3_UNDEFINED      0x00
#define AMF3_NULL           0x01
#define AMF3_FALSE          0x02
#define AMF3_TRUE           0x03
#define AMF3_INTEGER        0x04
#define AMF3_DOUBLE         0x05
#define AMF3_STRING         0x06
#define AMF3_XMLDOC         0x07
#define AMF3_DATE           0x08
#define AMF3_ARRAY          0x09
#define AMF3_OBJECT         0x0a // no support
#define AMF3_XML            0x0b
#define AMF3_BYTEARRAY      0x0c

#define AMF3_MAX_UINT    536870912
#define AMF3_MAX_INT     268435455 //  (2^28)-1
#define AMF3_MIN_INT    -268435456 // -(2^28)

#define AMF3_MAX_STR_LEN    268435455
#define AMF3_MAX_REFERENCES 268435455

void amf0_encode(lua_State *L, amf_buf *buf, int avmplus, int index, int obj_ref_idx);
void amf0_decode(lua_State *L, amf_cursor *cur, int obj_ref_idx);

void amf3_encode(lua_State *L, amf_buf *buf, int index, int sidx, int oidx, int tidx);
void amf3_decode(lua_State *L, amf_cursor *cur, int str_ref_idx, int obj_ref_idx, int traits_ref_idx);

#endif /* end of include guard: AMF_H */
