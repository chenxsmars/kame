# $Id: Makefile,v 1.7 2002/09/04 21:43:59 max Exp $
# $FreeBSD: src/sys/modules/netgraph/bluetooth/socket/Makefile,v 1.2 2002/11/21 07:21:45 imp Exp $

.PATH: ${.CURDIR}/../../../../netgraph/bluetooth/socket

CFLAGS+=	-g -I${.CURDIR}/../../../../netgraph/bluetooth/include \
		-DINVARIANTS=1 -DINVARIANT_SUPPORT=1 \
		-DWITNESS=1 -DWITNESS_SKIPSPIN=1

KMOD=		ng_btsocket
SRCS=		ng_btsocket.c \
		ng_btsocket_hci_raw.c \
		ng_btsocket_l2cap_raw.c \
		ng_btsocket_l2cap.c
MAN4=		ng_btsocket.4

.include <bsd.kmod.mk>
