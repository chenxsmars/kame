#	$OpenBSD: Makefile,v 1.1 2001/06/26 21:58:06 smurph Exp $

LIB=sa

CLEANFILES+=SRT0.o SRT1.o

NOPIC=nopic
NOPROFILE=noprofile

# Logically src/sys
S=${.CURDIR}/../../../..
DIR_SA=$S/lib/libsa
DIR_KERN=$S/lib/libkern

SRC_net= arp.c ether.c in_cksum.c net.c netif.c rpc.c nfs.c \
	 rarp.c bootparam.c 

SRC_sa=	alloc.c memcpy.c exit.c getfile.c gets.c globals.c \
	printf.c strerror.c memset.c memcmp.c strncpy.c strcmp.c strlen.c \
	close.c closeall.c dev.c disklabel.c dkcksum.c \
	lseek.c open.c nullfs.c read.c fstat.c \
	ufs.c cread.c

SRC_kern= ashrdi3.c bzero.c strcpy.c

SRC_mvme= exec_mvme.c

SRC_here= bugdev.c cache.c clock.c conf.c parse_args.c rawfs.c tftpfs.c

SRCS= ${SRC_net} ${SRC_sa} ${SRC_mvme} ${SRC_here} ${SRC_kern}

# DBG= -DDEBUG -DNETIF_DEBUG -DNFS_DEBUG -DRPC_DEBUG \
# 	-DNET_DEBUG -DRARP_DEBUG -DETHER_DEBUG

#DEFS= -DCOMPAT_UFS
DEFS= -D__INTERNAL_LIBSA_CREAD 
#-DNETIF_DEBUG
INCL= -I${.CURDIR} -I${.CURDIR}/../libbug -I${S}/lib/libsa -I${S}
COPTS= #-fno-defer-pop
CFLAGS+= ${XCFLAGS} ${COPTS} ${DEFS} ${DBG} ${INCL} -O2

.PATH:  ${DIR_SA} ${DIR_KERN} 

all: libsa.a

install:

.include <bsd.lib.mk>
