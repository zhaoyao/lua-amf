#ifndef AMF_ENDINESS

#define AMF_ENDINESS

#define reverse_if_little_endian(b, len) do {   \
    int           test;                         \
    char          *s, *e, tmp;                  \
    size_t        nlen;                         \
    unsigned char *testp;                       \
    test = 1;                                   \
    testp = (unsigned char *)&test;             \
    if (testp[0] == 1) {                        \
        s = (char *)b;                          \
        e = s + len - 1;                        \
        nlen = len / 2;                         \
        while(nlen--) {                         \
            tmp = *s;                           \
            *s = *e;                            \
            *e = tmp;                           \
            s++;                                \
            e--;                                \
        }                                       \
    }                                           \
} while(0)

#endif /* end of include guard: AMF_ENDINESS */
