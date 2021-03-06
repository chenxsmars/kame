#! /usr/bin/awk -f
#
#	create  dos_strerror()  from dos_errno.h
#
#	written by Yasha (ITOH Yasufumi)
#	public domain
#
#	$NetBSD: makestrerror.awk,v 1.1 1998/09/01 19:53:26 itohy Exp $

/^\/\* dos_errlist begin \*\/$/,/^\/\* dos_errlist end \*\/$/ {
	if ($0 ~ /^\/\* dos_errlist begin \*\/$/) {
		# assembly code
		print "| This file is automatically generated.  DO NOT EDIT."
		print "#include \"dos_errno.h\""
		print "	.text"
		print "	.even"
		print "	.globl	_dos_nerr"
		print "_dos_nerr:"
		print "	.long	DOS_ELAST+1"
		print ""
		print "	.globl	_dos_strerror"
		print "_dos_strerror:"
		print "	movel	sp@(4),d0"
		print "	moveq	#80,d1"
		print "	cmpl	d0,d1"
		print "	bnes	Lnot80"
		print "	moveq	#DOS_EEXIST,d0"
		print "Lnot80:	moveq	#DOS_ELAST+1,d1"
		print "	cmpl	d1,d0"
		print "	bcss	Lnotuk"
		print "	movel	d1,d0"
		print "Lnotuk:	lslw	#1,d0"
		print "Lh1:\tmovew\tpc@(Lerrtbl-Lh1-2:B,d0:W),d0\t| 303B 000A"
		print "Lh2:\tlea\tpc@(Lerrtbl-Lh2-2:B,d0:W),a0\t| 41FB 0006"
		print "	movel	a0,d0"
		print "	rts"
		print ""
		print "Lerrtbl:"
		nmsg = 0
	} else if ($0 ~ /^\/\* dos_errlist end \*\/$/) {
		print "\t.word\tLukmsg-Lerrtbl		| default message"
		print ""
		# error strings
		for (i = 0; i < nmsg; i++)
			print "Lmsg" i ":\t.asciz\t\"" msg[i] "\""

		print "Lukmsg:\t.asciz\t\"Unknown error\""
		exit
	} else {
		if ($3 != nmsg || $4 != "/*") {
			printf FILENAME ":" NR ": format error"
			exit(1);
		}
		# offset table
		print "\t.word\tLmsg" nmsg "-Lerrtbl"
		$1 = ""
		$2 = ""
		$3 = ""
		$4 = ""
		msg[nmsg] = substr($0, 5, length - 7)
		nmsg++
	}
}
