#ifndef AMF_REMOTING_H

#define AMF_REMOTING_H

#include "amf_codec.h"
#include "amf_buf.h"
#include "amf_cursor.h"

void amf_decode_msg(lua_State *L, amf_cursor *c);

void amf_encode_msg(lua_State *L, amf_buf *buf);

#endif /* end of include guard: AMF_REMOTING_H */
