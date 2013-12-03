#include "amf_codec.h"

#include "endiness.h"

#include <string.h>
#include <stdint.h>
#include <math.h>
#include <assert.h>

#include <lua.h>
#include <lauxlib.h>

#define abs_idx(L, i) do { if(idx < 0) idx = lua_gettop(L) + idx + 1; } while(0)

static inline void
save_ref(lua_State *L, int idx, int ridx)
{
    int ref;

    /* retrieve current ref count */
    lua_rawgeti(L, ridx, 1);
    ref = lua_tointeger(L, -1);
    lua_pop(L, 1);

    /* save object to the ref table */
    lua_pushvalue(L, idx);
    lua_pushinteger(L, ref);
    lua_rawset(L, ridx);

    /* increase the ref count */
    lua_pushinteger(L, ref + 1);
    lua_rawseti(L, ridx, 1);
}

/*
 * test if the giving lua table is a dense array
 * return array length if true else -1
 */
static int
strict_array_length(lua_State *L, int idx)
{
    luaL_checkstack(L, 2, "No more extra lua stack");
    abs_idx(L, idx);
    int len = 0;

    for (lua_pushnil(L); lua_next(L, idx); lua_pop(L, 1)) {
        len++;

        if (lua_type(L, -2) != LUA_TNUMBER || lua_tointeger(L, -2) != len) {
            lua_pop(L, 2);
            return -1;
        }
    }

    return len;
}

static void
amf0_encode_string(amf_buf *b, const char *s, size_t len)
{
    uint16_t u16;
    uint32_t u32;

    if (len < UINT16_MAX) {
        u16 = (uint16_t)len;

        amf_buf_append_char(b, AMF0_STRING);
        amf_buf_append_u16(b, u16);
        amf_buf_append(b, s, u16);

    } else { // long string
        if (len > UINT32_MAX) {
            len = UINT32_MAX;
        }
        u32 = (uint32_t)len;

        amf_buf_append_char(b, AMF0_L_STRING);
        amf_buf_append_u32(b, u32);
        amf_buf_append(b, s, u32);
    }
}

static void
amf0_encode_number(amf_buf *buf, lua_Number d)
{
    amf_buf_append_double(buf, d);
}

static int
amf0_encode_ref(amf_buf *buf, lua_State *L, int idx, int ridx)
{
    int ref;

    /* lookup in the ref table */
    lua_pushvalue(L, idx);
    lua_rawget(L, ridx);
    ref = (lua_isnumber(L, -1) ? lua_tonumber(L, -1) : -1);
    lua_pop(L, 1);

    if (ref >= 0) {
        amf_buf_append_char(buf, AMF0_REFERENCE);
        if (ref > UINT16_MAX) {
            luaL_error(L, "amf0 reference overflow");
        }
        amf_buf_append_u16(buf, ref);

    } else {
        save_ref(L, idx, ridx);

    }

    return ref;
}

static void
amf0_encode_table_as_array(amf_buf *buf, lua_State *L, int idx, int ridx, int len)
{
    amf_buf_append_char(buf, AMF0_STRICT_ARRAY);
    amf_buf_append_u32(buf, (uint32_t)len); // array count

    int i;
    for (i = 1; i <= len; i++) {
        lua_pushinteger(L, i);
        lua_gettable(L, idx);

        amf0_encode(L, buf, 0, -1, ridx);
        lua_pop(L, 1);
    }

}

/* TODO: typed object support */
static void
amf0_encode_table_as_object(amf_buf *buf, lua_State *L, int idx, int ridx)
{
    size_t key_len;
    const char *key;
    amf_buf_append_char(buf, AMF0_OBJECT);

    for (lua_pushnil(L); lua_next(L, idx); lua_pop(L, 1)) {
        switch (lua_type(L, -2)) {
        case LUA_TNUMBER:
            lua_pushvalue(L, -2);
            key = lua_tolstring(L, -1, &key_len);
            amf_buf_append_u16(buf, (uint16_t)key_len);
            amf_buf_append(buf, key, key_len);
            lua_pop(L, 1);

            break;

        case LUA_TSTRING:
            key = lua_tolstring(L, -2, &key_len);
            amf_buf_append_u16(buf, (uint16_t)key_len);
            amf_buf_append(buf, key, key_len);

            break;

        default: continue;
        }
        amf0_encode(L, buf, 0, -1, ridx);
    }

    amf_buf_append_u16(buf, (uint16_t)0);
    amf_buf_append_char(buf, AMF0_END_OF_OBJECT);

}

void
amf0_encode(lua_State *L, amf_buf *buf, int avmplus, int idx, int ridx)
{
    int         array_len, old_top, ref;
    size_t      len;
    const char  *str;

    abs_idx(L, idx);
    abs_idx(L, ridx);

    old_top = lua_gettop(L);

    switch (lua_type(L, idx)) {
    case LUA_TNIL:
        amf_buf_append_char(buf, AMF0_NULL);
        break;

    case LUA_TBOOLEAN:
        amf_buf_append_char(buf, AMF0_BOOLEAN);
        amf_buf_append_char(buf, lua_toboolean(L, idx) ? 1 : 0);
        break;

    case LUA_TNUMBER:
        amf_buf_append_char(buf, AMF0_NUMBER);
        amf0_encode_number(buf, lua_tonumber(L, idx));
        break;

    case LUA_TSTRING:
        str = lua_tolstring(L, idx, &len);
        amf0_encode_string(buf, str, len);
        break;

    case LUA_TTABLE: {
        if (avmplus) {
            lua_newtable(L);
            lua_newtable(L);
            lua_newtable(L);

            int top = lua_gettop(L);

            amf3_encode(L, buf, idx, top-2, top-1, top);

            lua_pop(L, 3);

        } else {
            ref = amf0_encode_ref(buf, L, idx, ridx);
            if (ref >= 0) {
                break;
            }

            array_len = strict_array_length(L, idx);
            if (array_len == 0) {
                amf_buf_append_char(buf, AMF0_NULL);
            } else if (array_len > 0) {
                amf0_encode_table_as_array(buf, L, idx, ridx, array_len);
            } else {
                amf0_encode_table_as_object(buf, L, idx, ridx);
            }
        }
        break;
    }
    }

    assert(lua_gettop(L) == old_top);
}

#define amf0_decode_string(L, c, bits) do {                 \
    uint##bits##_t len = 0;                                 \
    int nbits = bits;                                       \
    amf_cursor_need(c, bits/8);                             \
    len = c->p[0] << 8 | c->p[1];                           \
    int i;                                                  \
    for(i = 0; nbits >= 0; nbits -= 8, i++) {               \
        len |= (uint##bits##_t)c->p[i] << (nbits - 8);      \
    }                                                       \
    amf_cursor_consume(c, bits/8);                          \
    amf_cursor_need(c, len);                                \
    lua_pushlstring(L, c->p, len);                          \
    amf_cursor_consume(c, len);                             \
} while(0)

static void
amf0_decode_remember_ref(lua_State *L, int idx, int ridx)
{
    lua_pushvalue(L, idx);
    luaL_ref(L, ridx);
}

void
amf0_decode_to_lua_table(lua_State *L, amf_cursor *c, int ridx)
{
    lua_newtable(L);
    amf0_decode_remember_ref(L, -1, ridx);

    for (;;) {
        amf0_decode_string(L, c, 16);
        amf_cursor_checkerr(c);


        amf_cursor_need(c, 1);
        if (c->p[0] == AMF0_END_OF_OBJECT || lua_objlen(L, -1) == 0) {
            lua_pop(L, 1); // the empty string
            amf_cursor_consume(c, 1);
            break;
        }

        amf0_decode(L, c, ridx);
        amf_cursor_checkerr(c);

        lua_rawset(L, -3);
    }

}

void
amf0_decode(lua_State *L, amf_cursor *c, int ridx)
{
    amf_cursor_need(c, 1);

    switch (c->p[0]) {
    case AMF0_BOOLEAN:
        amf_cursor_need(c, 2);
        lua_pushboolean(L, c->p[1] == 0x01 ? 1 : 0);
        amf_cursor_consume(c, 2);

        break;

    case AMF0_NULL:
    case AMF0_UNDEFINED:
        lua_pushnil(L);
        amf_cursor_consume(c, 1);

        break;

    case AMF0_NUMBER:
        amf_cursor_need(c, 9);
        assert(sizeof(double) == 8);

        double d;
        memcpy(&d, c->p + 1, 8);
        reverse_if_little_endian(&d, 8);
        lua_pushnumber(L, d);
        amf_cursor_consume(c, 9);

        break;

    case AMF0_STRING:
        amf_cursor_consume(c, 1);
        amf0_decode_string(L, c, 16);

        break;

    case AMF0_L_STRING:
        amf_cursor_consume(c, 1);
        amf0_decode_string(L, c, 32);

        break;

    case AMF0_OBJECT:
        amf_cursor_consume(c, 1);
        amf0_decode_to_lua_table(L, c, ridx);
        break;

    case AMF0_ECMA_ARRAY:
        /* the property count, basicly 0 */
        amf_cursor_skip(c, 5);
        amf0_decode_to_lua_table(L, c, ridx);
        break;

    case AMF0_STRICT_ARRAY: {
        amf_cursor_need(c, 5);
        uint32_t count = c->p[1] << 24
                         | c->p[2] << 16
                         | c->p[3] << 8
                         | c->p[4];
        amf_cursor_consume(c, 5);

        assert(count <= INT_MAX);
        if (count > INT_MAX) {
            c->err = AMF_CUR_ERR_BADFMT;
            c->err_msg = "strict array elements count overflow";
            return;
        }

        lua_createtable(L, (int)count, 0);
        amf0_decode_remember_ref(L, -1, ridx);

        int i;
        for (i = 1; i <= (int)count; i++) {
            amf0_decode(L, c, ridx);
            amf_cursor_checkerr(c);
            lua_rawseti(L, -2, i);
        }
        break;
    }

    case AMF0_TYPED_OBJECT: {
        amf_cursor_consume(c, 1);

        amf0_decode_string(L, c, 16);

        amf0_decode_to_lua_table(L, c, ridx);

        /* push alias name */
        lua_pushstring(L, "__amf_alias__");
        lua_pushvalue(L, -3);
        lua_rawset(L, -3);

        /* remove alias from stack */
        lua_remove(L, -2);

        break;
    }

    case AMF0_REFERENCE:
        amf_cursor_need(c, 3);
        uint16_t ref = c->p[1] << 8 | c->p[2];
        amf_cursor_consume(c, 3);

        lua_rawgeti(L, ridx, ref + 1);
        if (lua_isnil(L, -1)) {
            c->err = AMF_CUR_ERR_BADFMT;
            c->err_msg = "reference not found";
            return;
        }

        break;

    case AMF0_AVMPLUS: {
        lua_newtable(L);
        lua_newtable(L);
        int sidx = lua_gettop(L);
        amf3_decode(L, c, sidx, ridx, sidx - 1);
        break;
    }

    default:
        c->err = AMF_CUR_ERR_BADFMT;

    }

}

static int
amf3_encode_ref(lua_State *L, amf_buf *buf, int idx, int ridx)
{
    abs_idx(L, idx);

    lua_pushvalue(L, idx);
    lua_rawget(L, ridx);

    int ref = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : -1;
    lua_pop(L, 1);

    if (ref >= 0) {
        if (ref > AMF3_MAX_REFERENCES) {
            luaL_error(L, "amf reference count overflow");
        }

        amf_buf_append_u29(buf, ref << 1);

    } else {
        save_ref(L, idx, ridx);

    }

    return ref;
}

static void
amf3_encode_string(lua_State *L, amf_buf *buf, int idx, int sidx)
{
    size_t len;
    const char *s = lua_tolstring(L, idx, &len);

    if (len > AMF3_MAX_STR_LEN) len = AMF3_MAX_STR_LEN;

    if (len > 0) {
        if (amf3_encode_ref(L, buf, idx, sidx) < 0) {
            amf_buf_append_u29(buf, (len << 1 | 1));
            amf_buf_append(buf, s, len);
        }

    } else {
        amf_buf_append_u29(buf, (0 << 1 | 1));
    }

}

#define class_name_not_empty(class_name) class_name != NULL && strcmp("", class_name) != 0

/**
 * encode the traits info for a lua table.
 * first iterate over the current traits ref table(ridx) to see if we should encode the traits info as reference.
 * if not, encode the whole traits info
 */
static int
amf3_encode_traits(lua_State *L, amf_buf *buf, int traits_table_idx, int sidx, int tidx, const char *class_name)
{
    int ncached = lua_objlen(L, tidx);

    int ref = 1;
    int traits_members = lua_objlen(L, traits_table_idx);

    for (; ref <= ncached; ref++) {

        lua_rawgeti(L, tidx, ref);

        int match = 1;
        int members = lua_objlen(L, -1);

        if (members != traits_members) {
            lua_pop(L, 1);
            continue;
        }

        int i;
        for (i = 1; i <= members; i++) {
            lua_rawgeti(L, traits_table_idx, i);
            lua_rawgeti(L, -2, i);

            if (!lua_rawequal(L, -1, -2)) {
                match = 0;
                lua_pop(L, 2);
                break;
            }

            lua_pop(L, 2);
        }

        lua_pop(L, 1); /* drop the cached traits table */

        if (match) {
            amf_buf_append_u29(buf, ((ref-1) << 2 | 1));
            return ref;
        }
    }

    /* remember the traits */
    lua_pushvalue(L, traits_table_idx);
    lua_rawseti(L, tidx, ref);

    amf_buf_append_u29(buf, 3 | (!(class_name_not_empty(class_name)) ? 8 : 0) | traits_members<<4);
    if (class_name_not_empty(class_name)) {
        printf("Traits Class Name: %s\n", class_name);
        lua_pushstring(L, class_name);
        amf3_encode_string(L, buf, -1, sidx);
        lua_pop(L, 1);
    } else {
        amf_buf_append_u29(buf, 0<<1|1);
    }

    int m;
    for (m = 1; m <= traits_members; m++) {
        lua_rawgeti(L, traits_table_idx, m);
        amf3_encode_string(L, buf, -1, sidx);
        lua_pop(L, 1);
    }

    return -1;
}

static void
amf3_encode_table_as_object(lua_State *L, amf_buf *buf, int idx, int sidx, int oidx, int tidx)
{
    const char *class_name = NULL;
    abs_idx(L, idx);

    amf_buf_append_char(buf, AMF3_OBJECT);

    if (amf3_encode_ref(L, buf, idx, oidx) >= 0) {
        return;
    }

    lua_newtable(L); /* traits table */
    int members = 1;
    for(lua_pushnil(L); lua_next(L, idx); lua_pop(L, 1)) { // get encoded table all keys
        switch (lua_type(L, -2)) {
            case LUA_TNUMBER:
                lua_pushvalue(L, -2);
                lua_tostring(L, -1);
                break;
            case LUA_TSTRING:
                if (strcmp(lua_tostring(L, -2), "__class") == 0) {
                    class_name = lua_tostring(L, -1);
                    printf("Class Name: %s\n", class_name);
                    continue;
                } else {
                    lua_pushvalue(L, -2);
                }
                break;
            default:
                continue;
        }
        lua_rawseti(L, -4, members++); // set all keys into traits table
    }

    amf3_encode_traits(L, buf, lua_gettop(L), sidx, tidx, class_name);
    lua_pop(L, 1); /* drop the traits table */

    for(lua_pushnil(L); lua_next(L, idx); lua_pop(L, 1)) {
        amf3_encode(L, buf, -1, sidx, oidx, tidx);
    }
}

static void
amf3_encode_table_as_array(lua_State *L, amf_buf *buf, int idx, int sidx, int oidx, int tidx, int array_len)
{
    abs_idx(L, idx);
    amf_buf_append_char(buf, AMF3_ARRAY);
    if (amf3_encode_ref(L, buf, idx, oidx) >=0) {
        return;
    }

    amf_buf_append_u29(buf, (array_len << 1) | 1);
    /*Send an empty string to imply no named keys*/
    amf_buf_append_u29(buf, (0 << 1) | 1);

    int i;
    for (i = 1; i <= array_len; i++) {
        lua_rawgeti(L, idx, i);

        amf3_encode(L, buf, -1, sidx, oidx, tidx);
        lua_pop(L, 1);
    }

}

void
amf3_encode(lua_State *L, amf_buf *buf, int idx, int sidx, int oidx, int tidx)
{
    int          old_top, array_len;
    const char  *str;
    size_t       str_len;

    old_top = lua_gettop(L);

    switch (lua_type(L, idx)) {
    case LUA_TNIL: {
        amf_buf_append_char(buf, AMF3_NULL);
        break;
    }

    case LUA_TBOOLEAN: {
        if (lua_toboolean(L, idx)) {
            amf_buf_append_char(buf, AMF3_TRUE);
        } else {
            amf_buf_append_char(buf, AMF3_FALSE);
        }
        break;
    }

    case LUA_TNUMBER: {
        lua_Number n = lua_tonumber(L, idx);
        /* encode as double */
        if (floor(n) != n || n < AMF3_MIN_INT || n > AMF3_MAX_INT) {
            amf_buf_append_char(buf, AMF3_DOUBLE);
            amf_buf_append_double(buf, n);
        } else {
            amf_buf_append_char(buf, AMF3_INTEGER);
            amf_buf_append_u29(buf, (int)n);
        }
        break;
    }

    case LUA_TSTRING: {
        str = lua_tolstring(L, idx, &str_len);
        amf_buf_append_char(buf, AMF3_STRING);
        amf3_encode_string(L, buf, idx, sidx);
        break;
    }

    case LUA_TTABLE:
        array_len = strict_array_length(L, idx);

        if (array_len == 0) {
            amf_buf_append_char(buf, AMF3_NULL);

        } else if (array_len > 0) {
            amf3_encode_table_as_array(L, buf, idx, sidx, oidx, tidx, array_len);

        } else {
            amf3_encode_table_as_object(L, buf, idx, sidx, oidx, tidx);

        }

    }

    assert(lua_gettop(L) == old_top);
}

static void
amf3_decode_u29(amf_cursor *c, uint32_t *v)
{
    unsigned int ofs = 0, res = 0, tmp;
    do {
        amf_cursor_need(c, 1);
        tmp = *c->p;
        if (ofs == 3) {
            res <<= 8;
            res |= tmp & 0xff;
        } else {
            res <<= 7;
            res |= tmp & 0x7f;
        }
        amf_cursor_consume(c, 1);
    } while ((++ofs < 4) && (tmp & 0x80));
    *v = res;
}

#define amf3_decode_ref(L, c, ref, ridx) lua_rawgeti(L, ridx, (ref) + 1)
#define amf3_is_ref(i) ((i) & 1) == 0
#define remember_object(L, idx, ridx) lua_pushvalue(L, idx); luaL_ref(L, ridx)

static void
amf3_decode_str(lua_State *L, amf_cursor *c, int sidx) {
    uint32_t ref, len;
    amf3_decode_u29(c, &ref);
    amf_cursor_checkerr(c);

    if (!amf3_is_ref(ref)) {
        len = ref >> 1;
        if (len > 0) {
            amf_cursor_need(c, len);
            lua_pushlstring(L, c->p, len);
            amf_cursor_consume(c, len);

            remember_object(L, -1, sidx);
        } else {
            lua_pushliteral(L, "");
        }
    } else {
        amf3_decode_ref(L, c, ref >> 1, sidx);
    }

}

#define amf3_decode_double(c, n) do {\
    amf_cursor_need(c, 8+n);\
    double d;\
    memcpy(&d, c->p+n, 8);\
    reverse_if_little_endian(&d, 8);\
    lua_pushnumber(L, d);\
    amf_cursor_consume(c, 8+n);\
} while(0)

static void
amf3_decode_date(lua_State *L, amf_cursor *c, int oidx) {
    amf_cursor_consume(c, 1);
    uint32_t ref;
    amf3_decode_u29(c, &ref);
    amf_cursor_checkerr(c);

    if (!amf3_is_ref(ref)) {
        amf3_decode_double(c, 0);
    } else {
        amf3_decode_ref(L, c, ref >> 1, oidx);
    }    
}

static void
amf3_decode_array(lua_State *L, amf_cursor *c, int sidx, int oidx, int tidx) {
    uint32_t ref, len;
    amf_cursor_consume(c, 1);
    amf3_decode_u29(c, &ref);
    amf_cursor_checkerr(c);

    if (!amf3_is_ref(ref)) {
        len = ref >> 1;
        amf_cursor_skip(c, 1); // empty class name

        lua_createtable(L, len, 0);

        remember_object(L, -1, oidx);

        unsigned int i;
        for (i = 1; i <= len; i++) {
            amf3_decode(L, c, sidx, oidx, tidx);
            amf_cursor_checkerr(c);
            lua_rawseti(L, -2, i);
        }

    } else {
        amf3_decode_ref(L, c, ref >> 1, oidx);
    }   
}

static void
amf3_decode_object(lua_State *L, amf_cursor *c, int sidx, int oidx, int tidx) {
    uint32_t ref;
    amf_cursor_consume(c, 1);
    amf3_decode_u29(c, &ref);
    amf_cursor_checkerr(c);

    if (!amf3_is_ref(ref)) {
        uint32_t traits_ext = ref;
        const char *class_name = NULL;
        unsigned int members = 0, dynamic = 0, external = 0;

        /*
         * read the traits info
         * after reading, the stack top is the traits table
         */
        if ((traits_ext & 3) != 1) {
            members = traits_ext >> 4;
            dynamic = (traits_ext & 8) == 8;
            external = (traits_ext & 4) == 4;

            amf3_decode_str(L, c, sidx);
            amf_cursor_checkerr(c);
            class_name = lua_tostring(L, -1);
            lua_pop(L, 1);

            if (external) {
                //TODO
                assert(0);

            } else {
                lua_createtable(L, members, 2 + (class_name_not_empty(class_name) ? 1 : 0));

                unsigned int i;
                for (i = 1; i <= members; i++) {
                    amf3_decode_str(L, c, sidx);
                    amf_cursor_checkerr(c);
                    lua_rawseti(L, -2, i);
                }

                lua_pushliteral(L, "dynamic");
                lua_pushinteger(L, dynamic);
                lua_rawset(L, -3);

                lua_pushliteral(L, "external");
                lua_pushinteger(L, external);
                lua_rawset(L, -3);

                if (class_name_not_empty(class_name)) {
                    lua_pushliteral(L, "__class");
                    lua_pushstring(L, class_name);
                    lua_rawset(L, -3);
                }

                /* remember the traits table */
                lua_pushvalue(L, -1);
                luaL_ref(L, tidx);
            }

            remember_object(L, -1, tidx);

        } else {
            amf3_decode_ref(L, c, traits_ext >> 2, tidx);
            amf_cursor_checkerr(c);

            assert(lua_istable(L, -1));

            members = lua_objlen(L, -1);

            lua_pushliteral(L, "external");
            lua_rawget(L, -2);
            external = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_pushliteral(L, "dynamic");
            lua_rawget(L, -2);
            dynamic = lua_tointeger(L, -1);
            lua_pop(L, 1);

            lua_pushliteral(L, "__class");
            lua_rawget(L, -2);
            class_name = lua_tostring(L, -1);
            lua_pop(L, 1);
        }

        if (external) {
            //TODO external support
            assert(0);

        } else {
            lua_createtable(L, 0, members + (class_name_not_empty(class_name) ? 1 : 0));
            remember_object(L, -1, oidx);

            if (class_name_not_empty(class_name)) {
                lua_pushliteral(L, "__class");
                lua_pushstring(L, class_name);
                lua_rawset(L, -3);
            }

            unsigned int i;
            for (i = 1; i <= members; i++) {
                lua_rawgeti(L, -2, i);

                amf3_decode(L, c, sidx, oidx, tidx);
                amf_cursor_checkerr(c);

                lua_rawset(L, -3);
            }

            lua_remove(L, -2); /* drop trait table */
        }

    } else {
        amf3_decode_ref(L, c, ref >> 1, oidx);
    }
}

void amf3_decode(lua_State *L, amf_cursor *c,  int sidx, int oidx, int tidx)
{
    amf_cursor_need(c, 1);

    int top = lua_gettop(L);

    switch (c->p[0]) {
        case AMF3_UNDEFINED:
        case AMF3_NULL:
            lua_pushnil(L);
            amf_cursor_consume(c, 1);
            break;

        case AMF3_TRUE:
            lua_pushboolean(L, 1);
            amf_cursor_consume(c, 1);
            break;

        case AMF3_FALSE:
            lua_pushboolean(L, 0);
            amf_cursor_consume(c, 1);
            break;

        case AMF3_INTEGER: {
            amf_cursor_consume(c, 1);
            int32_t i;
            amf3_decode_u29(c, (uint32_t *)&i);
            amf_cursor_checkerr(c);
            i = (i << 3) >> 3;
            lua_pushinteger(L, i);
            break;

        }

        case AMF3_DATE: {
            amf3_decode_date(L, c, oidx);
            break;
        }

        case AMF3_DOUBLE: {
            amf3_decode_double(c, 1);
            break;
        }

        case AMF3_STRING:
            amf_cursor_consume(c, 1);
            amf3_decode_str(L, c, sidx);
            break;

        case AMF3_XMLDOC:
        case AMF3_XML:
            amf_cursor_consume(c, 1);
            amf3_decode_str(L, c, oidx);
            break;

        case AMF3_ARRAY: {
            amf3_decode_array(L, c, sidx, oidx, tidx);
            break;
        }

        case AMF3_BYTEARRAY: {
            amf_cursor_consume(c, 1);
            amf3_decode_str(L, c, oidx);
            break;
        }

        case AMF3_OBJECT: {
            amf3_decode_object(L, c, sidx, oidx, tidx);
            break;
        }
        default:
            luaL_error(L, "unsupported type: %d", c->p[0]);
    }

    assert(lua_gettop(L) - top == 1);
}

