SRCDIR=	${.CURDIR}/../../kame/v6test

PROG=	v6test
SRCS=	v6test.c cksum.c testcap.c getconfig.c
MAN=	v6test.1 v6test.conf.5

CPPFLAGS+=-I${SRCDIR} -DIPSEC
#CPPFLAGS+=-Duint8_t=u_int8_t -Duint16_t=u_int16_t -Duint32_t=u_int32_t
CPPFLAGS+=-DSTDC_HEADERS=1 
CPPFLAGS+=-I${.CURDIR}/../../kame/libpcap -DHAVE_LIBPCAP
LDADD+=	-L${.CURDIR}/../../lib/libinet6 -L${.CURDIR}/../../lib/libinet6/obj \
	-L/usr/local/v6/lib -linet6 \
	-L${.CURDIR}/../../lib/libpcap \
	-L/usr/local/v6/lib -lpcap -linet6
DPADD+= ${.CURDIR}/../lib/libinet6/libinet6.a \
	${.CURDIR}/../lib/libinet6/obj/libinet6.a \
	/usr/local/v6/lib/libinet6.a \
	${.CURDIR}/../../lib/libpcap/libcap.a \
	/usr/local/v6/lib/libpcap.a

#CPPFLAGS += -DDEFAUT_IF="ep0"
CONFS = ext.conf ndp.conf synloop.conf
PREFIX?= /usr/local/v6
CONFDIR= $(PREFIX)/share/v6test

.PATH:	${SRCDIR}

.include <bsd.prog.mk>

afterinstall:
	[ -d $(CONFDIR) ] || mkdir -p $(CONFDIR); \
	for i in $(CONFS); do \
		cp $(SRCDIR)/conf/$$i $(CONFDIR); \
	done
