#include <stdlib.h>
#include <stdio.h>

#include "vrt.h"
#include "cache/cache.h"
#include "vcc_if.h"
#include "keystore_driver.h"

#define ARRAY_SIZE(array) (sizeof(array) / sizeof((array)[0]))
#define STR_LEN(str) (ARRAY_SIZE(str) - 1)
#define STR_SIZE(str) (ARRAY_SIZE(str))

struct vmod_driver {
    unsigned magic;
#define VMOD_STORE_OBJ_MAGIC 0x3366feff
    const vmod_key_store_driver *driver;
    void *private;
};

struct vmod_key_store_registered_driver {
    unsigned magic;
#define REGISTERED_DRIVER_MAGIC 0x1166feff
    const vmod_key_store_driver *driver;
    VTAILQ_ENTRY(vmod_key_store_registered_driver) list;
};

static VTAILQ_HEAD(, vmod_key_store_registered_driver) drivers = VTAILQ_HEAD_INITIALIZER(drivers);

void vmod_key_store_register_driver(const vmod_key_store_driver * const driver)
{
    struct vmod_key_store_registered_driver *d;

    ALLOC_OBJ(d, REGISTERED_DRIVER_MAGIC);
    AN(d);
    d->driver = driver;
    VTAILQ_INSERT_HEAD(&drivers, d, list);
}

#include <math.h>
static int parse_tv(const char *string, struct timeval *tv)
{
    double d;
    char *endptr;

    /**
     * \d+(?:\.\d+)?s?
     * \d+ms
     **/
    if (NULL == string || '\0' == *string) {
        return 0;
    }
    d = strtod(string, &endptr);
    if (!isfinite(d)) {
        return 0;
    }
    while (' ' == *string) {
        ++string;
    }
    switch (*endptr) {
        case '\0':
        case 's':
            tv->tv_sec = (long int) d;
            tv->tv_usec = (long int) ((d - (double) tv->tv_sec) * 1000000.0);
            break;
        case 'm':
            if ('\0' != *endptr && 's' == ++*endptr) {
                tv->tv_usec = (long int) d;
            }
            break;
        default:
            return 0;
    }

    return 1;
}

static int strcmp_l(
    const char *str1, size_t str1_len,
    const char *str2, size_t str2_len
) {
    size_t min_len;

    if (str1 != str2) {
        if (str2_len < str1_len) {
            min_len = str2_len;
        } else {
            min_len = str1_len;
        }
        while (min_len--) {
            if (*str1 != *str2) {
                return (unsigned char) *str1 - (unsigned char) *str2;
            }
            ++str1, ++str2;
        }
    }

    return str1_len - str2_len;
}

VCL_VOID vmod_driver__init(const struct vrt_ctx *ctx, struct vmod_driver **pp, const char *vcl_name, VCL_STRING dsn)
{
    int port;
    void *conn;
    struct timeval tv;
    struct vmod_driver *p;
    char *ptr, *host, *pname, *pvalue;
    struct vmod_key_store_registered_driver *d;
    const vmod_key_store_driver *effective_driver;

    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    AN(pp);
    AZ(*pp);

    port = -1;
    host = NULL;
    memset(&tv, 0, sizeof(tv));
    if (NULL == (ptr = strchr(dsn, ':'))) {
        VSLb(ctx->vsl, SLT_Error, "no driver name found");
    }
    XXXAN(ptr);
    VTAILQ_FOREACH(d, &drivers, list) {
        if (0 == strcmp_l(dsn, ptr - dsn /*- 1*/ /* ':' */, d->driver->name, strlen(d->driver->name))) {
            effective_driver = d->driver;
            break;
        }
    }
    if (NULL == effective_driver) {
        VSLb(ctx->vsl, SLT_Error, "driver '%.*s' not found", ptr - dsn, dsn);
    }
    XXXAN(effective_driver);
    ++ptr; /* move after ':' */
    if ('\0' != *ptr) {
        const char * const dsn_end = dsn + strlen(dsn);

        pname = ptr;
        while (ptr < dsn_end) {
            if ('=' == *ptr) {
                ++ptr; /* move after '=' */
                pvalue = ptr;
                while (ptr < dsn_end) {
                    if (';' == *ptr) {
                        break;
                    }
                    ++ptr;
                }
//                 debug("attr = >%.*s<, value = >%.*s<\n", pvalue - pname - 1 /* '=' */, pname, ptr - pvalue /*- ('\0' == *ptr ? 0 : 1)*/, pvalue);
                if (0 == strcmp_l("host", STR_LEN("host"), pname, pvalue - pname - 1)) {
                    host = malloc(sizeof(*host) * (ptr - pvalue + 1));
                    strncpy(host, pvalue, ptr - pvalue);
                } else if (0 == strcmp_l("timeout", STR_LEN("timeout"), pname, pvalue - pname - 1)) {
                    parse_tv(pvalue, &tv);
                } else if (0 == strcmp_l("port", STR_LEN("port"), pname, pvalue - pname - 1)) {
                    port = atoi(pvalue);
                }
                pname = ptr + 1;
            }
            ++ptr;
        }
    }
    XXXAN(host);
    conn = effective_driver->open(host, port, tv);
    free(host);
    XXXAN(conn);

    ALLOC_OBJ(p, VMOD_STORE_OBJ_MAGIC);
    AN(p);
    *pp = p;
    p->driver = effective_driver;
    p->private = conn;
    AN(*pp);
}

VCL_VOID vmod_driver__fini(struct vmod_driver **pp)
{
    struct vmod_driver *p;

    AN(pp);
    CHECK_OBJ_NOTNULL(*pp, VMOD_STORE_OBJ_MAGIC);

    p = *pp;
    p->driver->close(p->private);
    FREE_OBJ(*pp);
    *pp = NULL;
}

VCL_STRING vmod_driver_get(const struct vrt_ctx *ctx, struct vmod_driver *p, VCL_STRING key)
{
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(p, VMOD_STORE_OBJ_MAGIC);
    AN(p->driver->get);

    return p->driver->get(ctx->ws, p->private, key);
}

VCL_BOOL vmod_driver_add(const struct vrt_ctx *ctx, struct vmod_driver *p, VCL_STRING key, VCL_STRING value)
{
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(p, VMOD_STORE_OBJ_MAGIC);
    AN(p->driver->add);

    return p->driver->add(p->private, key, value);
}

VCL_VOID vmod_driver_set(const struct vrt_ctx *ctx, struct vmod_driver *p, VCL_STRING key, VCL_STRING value)
{
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(p, VMOD_STORE_OBJ_MAGIC);
    AN(p->driver->set);

    return p->driver->set(p->private, key, value);
}

VCL_BOOL vmod_driver_exists(const struct vrt_ctx *ctx, struct vmod_driver *p, VCL_STRING key)
{
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(p, VMOD_STORE_OBJ_MAGIC);
    AN(p->driver->exists);

    return p->driver->exists(p->private, key);
}

VCL_BOOL vmod_driver_delete(const struct vrt_ctx *ctx, struct vmod_driver *p, VCL_STRING key)
{
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(p, VMOD_STORE_OBJ_MAGIC);
    AN(p->driver->delete);

    return p->driver->delete(p->private, key);
}

VCL_VOID vmod_driver_expire(const struct vrt_ctx *ctx, struct vmod_driver *p, VCL_STRING key, VCL_DURATION duration)
{
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(p, VMOD_STORE_OBJ_MAGIC);
    AN(p->driver->expire);

    p->driver->expire(p->private, key, duration);
}

VCL_INT vmod_driver_increment(const struct vrt_ctx *ctx, struct vmod_driver *p, VCL_STRING key)
{
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(p, VMOD_STORE_OBJ_MAGIC);
    AN(p->driver->increment);

    return p->driver->increment(p->private, key);
}

VCL_INT vmod_driver_decrement(const struct vrt_ctx *ctx, struct vmod_driver *p, VCL_STRING key)
{
    CHECK_OBJ_NOTNULL(ctx, VRT_CTX_MAGIC);
    CHECK_OBJ_NOTNULL(p, VMOD_STORE_OBJ_MAGIC);
    AN(p->driver->decrement);

    return p->driver->decrement(p->private, key);
}

int init_function(struct vmod_priv *priv, const struct VCL_conf *cfg)
{
#if 0
    priv->priv =
    priv->free =
#endif
#ifdef REDIS_STATIC_DRIVER
    extern const vmod_key_store_driver redis_driver;

    vmod_key_store_register_driver(&redis_driver);
#endif /* REDIS_STATIC_DRIVER */
#ifdef MEMCACHED_STATIC_DRIVER
    extern const vmod_key_store_driver memcached_driver;

    vmod_key_store_register_driver(&memcached_driver);
#endif /* MEMCACHED_STATIC_DRIVER */

    return 0;
}
