#include "amf_remoting.h"

#include <stdint.h>
#include <lauxlib.h>

static void
decode_hdr(lua_State *L, amf_cursor *c)
{
    lua_createtable(L, 3, 0);

    const char *name;
    size_t len;
    amf_cursor_read_str(c, &name, &len);
    amf_cursor_checkerr(c);
    lua_pushlstring(L, name, len);
    lua_rawseti(L, -2, 1);

    uint8_t must_understand;
    amf_cursor_read_u8(c, &must_understand);
    amf_cursor_checkerr(c);
    lua_pushboolean(L, must_understand);
    lua_rawseti(L, -2, 2);

    /* content length*/
    amf_cursor_skip(c, 4);

    lua_newtable(L);
    amf0_decode(L, c, lua_gettop(L));
    amf_cursor_checkerr(c);
    lua_rawseti(L, -3, 3);

    /* ref table */
    lua_pop(L, 1);
}

static void
decode_body(lua_State *L, amf_cursor *c)
{
    lua_createtable(L, 3, 0);

    const char *uri;
    size_t len;

    /* target uri */
    amf_cursor_read_str(c, &uri, &len);
    amf_cursor_checkerr(c);
    lua_pushlstring(L, uri, len);
    lua_rawseti(L, -2, 1);

    /* response uri */
    amf_cursor_read_str(c, &uri, &len);
    amf_cursor_checkerr(c);
    lua_pushlstring(L, uri, len);
    lua_rawseti(L, -2, 2);

    /* content length*/
    amf_cursor_skip(c, 4);

    lua_newtable(L);
    amf0_decode(L, c, lua_gettop(L));
    amf_cursor_checkerr(c);
    lua_rawseti(L, -3, 3);

    /* ref table */
    lua_pop(L, 1);

}

void amf_decode_msg(lua_State *L, amf_cursor *c)
{
    uint16_t ver, hc, bc;

    lua_createtable(L, 3, 0);

    amf_cursor_read_u16(c, &ver);
    amf_cursor_checkerr(c);
    lua_pushinteger(L, ver);
    lua_rawseti(L, -2, 1);


    /* headers */
    amf_cursor_read_u16(c, &hc);
    if (hc > 0) lua_createtable(L, hc, 0);
    for (unsigned int i = 0; i < hc; i++) {
        decode_hdr(L, c);
        amf_cursor_checkerr(c);
        lua_rawseti(L, -2, i+1);
    }
    lua_rawseti(L, -2, 2);

    /* bodies */
    amf_cursor_read_u16(c, &bc);
    if (bc > 0) lua_createtable(L, bc, 0);
    for (unsigned int i = 0; i < hc; i++) {
        decode_body(L, c);
        amf_cursor_checkerr(c);
        lua_rawseti(L, -2, i+1);
    }
    lua_rawseti(L, -2, 3);
}

static void
encode_hdr(lua_State *L, amf_buf *buf, int ver)
{
    if(!lua_istable(L, -1)) {
        luaL_error(L, "invalid header structure, must be a dense table");
    }

    /* name */
    lua_rawgeti(L, -1, 1);
    size_t len;
    const char *name = lua_tolstring(L, -1, &len);
    amf_buf_append_u16(buf, (uint16_t)len);
    if (len > 0)
        amf_buf_append(buf, name, (uint16_t)len);
    lua_pop(L, 1);

    /* must understand */
    lua_rawgeti(L, -1, 2);
    int mu = lua_toboolean(L, -1);
    amf_buf_append_char(buf, (char)mu);
    lua_pop(L, 1);

    /* content length */
    amf_buf_append_u32(buf, (uint32_t)0);

    lua_rawgeti(L, -1, 3);
    lua_newtable(L);
    amf0_encode(L, buf, (ver == 3), -2, lua_gettop(L));
    lua_pop(L, 2); /* pops the obj and ref table */
}


static void
encode_body(lua_State *L, amf_buf *buf, int ver)
{
    if(!lua_istable(L, -1)) {
        luaL_error(L, "invalid body structure, must be a dense table");
    }

    size_t len;
    const char *uri;

    /* target uri */
    lua_rawgeti(L, -1, 1);
    uri = lua_tolstring(L, -1, &len);
    amf_buf_append_u16(buf, (uint16_t)len);
    if (len > 0) {
        amf_buf_append(buf, uri, (uint16_t)len);
    printf("target: %s\n", uri);
    }
    lua_pop(L, 1);

    /* response uri */
    lua_rawgeti(L, -1, 2);
    uri = lua_tolstring(L, -1, &len);
    amf_buf_append_u16(buf, (uint16_t)len);
    if (len > 0) {
        amf_buf_append(buf, uri, (uint16_t)len);
    printf("response: %s\n", uri);
    }
    lua_pop(L, 1);


    /* content length */
    amf_buf_append_u32(buf, (uint32_t)0);

    lua_rawgeti(L, -1, 3);
    lua_newtable(L);
    amf0_encode(L, buf, (ver == 3), -2, lua_gettop(L));
    lua_pop(L, 2); /* pops the obj and ref table */
}


void amf_encode_msg(lua_State *L, amf_buf *buf)
{
    luaL_checktype(L, -1, LUA_TTABLE);

    int i = 0, ver;
    while(i++ < 3) {
        lua_rawgeti(L, -1, i);

        switch (i) {
        case 1: {
            ver = lua_tonumber(L, -1);
            amf_buf_append_u16(buf, ver);
            break;
        }

        case 2: {
            /* headers */
            if (lua_isnil(L, -1)) {
                amf_buf_append_u16(buf, 0);
            } else if (lua_istable(L, -1)) {
                int hc = lua_objlen(L, -1);
                amf_buf_append_u16(buf, hc);
                for (int j = 1; j <= hc; j++) {
                    lua_rawgeti(L, -1, j);
                    encode_hdr(L, buf, ver);
                    lua_pop(L, 1);
                }
            } else {
                luaL_error(L, "invalid amf msg header container structure, must be a table or nil");
            }
            break;
        }

        case 3: {
            if (lua_isnil(L, -1)) {
                amf_buf_append_u16(buf, 0);
            } else if (lua_istable(L, -1)) {
                int bc = lua_objlen(L, -1);
                amf_buf_append_u16(buf, bc);
                for (int j = 1; j <= bc; j++) {
                    lua_rawgeti(L, -1, j);
                    encode_body(L, buf, ver);
                    lua_pop(L, 1);
                }
            } else {
                luaL_error(L, "invalid amf msg body container structure, must be a table or nil");
            }
            break;
        }
        }

        lua_pop(L, 1);
    }


}
