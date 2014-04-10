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

typedef struct {
    const char *name;
    void *(*open)(const char *host, int port, struct timeval timeout);
    void (*close)(void *);
    VCL_STRING (*get)(struct ws *ws, void *, VCL_STRING);
    VCL_BOOL (*add)(void *, VCL_STRING, VCL_STRING);
    VCL_VOID (*set)(void *, VCL_STRING, VCL_STRING);
    VCL_BOOL (*exists)(void *, VCL_STRING);
    VCL_BOOL (*delete)(void *, VCL_STRING);
    VCL_VOID (*expire)(void *, VCL_STRING, VCL_DURATION);
    VCL_INT (*increment)(void *, VCL_STRING);
    VCL_INT (*decrement)(void *, VCL_STRING);
} vmod_key_store_driver;

void vmod_key_store_register_driver(const vmod_key_store_driver * const);

#endif /* !KEY_STORE_DRIVER_H */
