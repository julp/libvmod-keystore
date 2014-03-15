# Available drivers

* redis
* memcached

# Prerequisites

* cmake
* varnish >= 4 (including its sources)
* hiredis for redis driver
* [libmemcached](http://libmemcached.org) for memcached driver

# How to build

## With embedded drivers (statically linked)

```
cd /path/to/vmod_keystore
cmake . -DVARNISHSRC:PATH=/path/to/varnish/sources
```
(if hiredis is found, redis driver will automatically be enabled ; same thing with libmemcached for memcached driver)

In top of your Varnish configuration, add:
```
import keystore;
```

## With drivers out of the box (as separated vmods)

First build vmod_keystore alone:
```
cd /path/to/vmod_keystore
cmake . -DVARNISHSRC:PATH=/path/to/varnish/sources -DWITH_REDIS:BOOL=OFF -DWITH_MEMCACHED:BOOL=OFF
```
Then build your driver(s). Example for redis:
```
cd /path/to/vmod_keystore/drivers/redis
cmake . -DVARNISHSRC:PATH=/path/to/varnish/sources
```

To load redis driver, your varnish configuration should be similar to:
```
import keystore;
import keystore_redis; # add this line AFTER keystore
```

# VMod methods

Instanciate a connection to your backend:

```
new <variable name> = keystore.driver('<driver name>:host=<IP address or hostname or path to socket>;port=<port>;timeout=<timeout>');
```

* `STRING get(STRING key)`: fetch current value associated to *key*
* `BOOL add(STRING key, STRING value)`: add the given *key* if it does not already exist (returns FALSE if it already exists)
* `VOID set(STRING key, STRING value)`: add or replace (overwrites) the *value* associated to *key*
* `BOOL exists(STRING key)`: does *key* exist?
* `BOOL delete(STRING key)`: delete *key*
* `BOOL expire(STRING key, DURATION ttl)`: set expiration of the given *key* (keys are inserted as persitent with 0 as TTL ; use 30s as value of *ttl*, for the *key* to expire in 30 seconds)
* `INT increment(STRING key)`: return value associated to *key* after incrementing it (of 1)
* `INT decrement(STRING key)`: return value associated to *key* after decrementing it (of 1)

# Limitations

* varnish is strongly typed: methods are planned to get/return a given type, nothing else
* varnish has no iterator and/or a kind of foreach construct to work on non-scalar type (like arrays, sets, etc)
* varnish has no null or nil value
