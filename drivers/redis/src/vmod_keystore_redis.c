#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "vrt.h"
#include "cache/cache.h"
#include "vcc_if.h"
#include "keystore_driver.h"

#include <hiredis.h>

struct vmod_keystore_redis_data_t {
    unsigned magic;
#define REDIS_MAGIC 0x0066feff
    int port;
    char *host;
    struct timeval tv;
};

static pthread_key_t key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

static void make_key(void)
{
    pthread_key_create(&key, (void (*)(void *)) redisFree);
}

static void *vmod_keystore_redis_open(const char *host, int port, struct timeval tv)
{
    struct vmod_keystore_redis_data_t *d;

    ALLOC_OBJ(d, REDIS_MAGIC);
    AN(d);
    d->port = port;
    d->host = strdup(host);
    d->tv = tv;

    return d;
}

static void vmod_keystore_redis_close(void *c)
{
    struct vmod_keystore_redis_data_t *d;

    d = (struct vmod_keystore_redis_data_t *) c;
    CHECK_OBJ_NOTNULL(d, REDIS_MAGIC);
//     AN(d->host);
    free(d->host);
    FREE_OBJ(d);
}

static redisContext *_redis_do_connect(struct vmod_keystore_redis_data_t *d)
{
    int tv_set;

    /* caller is responsible of CHECK_OBJ_NOTNULL(d, REDIS_MAGIC) */
    tv_set = 0 != d->tv.tv_sec && 0 != d->tv.tv_usec;
    if (-1 == d->port) {
        if (tv_set) {
            return redisConnectUnixWithTimeout(d->host, d->tv);
        } else {
            return redisConnectUnix(d->host);
        }
    } else {
        if (tv_set) {
            return redisConnectWithTimeout(d->host, d->port, d->tv);
        } else {
            return redisConnect(d->host, d->port);
        }
    }
//     if (NULL != d->c && d->c->err) {
// //         VSLb(ctx->vsl, SLT_Error, "redis connection error: %s", c->errstr);
//     }
}

static int _redis_do_string_command(struct ws *ws, void *c, int *output_type, char **output_value, const char *command, ...)
{
    int ret;
    va_list ap;
    redisReply *r;
    redisContext *ctxt;
    struct vmod_keystore_redis_data_t *d;

    d = (struct vmod_keystore_redis_data_t *) c;
    CHECK_OBJ_NOTNULL(d, REDIS_MAGIC);
    ret = 1;
    va_start(ap, command);
    if (NULL == (ctxt = (redisContext *) pthread_getspecific(key))) {
        ctxt = _redis_do_connect(d);
        AN(ctxt);
        pthread_setspecific(key, ctxt);
    }
//     r = redisvCommand(ctxt, command, ap);
    redisvAppendCommand(ctxt, command, ap);
    va_end(ap);
    if (REDIS_OK == redisGetReply(ctxt, (void **) &r)) {
        AN(r);
        switch (*output_type = r->type) {
            case REDIS_REPLY_NIL:
                *output_value = NULL;
                break;
            case REDIS_REPLY_INTEGER:
                *output_value = WS_Printf(ws, "%lld", r->integer); /* NOTE: WS_Printf was introduced lately (after 4.0.0-tp2) */
                break;
            case REDIS_REPLY_ERROR:
                ret = 0;
                /* no break here */
            case REDIS_REPLY_STRING:
            case REDIS_REPLY_STATUS:
                *output_value = WS_Copy(ws, r->str, r->len + 1);
                break;
            default:
               *output_value = NULL;
                break;
        }
    } else {
        ret = 0;
        *output_value = NULL;
    }
    freeReplyObject(r);

    return ret;
}

static VCL_STRING vmod_keystore_redis_get(struct ws *ws, void *c, VCL_STRING key)
{
    char *ovalue;
    int ret, otype;

    ret = _redis_do_string_command(ws, c, &otype, &ovalue, "GET %s", key); /* nil when key does not exist */
    AN(ret);

    return ovalue;
}

static int _redis_do_int_command(void *c, int *output_type, void *output_value, const char *command, ...)
{
    int ret;
    va_list ap;
    redisReply *r;
    redisContext *ctxt;
    struct vmod_keystore_redis_data_t *d;

    d = (struct vmod_keystore_redis_data_t *) c;
    CHECK_OBJ_NOTNULL(d, REDIS_MAGIC);
    ret = 1;
    va_start(ap, command);
    if (NULL == (ctxt = (redisContext *) pthread_getspecific(key))) {
        ctxt = _redis_do_connect(d);
        AN(ctxt);
        pthread_setspecific(key, ctxt);
    }
//     r = redisvCommand(ctxt, command, ap);
    redisvAppendCommand(ctxt, command, ap);
    va_end(ap);
    if (REDIS_OK == redisGetReply(ctxt, (void **) &r)) {
        AN(r);
        switch (*output_type = r->type) {
            case REDIS_REPLY_NIL:
                *((int *) output_value) = 0;
                break;
            case REDIS_REPLY_INTEGER:
                *((int *) output_value) = r->integer;
                break;
            case REDIS_REPLY_STATUS:
                *((int *) output_value) = 0 == strcmp(r->str, "OK");
                break;
            case REDIS_REPLY_ERROR:
                ret = 0;
                break;
            default:
                output_value = NULL;
                break;
        }
    } else {
        ret = 0;
        output_value = NULL;
    }
    freeReplyObject(r);

    return ret;
}

static VCL_BOOL vmod_keystore_redis_add(void *c, VCL_STRING key, VCL_STRING value)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "SETNX %s %s", key, value);
    AN(ret);

    // TODO: retun FALSE if key already exists
    return REDIS_REPLY_INTEGER == otype && 1 == ovalue;
}

static VCL_VOID vmod_keystore_redis_set(void *c, VCL_STRING key, VCL_STRING value)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "SET %s %s", key, value);
    AN(ret);
    AN(REDIS_REPLY_STATUS == otype && 1 == ovalue);
}

static VCL_BOOL vmod_keystore_redis_exists(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "EXISTS %s", key);
    AN(ret);
    AN(REDIS_REPLY_INTEGER == otype);

    return ovalue;
}

static VCL_VOID vmod_keystore_redis_delete(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "DEL %s", key);
    AN(ret);

//     return REDIS_REPLY_INTEGER == otype && 1 == ovalue;
}

static VCL_VOID vmod_keystore_redis_expire(void *c, VCL_STRING key, VCL_DURATION d)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "EXPIRE %s %.f", key, d);
    AN(ret);

//     return REDIS_REPLY_INTEGER == otype && 1 == ovalue;
}

static VCL_INT vmod_keystore_redis_increment(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "INCR %s", key);
    AN(ret);
    AN(REDIS_REPLY_INTEGER == otype);

    return ovalue;
}

static VCL_INT vmod_keystore_redis_decrement(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "DECR %s", key);
    AN(ret);
    AN(REDIS_REPLY_INTEGER == otype);

    return ovalue;
}

static VCL_STRING vmod_keystore_redis_raw(struct ws *ws, void *c, VCL_STRING cmd)
{
    char *ovalue;
    int ret, otype;

    ret = _redis_do_string_command(ws, c, &otype, &ovalue, cmd);
    AN(ret);

    return ovalue;
}

#ifdef REDIS_SHARED_DRIVER
static
#endif /* REDIS_SHARED_DRIVER */
const vmod_keystore_driver_imp redis_driver = {
    "redis",
    vmod_keystore_redis_open,
    vmod_keystore_redis_close,
    vmod_keystore_redis_get,
    vmod_keystore_redis_add,
    vmod_keystore_redis_set,
    vmod_keystore_redis_exists,
    vmod_keystore_redis_delete,
    vmod_keystore_redis_expire,
    vmod_keystore_redis_increment,
    vmod_keystore_redis_decrement,
    vmod_keystore_redis_raw
};

#ifdef REDIS_SHARED_DRIVER
int init_function(struct vmod_priv *priv, const struct VCL_conf *cfg)
{
    pthread_once(&key_once, make_key);
    vmod_keystore_register_driver(&redis_driver);

    return 0;
}
#endif /* REDIS_SHARED_DRIVER */
