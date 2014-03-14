#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "vrt.h"
#include "cache/cache.h"
#include "vcc_if.h"
#include "keystore_driver.h"

#include <hiredis.h>

#define debug(fmt, ...) \
    do { \
        FILE *fp;\
        fp = fopen("/tmp/keystore.log", "a"); \
        fprintf(fp, fmt "\n", ## __VA_ARGS__); \
        fclose(fp); \
    } while (0);

static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

static void *vmod_key_store_redis_open(const char *host, int port, struct timeval tv)
{
    int tv_set;

debug("redisConnect*");
    tv_set = 0 != tv.tv_sec && 0 != tv.tv_usec;
    if (-1 == port) {
        if (tv_set) {
            return redisConnectUnixWithTimeout(host, tv);
        } else {
            return redisConnectUnix(host);
        }
    } else {
        if (tv_set) {
            return redisConnectWithTimeout(host, port, tv);
        } else {
            return redisConnect(host, port);
        }
    }
}

static void vmod_key_store_redis_close(void *c)
{
    redisFree((redisContext *) c);
}

static int _redis_do_command(void *c, int *output_type, void *output_value, const char *command, ...)
{
    va_list ap;
    redisReply *r;
    int i, ret, expected_type;

    AN(c);
    ret = 1;
    va_start(ap, command);
    AZ(pthread_mutex_lock(&mtx));
//     r = redisvCommand((redisContext *) c, command, ap);
    redisvAppendCommand((redisContext *) c, command, ap);
    va_end(ap);
    if (REDIS_OK == redisGetReply((redisContext *) c, (void **) &r)) {
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
    AZ(pthread_mutex_unlock(&mtx));
    freeReplyObject(r);

    return ret;
}

static VCL_BOOL vmod_key_store_redis_add(void *c, VCL_STRING key, VCL_STRING value)
{
    int ret, otype, ovalue;

    ret = _redis_do_command(c, &otype, &ovalue, "SETNX %s %s", key, value);
    AN(ret);

    return REDIS_REPLY_INTEGER == otype && 1 == ovalue;
}

static VCL_BOOL vmod_key_store_redis_delete(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_command(c, &otype, &ovalue, "DEL %s", key);
    AN(ret);

    return REDIS_REPLY_INTEGER == otype && 1 == ovalue;
}

static VCL_VOID vmod_key_store_redis_set(void *c, VCL_STRING key, VCL_STRING value)
{
    int ret, otype, ovalue;

    ret = _redis_do_command(c, &otype, &ovalue, "SET %s %s", key, value);
    AN(ret);
    AN(REDIS_REPLY_STATUS == otype && 1 == ovalue);
}

static VCL_BOOL vmod_key_store_redis_expire(void *c, VCL_STRING key, VCL_DURATION d)
{
    int ret, otype, ovalue;

    ret = _redis_do_command(c, &otype, &ovalue, "EXPIRE %s %.f", key, d);
    AN(ret);

    return REDIS_REPLY_INTEGER == otype && 1 == ovalue;
}

static VCL_INT vmod_key_store_redis_increment(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_command(c, &otype, &ovalue, "INCR %s", key);
    AN(ret);
    AN(REDIS_REPLY_INTEGER == otype);

    return ovalue;
}

static VCL_INT vmod_key_store_redis_decrement(void *c, VCL_STRING key)
{
    int ret, otype, ovalue;

    ret = _redis_do_command(c, &otype, &ovalue, "DECR %s", key);
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
    vmod_key_store_redis_add,
    vmod_key_store_redis_delete,
    vmod_key_store_redis_set,
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
