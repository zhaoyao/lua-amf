
#include "amf_codec.h"
#include "amf_remoting.h"

#include "endiness.h"

#include <lua.h>
#include <lauxlib.h>

#include <stdint.h>



#define check_amf_ver(ver, i) do {                                  \
    if(ver != AMF_VER0 && ver != AMF_VER3) {                        \
        if (i > 0) {                                                \
            luaL_argerror(L, i, "amf version not supported");         \
        } else {                                                    \
            luaL_error(L, "amf version not supported: %d", ver);    \
        }                                                           \
    }                                                               \
} while(0)


int
lua_amf_encode(lua_State *L)
{
    int ver, freebuf = 1;
    amf_buf *buf;

    if (lua_isnumber(L, 1)) {
        ver = luaL_checkint(L, 1);
        check_amf_ver(ver, 1);
        luaL_checkany(L, 2);

        buf = amf_buf_init(NULL);

    } else {
        buf = luaL_checkudata(L, 1, "amf_buffer");
        ver = luaL_checkint(L, 2);
        check_amf_ver(ver, 2);
        luaL_checkany(L, 3);
        freebuf = 0;

    }


    if (buf == NULL) {
        lua_pushnil(L);
        return 1;
    }

    int base = lua_gettop(L);

    if (ver == AMF_VER0) {
        lua_newtable(L);
        amf0_encode(L, buf, 0, base, base+1);

    } else {
        lua_newtable(L);
        lua_newtable(L);
        lua_newtable(L);
        amf3_encode(L, buf, base, base+1, base+2, base+3);

    }

    if (freebuf) {
        lua_pushlstring(L, buf->b, buf->len);
        amf_buf_free(buf);
        return 1;

    } else {
        return 0;

    }
}

#define min(x,y) ((x)>(y) ? (y) : (x))
int
lua_amf_decode(lua_State *L)
{
    int          ver;
    int          top;
    size_t       pos;
    size_t       buf_size;
    const char  *buf;
    amf_cursor  *cur;

    ver = luaL_checkint(L, 1);
    check_amf_ver(ver, 1);

    buf = luaL_checklstring(L, 2, &buf_size);

    pos = luaL_optint(L, 3, 0);
    luaL_argcheck(L, pos >= 0, 3,
                  "input offset may not be negative");

    buf_size = min(luaL_optint(L, 4, buf_size), (int)buf_size);
    luaL_argcheck(L, buf_size >= pos, 4, "input buf overflow");

    top = lua_gettop(L);
    cur = amf_cursor_new(buf, buf_size);

    if (ver == AMF_VER0) {
        lua_newtable(L);
        amf0_decode(L, cur, top+1);

    } else {
        lua_newtable(L);
        lua_newtable(L);
        lua_newtable(L);
        amf3_decode(L, cur, top + 1, top + 2, top + 3);

    }

    if (cur->err) {
        lua_pushnil(L);
        lua_pushstring(L, cur->err_msg);

    } else {
        //obj is at top of the stack
        lua_pushnil(L);
    }

    lua_pushinteger(L, buf_size - cur->left);
    amf_cursor_free(cur);

    return 3;
}

int
lua_amf_decode_msg(lua_State *L)
{
    size_t len;
    const char *buf = luaL_checklstring(L, 1, &len);

    size_t offset = luaL_optint(L, 2, 0);
    luaL_argcheck(L, offset >= 0 && offset < len, 2, "invalid buffer offset");

    size_t actual_len = luaL_optint(L, 3, len);
    luaL_argcheck(L, actual_len > 0 && offset + actual_len <= len, 2, "invalid buffer length");

    amf_cursor *c = amf_cursor_new(buf + offset, actual_len);
    if (c == NULL) {
        luaL_error(L, "cursor creation failed");
    }

    amf_decode_msg(L, c);

    if (c->err) {
        lua_pushnil(L);
        lua_pushstring(L, c->err_msg);
        return 2;
    }

    return 1;
}

int
lua_amf_encode_msg(lua_State *L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    amf_buf *buf = amf_buf_init(NULL);

    amf_encode_msg(L, buf);

    lua_pushlstring(L, buf->b, buf->len);

    return 1;
}

static int
lua_amf_new_buffer(lua_State *L)
{
    amf_buf *b = lua_newuserdata(L, sizeof(struct amf_buf));
    if (b == NULL) return 0;

    amf_buf_init(b);

    luaL_getmetatable(L, "amf_buffer");
    lua_setmetatable(L, -2);

    return 1;
}

static int
lua_amf_buffer_free(lua_State *L)
{
    amf_buf *b = lua_newuserdata(L, sizeof(struct amf_buf));
    if (b != NULL) {
        amf_buf_free(b);
    }

    return 0;
}

static int
lua_amf_buffer_write_ushort(lua_State *L)
{
    amf_buf *b = luaL_checkudata(L, 1, "amf_buffer");
    unsigned short s = (unsigned short)luaL_checkint(L, 2);

    amf_buf_append_u16(b, s);

    return 0;
}

static int
lua_amf_buffer_raw_string(lua_State *L)
{
    amf_buf *b = luaL_checkudata(L, 1, "amf_buffer");
    lua_pushlstring(L, b->b, b->len);
    return 1;
}

static int
lua_amf_buffer_tostring(lua_State *L)
{
    amf_buf *b = luaL_checkudata(L, 1, "amf_buffer");
    lua_pushfstring(L, "<buffer len:%d free:%d>", (int)b->len, (int)b->free);

    return 1;
}

static int
lua_amf_buffer_write_int32(lua_State *L)
{
    amf_buf *b = luaL_checkudata(L, 1, "amf_buffer");
    int32_t i = luaL_checkint(L, 2);

    amf_buf_append_u32(b, i);

    return 0;
}

static int
lua_amf_buffer_write_str(lua_State *L)
{
    amf_buf *b = luaL_checkudata(L, 1, "amf_buffer");
    size_t len;
    const char *str = luaL_checklstring(L, 2, &len);

    printf("string len: %d\n", (uint16_t)len);
    amf_buf_append_u16(b, (uint16_t)len);
    amf_buf_append(b, str, len);

    return 0;
}

static int
lua_amf_buffer_write_uchar(lua_State *L)
{
    amf_buf *b = luaL_checkudata(L, 1, "amf_buffer");
    unsigned char c = (unsigned char)luaL_checkint(L, 2);

    amf_buf_append_char(b, c);

    return 0;
}

#define lib_func(name) { #name, lua_amf_##name }

const struct luaL_Reg amf_lib[] = {
    lib_func(encode),
    lib_func(decode),
    lib_func(decode_msg),
    lib_func(encode_msg),
    lib_func(new_buffer),
    { NULL, NULL }
};

const struct luaL_Reg amf_buf_lib[] = {
    { "write_uchar",  lua_amf_buffer_write_uchar },
    { "write_ushort", lua_amf_buffer_write_ushort },
    { "write_int32",  lua_amf_buffer_write_int32 },
    { "write_str",    lua_amf_buffer_write_str },
    { "raw_string",   lua_amf_buffer_raw_string },
    { "__tostring",   lua_amf_buffer_tostring },
    { "__gc",         lua_amf_buffer_free },
    { NULL, NULL}
};


LUALIB_API int
luaopen_amf_codec(lua_State* L)
{
    luaL_newmetatable(L, "amf_buffer");
    lua_pushstring(L, "__index");
    lua_pushvalue(L, -2);
    lua_settable(L, -3);
    luaL_openlib(L, NULL, amf_buf_lib, 0);

    luaL_register(L, "amf_codec", amf_lib);

    /*
    lua_pushliteral(L, "undefined");
    lua_pushlightuserdata(L, &AMF_UNDEFINED);
    lua_settable(L, -3);
    */

    lua_pushliteral(L, "AMF0");
    lua_pushinteger(L, AMF_VER0);
    lua_settable(L, -3);

    lua_pushliteral(L, "AMF3");
    lua_pushinteger(L, AMF_VER3);
    lua_settable(L, -3);

    lua_pushliteral(L, "MAX_INT");
    lua_pushinteger(L, AMF3_MAX_INT);
    lua_settable(L, -3);

    lua_pushliteral(L, "MIN_INT");
    lua_pushinteger(L, AMF3_MIN_INT);
    lua_settable(L, -3);

    return 1;
}
