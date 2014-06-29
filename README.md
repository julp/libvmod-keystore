# Available drivers

* redis
* memcached

# Prerequisites

* cmake
* varnish >= 4 (including its sources)
* [hiredis](https://github.com/redis/hiredis) for redis driver
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

## With drivers as separate vmods

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
* `VOID expire(STRING key, DURATION ttl)`: set expiration of the given *key* (keys are inserted as persitent with 0 as TTL ; use 30s as value of *ttl*, for the *key* to expire in 30 seconds)
* `INT increment(STRING key)`: return value associated to *key* after incrementing it (of 1)
* `INT decrement(STRING key)`: return value associated to *key* after decrementing it (of 1)
* `STRING name()` : return current driver name

# Examples

## Prevent brute-force on http authentication

```
import std;
import keystore;
#import keystore_redis; # if compiled as a separate VMOD

sub vcl_init {
    new ipstore = keystore.driver("redis:host=localhost;port=6379");
}

sub vcl_recv {
    if (std.integer(ipstore.get("" + client.ip), 0) >= 5) {
        return(synth(403));
    }
    # ...
}

sub vcl_deliver {
    # ...
    if (401 == resp.status && req.http.Authorization) {
        if (ipstore.increment("" + client.ip) >= 5) {
            ipstore.expire("" + client.ip, 4h); # ban for 4 hours
        } else {
            ipstore.expire("" + client.ip, 1h); # reset attempts count after 1h
        }
    }
    # ...
}
```
