#ifndef KEY_STORE_DRIVER_H

# define KEY_STORE_DRIVER_H 1

# ifdef DEBUG
#  include <stdarg.h>
#  define debug(fmt, ...) \
    do { \
        FILE *fp;\
        fp = fopen("/tmp/keystore.log", "a"); \
        fprintf(fp, fmt "\n", ## __VA_ARGS__); \
        fclose(fp); \
    } while (0);
# else
#  define debug(fmt, ...)
# endif /* DEBUG */

# define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
# define STR_LEN(str) (ARRAY_SIZE(str) - 1)
# define STR_SIZE(str) (ARRAY_SIZE(str))

typedef struct {
    const char *name;
    void *(*open)(const char *host, int port, struct timeval timeout);
    void (*close)(void *);
    VCL_STRING (*get)(struct ws *, void *, VCL_STRING);
    VCL_BOOL (*add)(void *, VCL_STRING, VCL_STRING);
    VCL_VOID (*set)(void *, VCL_STRING, VCL_STRING);
    VCL_BOOL (*exists)(void *, VCL_STRING);
    VCL_VOID (*delete)(void *, VCL_STRING);
    VCL_VOID (*expire)(void *, VCL_STRING, VCL_DURATION);
    VCL_INT (*increment)(void *, VCL_STRING);
    VCL_INT (*decrement)(void *, VCL_STRING);
    VCL_STRING (*raw)(struct ws *, void *, VCL_STRING);
} vmod_keystore_driver_imp;

void vmod_keystore_register_driver(const vmod_keystore_driver_imp * const);

#endif /* !KEY_STORE_DRIVER_H */
