# Available drivers

* redis
* memcached

# Prerequisites

* cmake
* varnish >= 4 (includings its sources)
* hiredis for redis driver
* [libmemcached](http://libmemcached.org) for memcached driver

# How to build

## With embedded drivers (statically linked)

```
cd /path/to/vmod_keystore
cmake . -DVARNISHSRC:PATH=/path/to/varnish/sources
```

Varnish configuration:
```
import keystore;
```

## With drivers out of the box (as a separate vmod)

First build vmod_keystore alone:
```
cd /path/to/vmod_keystore
cmake . -DVARNISHSRC:PATH=/path/to/varnish/sources
```
Then build your driver(s). Example for redis:
```
cd /path/to/vmod_keystore/drivers/redis
cmake . -DVARNISHSRC:PATH=/path/to/varnish/sources
```

Varnish configuration, with redis as driver:
```
import keystore;
import keystore_redis;
```

# VMod methods

Instanciate a connection to your backend:

```
new <variable name> = keystore.driver('<driver name>:host=<IP address or hostname or path to socket>;port=<port>;timeout=<timeout>');
```

* `BOOL add(STRING key, STRING value)`: add the given *key* if it does not already exist
* `BOOL delete(STRING key)`: delete the given *key*
* `VOID set(STRING key, STRING value)`: add or replace the given *key*
* `BOOL expire(STRING key, DURATION ttl)`: set expiration of the given *key*
* `INT increment(STRING key)`: increment (of 1) value for `key` and return its current value
* `INT decrement(STRING key)`: decrement (of 1) value for `key` and return its current value
