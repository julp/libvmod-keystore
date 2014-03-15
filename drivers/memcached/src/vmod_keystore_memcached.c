#include <stdlib.h>
#include <stdio.h>

#include "vrt.h"
#include "cache/cache.h"
#include "vcc_if.h"
#include "keystore_driver.h"

#include <memcached.h>

static void *vmod_key_store_memcached_open(const char *host, int port, struct timeval tv)
{
    memcached_st *c;
    memcached_return_t rc;

    c = memcached_create(NULL);
    if (-1 == port) {
        rc = memcached_server_add_unix_socket(c, host);
    } else {
        rc = memcached_server_add(c, host, port);
    }
    if (MEMCACHED_SUCCESS != rc) {
        // libmemcached_strerror(rc)
    }
    AN(MEMCACHED_SUCCESS == rc);
    if (0 != tv.tv_sec && 0 != tv.tv_usec) {
        memcached_behavior_set(c, MEMCACHED_BEHAVIOR_CONNECT_TIMEOUT, (uint64_t) tv.tv_sec);
    }

    return c;
}

static void vmod_key_store_memcached_close(void *c)
{
    memcached_free((memcached_st *) c);
}

static VCL_STRING vmod_key_store_memcached_get(struct ws *ws, void *c, VCL_STRING key)
{
    uint32_t flags;
    size_t ovalue_len;
    memcached_return_t rc;
    char *ovalue, *vvalue;

    if (NULL == (ovalue = memcached_get((memcached_st *) c, key, strlen(key), &ovalue_len, &flags, &rc))) {
        vvalue = NULL;
    } else {
        vvalue = WS_Copy(ws, ovalue, ovalue_len + 1);
        free(ovalue);
    }

    return vvalue;
}

static int _memcached_do_set_add_replace(
    memcached_return_t (*fn)(memcached_st *, const char *, size_t, const char *, size_t, time_t, uint32_t),
    void *c,
    VCL_STRING key,
    VCL_STRING value
) {
    memcached_return_t rc;

    rc = fn((memcached_st *) c, key, strlen(key), value, strlen(value), (time_t) 0, 0);
    // TODO: For memcached_replace() and memcached_add(), MEMCACHED_NOTSTORED is a legitmate error in the case of a collision

    return MEMCACHED_SUCCESS == rc;
}

static VCL_BOOL vmod_key_store_memcached_add(void *c, VCL_STRING key, VCL_STRING value)
{
    // TODO: retun FALSE if key already exists
    return _memcached_do_set_add_replace(memcached_add, c, key, value);
}

static VCL_VOID vmod_key_store_memcached_set(void *c, VCL_STRING key, VCL_STRING value)
{
    _memcached_do_set_add_replace(memcached_set, c, key, value);
}

static VCL_BOOL vmod_key_store_memcached_exists(void *c, VCL_STRING key)
{
    memcached_return_t rc;

    rc = memcached_exist((memcached_st *) c, key, strlen(key));
    AN(MEMCACHED_NOTFOUND == rc || MEMCACHED_SUCCESS == rc);

    return MEMCACHED_SUCCESS == rc;
}

static VCL_BOOL vmod_key_store_memcached_delete(void *c, VCL_STRING key)
{
    memcached_return_t rc;

    rc = memcached_delete((memcached_st *) c, key, strlen(key), 0);
    if (MEMCACHED_SUCCESS != rc) {
        // VSLb(ctx->vsl, SLT_Error, "memcached error: %s", memcached_strerror((memcached_st *) c, rc));
    }

    return MEMCACHED_SUCCESS == rc;
}

static VCL_BOOL vmod_key_store_memcached_expire(void *c, VCL_STRING key, VCL_DURATION d)
{
    memcached_return_t rc;

    rc = memcached_touch((memcached_st *) c, key, strlen(key), (time_t) (int) d);

    return MEMCACHED_SUCCESS == rc;
}

static int _memcached_do_in_de_crement(memcached_return_t (*fn)(memcached_st *, const char *, size_t, uint32_t, uint64_t *), void *c, VCL_STRING key)
{
    uint64_t ovalue;
    memcached_return_t rc;

    ovalue = 0;
    rc = fn((memcached_st *) c, key, strlen(key), 1, &ovalue);
    if (MEMCACHED_SUCCESS != rc) {
        // VSLb(ctx->vsl, SLT_Error, "memcached error: %s", memcached_strerror((memcached_st *) c, rc));
    }

    return ovalue;
}

static VCL_INT vmod_key_store_memcached_increment(void *c, VCL_STRING key)
{
    return _memcached_do_in_de_crement(memcached_increment, c, key);
}

static VCL_INT vmod_key_store_memcached_decrement(void *c, VCL_STRING key)
{
    return _memcached_do_in_de_crement(memcached_decrement, c, key);
}

#ifdef MEMCACHED_SHARED_DRIVER
static
#endif /* MEMCACHED_SHARED_DRIVER */
const vmod_key_store_driver memcached_driver = {
    "memcached",
    vmod_key_store_memcached_open,
    vmod_key_store_memcached_close,
    vmod_key_store_memcached_get,
    vmod_key_store_memcached_add,
    vmod_key_store_memcached_set,
    vmod_key_store_memcached_exists,
    vmod_key_store_memcached_delete,
    vmod_key_store_memcached_expire,
    vmod_key_store_memcached_increment,
    vmod_key_store_memcached_decrement
};

#ifdef MEMCACHED_SHARED_DRIVER
int init_function(struct vmod_priv *priv, const struct VCL_conf *cfg)
{
    vmod_key_store_register_driver(&memcached_driver);

    return 0;
}
#endif /* MEMCACHED_SHARED_DRIVER */
