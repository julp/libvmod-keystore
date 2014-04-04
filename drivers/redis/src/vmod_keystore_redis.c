#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "vrt.h"
#include "cache/cache.h"
#include "vcc_if.h"
#include "keystore_driver.h"

#include <hiredis.h>

typedef struct {
    redisContext *c;
    pthread_mutex_t mtx;
} vmod_key_store_redis_data_t;

static void *vmod_key_store_redis_open(const char *host, int port, struct timeval tv)
{
    int tv_set;
    vmod_key_store_redis_data_t *d;

#if 1
    d = malloc(sizeof(*d));
    XXXAN(d);
#else
    ALLOC_OBJ(d, X_MAGIC);
#endif
    AZ(pthread_mutex_init(&d->mtx, NULL));
    tv_set = 0 != tv.tv_sec && 0 != tv.tv_usec;
    if (-1 == port) {
        if (tv_set) {
            d->c = redisConnectUnixWithTimeout(host, tv);
        } else {
            d->c = redisConnectUnix(host);
        }
    } else {
        if (tv_set) {
            d->c = redisConnectWithTimeout(host, port, tv);
        } else {
            d->c = redisConnect(host, port);
        }
    }
    if (NULL != d->c && d->c->err) {
//         VSLb(ctx->vsl, SLT_Error, "redis connection error: %s", c->errstr);
    }

    return d;
}

static void vmod_key_store_redis_close(void *c)
{
    vmod_key_store_redis_data_t *d;

    d = (vmod_key_store_redis_data_t *) c;
#if 1
    AN(d);
#else
    CHECK_OBJ_NOTNULL(d, X_MAGIC);
#endif
    AN(d->c);
    AZ(pthread_mutex_destroy(&d->mtx));
    redisFree((redisContext *) d->c);
#if 1
    free(c);
#else
    FREE_OBJ(c);
#endif
}

static int _redis_do_string_command(struct ws *ws, void *c, int *output_type, char **output_value, const char *command, ...)
{
    int ret;
    va_list ap;
    redisReply *r;
    vmod_key_store_redis_data_t *d;

    d = (vmod_key_store_redis_data_t *) c;
#if 1
    AN(d);
#else
    CHECK_OBJ_NOTNULL(d, X_MAGIC);
#endif
    AN(d->c);
    ret = 1;
    va_start(ap, command);
    AZ(pthread_mutex_lock(&d->mtx));
//     r = redisvCommand((redisContext *) d->c, command, ap);
    redisvAppendCommand((redisContext *) d->c, command, ap);
    va_end(ap);
    if (REDIS_OK == redisGetReply((redisContext *) d->c, (void **) &r)) {
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
    AZ(pthread_mutex_unlock(&d->mtx));
    freeReplyObject(r);

    return ret;
}

static VCL_STRING vmod_key_store_redis_get(struct ws *ws, void *c, VCL_STRING key)
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
    vmod_key_store_redis_data_t *d;

    d = (vmod_key_store_redis_data_t *) c;
#if 1
    AN(d);
#else
    CHECK_OBJ_NOTNULL(d, X_MAGIC);
#endif
    AN(d->c);
    ret = 1;
    AZ(pthread_mutex_lock(&d->mtx));
    va_start(ap, command);
//     r = redisvCommand((redisContext *) d->c, command, ap);
    redisvAppendCommand((redisContext *) d->c, command, ap);
    va_end(ap);
    if (REDIS_OK == redisGetReply((redisContext *) d->c, (void **) &r)) {
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
    AZ(pthread_mutex_unlock(&d->mtx));
    freeReplyObject(r);

    return ret;
}

static VCL_BOOL vmod_key_store_redis_add(void *c, VCL_STRING key, VCL_STRING value)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "SETNX %s %s", key, value);
    AN(ret);

    // TODO: retun FALSE if key already exists
    return REDIS_REPLY_INTEGER == otype && 1 == ovalue;
}

static VCL_VOID vmod_key_store_redis_set(void *c, VCL_STRING key, VCL_STRING value)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "SET %s %s", key, value);
    AN(ret);
    AN(REDIS_REPLY_STATUS == otype && 1 == ovalue);
}

static VCL_BOOL vmod_key_store_redis_exists(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "EXISTS %s", key);
    AN(ret);
    AN(REDIS_REPLY_INTEGER == otype);

    return ovalue;
}

static VCL_BOOL vmod_key_store_redis_delete(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "DEL %s", key);
    AN(ret);

    return REDIS_REPLY_INTEGER == otype && 1 == ovalue;
}

static VCL_VOID vmod_key_store_redis_expire(void *c, VCL_STRING key, VCL_DURATION d)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "EXPIRE %s %.f", key, d);
    AN(ret);

//     return REDIS_REPLY_INTEGER == otype && 1 == ovalue;
}

static VCL_INT vmod_key_store_redis_increment(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "INCR %s", key);
    AN(ret);
    AN(REDIS_REPLY_INTEGER == otype);

    return ovalue;
}

static VCL_INT vmod_key_store_redis_decrement(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_int_command(c, &otype, &ovalue, "DECR %s", key);
    AN(ret);
    AN(REDIS_REPLY_INTEGER == otype);

    return ovalue;
}

#ifdef REDIS_SHARED_DRIVER
static
#endif /* REDIS_SHARED_DRIVER */
const vmod_key_store_driver redis_driver = {
    "redis",
    vmod_key_store_redis_open,
    vmod_key_store_redis_close,
    vmod_key_store_redis_get,
    vmod_key_store_redis_add,
    vmod_key_store_redis_set,
    vmod_key_store_redis_exists,
    vmod_key_store_redis_delete,
    vmod_key_store_redis_expire,
    vmod_key_store_redis_increment,
    vmod_key_store_redis_decrement
};

#ifdef REDIS_SHARED_DRIVER
int init_function(struct vmod_priv *priv, const struct VCL_conf *cfg)
{
    vmod_key_store_register_driver(&redis_driver);

    return 0;
}
#endif /* REDIS_SHARED_DRIVER */
