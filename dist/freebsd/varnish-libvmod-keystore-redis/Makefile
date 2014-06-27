# $FreeBSD$

PORTNAME=	libvmod-keystore-redis
PORTVERSION=	1.0
CATEGORIES=	www
PKGNAMEPREFIX=	varnish-
DISTNAME=	libvmod-keystore

MAINTAINER=	e2c2be7ed0f2f336@gmail.com
COMMENT=	Varnish module (vmod) to interact with redis

LICENSE=	BSD

# Disable dependency on www/varnish for now as it is still version 3.0.5
#BUILD_DEPENDS=	${LOCALBASE}/include/varnish/varnishapi.h:${PORTSDIR}/www/varnish
LIB_DEPENDS=	libhiredis.so:${PORTSDIR}/databases/hiredis
RUN_DEPENDS=	${LOCALBASE}/lib/varnish/libvmod_keystore.so:${PORTSDIR}/www/varnish-libvmod-keystore

USE_GITHUB=	yes
GH_ACCOUNT=	julp
GH_COMMIT=	79fa514
GH_TAGNAME=	${GH_COMMIT}
GH_PROJECT=	${DISTNAME}

USES=		cmake:outsource
WRKSRC=		${WRKDIR}/${GH_ACCOUNT}-${GH_PROJECT}-${GH_COMMIT}/drivers/redis

XFLAGS=		-I${LOCALBASE}/include -I${LOCALBASE}/include/varnish
CFLAGS+=	${XFLAGS}
CPPFLAGS+=	${XFLAGS}

.include <bsd.port.mk>