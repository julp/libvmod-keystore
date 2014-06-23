# $FreeBSD$

PORTNAME=		libvmod-keystore
PORTVERSION=		1.0
CATEGORIES=		www
PKGNAMEPREFIX=		varnish-

MAINTAINER=		e2c2be7ed0f2f336@gmail.com
COMMENT=		Varnish module (vmod) to interact with key/value store databases

LICENSE=		BSD

# Disable dependency on www/varnish for now as it is still version 3.0.5
#BUILD_DEPENDS=		${LOCALBASE}/include/varnish/varnishapi.h:${PORTSDIR}/www/varnish

USE_GITHUB=		yes
GH_ACCOUNT=		julp
GH_COMMIT=		97fb7cf
GH_TAGNAME=		${GH_COMMIT}

USES=			cmake:outsource

OPTIONS_DEFINE=		REDIS MEMCACHED
OPTIONS_DEFAULT=	REDIS MEMCACHED

REDIS_DESC=		Enable redis support
MEMCACHED_DESC=		Enable memcached support

REDIS_CMAKE_ON=		-DWITH_REDIS:BOOL=ON
REDIS_CMAKE_OFF=	-DWITH_REDIS:BOOL=OFF
REDIS_LIB_DEPENDS=	libhiredis.so:${PORTSDIR}/databases/hiredis
MEMCACHED_CMAKE_ON=	-DWITH_MEMCACHED:BOOL=ON
MEMCACHED_CMAKE_OFF=	-DWITH_MEMCACHED:BOOL=OFF
MEMCACHED_LIB_DEPENDS=	libmemcached.so:${PORTSDIR}/databases/libmemcached

XFLAGS=			-I${LOCALBASE}/include -I${LOCALBASE}/include/varnish
CFLAGS+=		${XFLAGS}
CPPFLAGS+=		${XFLAGS}

.include <bsd.port.options.mk>

.if empty(PORT_OPTIONS:MREDIS) && empty(PORT_OPTIONS:MMEMCACHED)
IGNORE=			no backend were enabled, please re-run make config
.endif

.include <bsd.port.mk>