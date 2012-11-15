#ifndef DEBUG_H

#define DEBUG_H

#include <lua.h>
#include <stdio.h>
#include <assert.h>

#define dd(...) do {\
    fprintf(stderr, __VA_ARGS__);\
    fprintf(stderr, "\n");\
} while(0)


#define lua_dump_table(L, idx) do {                                             \
    lua_pushnil(L);                                                             \
    printf("{\n");                                                              \
    assert(lua_type(L, idx) == LUA_TTABLE);                                     \
    while (lua_next(L, idx) != 0) {                                             \
        int kt = lua_type(L, -2);                                               \
        int vt = lua_type(L, -1);                                               \
        printf("    ");                                                         \
        switch (kt) {                                                           \
            case LUA_TSTRING: {                                                 \
                printf("string(%s)", lua_tostring(L, -2));                      \
                break;                                                          \
            }                                                                   \
            case LUA_TNUMBER: {                                                 \
                printf("number(%g)", lua_tonumber(L, -2));                      \
                break;                                                          \
            }                                                                   \
            case LUA_TBOOLEAN: {                                                \
                printf("boolean(%s)", lua_toboolean(L, -2) ? "true" : "false"); \
                break;                                                          \
            }                                                                   \
            default: {                                                          \
                printf("unprintable[%s]\n", lua_typename(L, kt));               \
            }                                                                   \
        }                                                                       \
        printf(" = ");                                                          \
        switch (vt) {                                                           \
            case LUA_TSTRING: {                                                 \
                printf("string(%s)", lua_tostring(L, -1));                      \
                break;                                                          \
            }                                                                   \
            case LUA_TNUMBER: {                                                 \
                printf("number(%g)", lua_tonumber(L, -1));                      \
                break;                                                          \
            }                                                                   \
            case LUA_TBOOLEAN: {                                                \
                printf("boolean(%s)\n", lua_toboolean(L, -1) ? "true" : "false");\
                break;                                                          \
            }                                                                   \
            default: {                                                          \
                printf("unprintable[%s]\n", lua_typename(L, vt));               \
            }                                                                   \
        }                                                                       \
        printf("\n");                                                           \
        lua_pop(L, 1);                                                          \
    }                                                                           \
    printf("}\n");                                                              \
} while(0)

#define lua_dump(L,idx) do {                                                    \
    int __l_t = lua_type(L, idx);                                               \
    int __l_top = lua_gettop(L);                                                \
    int __l_r_idx = -__l_top + (idx) - 1;                                       \
    switch (__l_t) {                                                            \
        case LUA_TSTRING: {                                                     \
            printf("L[%d:%d]: string(%s)\n",                                    \
                    idx, __l_r_idx, lua_tostring(L, idx));                      \
            break;                                                              \
        }                                                                       \
        case LUA_TNUMBER: {                                                     \
            printf("L[%d:%d]: number(%g)\n",                                    \
                    idx, __l_r_idx, lua_tonumber(L, idx));                      \
            break;                                                              \
        }                                                                       \
        case LUA_TTABLE: {                                                      \
            printf("[%d:%d]: table\n", idx, __l_r_idx);                         \
            lua_dump_table(L, idx);                                             \
            break;                                                              \
        }                                                                       \
        default: {                                                              \
                printf("unprintable[%s]\n", luaL_typename(L, idx));             \
        }                                                                       \
        printf("\n");                                                           \
    }                                                                           \
    printf("}\n");                                                              \
} while(0)

#define lua_dump_stack(L, label) do {                                           \
    int __l_i;                                                                  \
    printf("--------------------------[%s start]--------------------\n", label);\
    for (__l_i = lua_gettop(L); __l_i >= 1; __l_i--) {                          \
        lua_dump(L, __l_i);                                                     \
    }                                                                           \
    printf("--------------------------[%s end]---------------------\n", label); \
} while (0)

#endif /* end of include guard: DEBUG_H */
