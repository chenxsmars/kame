/*	$KAME: icmp6.c,v 1.248 2001/10/15 12:36:49 keiichi Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 1982, 1986, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ip_icmp.c	8.2 (Berkeley) 1/4/94
 */

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_mip6.h"
#endif
#ifdef __NetBSD__
#include "opt_inet.h"
#include "opt_ipsec.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>
#include <sys/domain.h>

#include <net/if.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#if defined(__OpenBSD__) || (defined(__bsdi__) && _BSDI_VERSION >= 199802)
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#endif
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/mld6_var.h>
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>
#elif defined(__OpenBSD__) || (defined(__bsdi__) && _BSDI_VERSION >= 199802)
#include <netinet/in_pcb.h>
#else
#include <netinet6/in6_pcb.h>
#endif
#include <netinet6/nd6.h>
#include <netinet6/in6_ifattach.h>
#include <netinet6/ip6protosw.h>

#ifdef __OpenBSD__ /* KAME IPSEC */
#undef IPSEC
#endif

#ifdef IPSEC
#include <netinet6/ipsec.h>
#include <netkey/key.h>
#endif

#include "faith.h"
#if defined(NFAITH) && 0 < NFAITH
#include <net/if_faith.h>
#endif

#ifdef MIP6
#include <netinet6/mip6.h>
#endif

#include <net/net_osdep.h>

#ifdef HAVE_NRL_INPCB
/* inpcb members */
#define in6pcb		inpcb
#define in6p_laddr	inp_laddr6
#define in6p_faddr	inp_faddr6
#define in6p_icmp6filt	inp_icmp6filt
#define in6p_route	inp_route
#define in6p_socket	inp_socket
#define in6p_flags	inp_flags
#define in6p_moptions	inp_moptions6
#define in6p_outputopts	inp_outputopts6
#define in6p_ip6	inp_ipv6
#define in6p_flowinfo	inp_flowinfo
#define in6p_sp		inp_sp
#define in6p_next	inp_next
#define in6p_prev	inp_prev

/*
 * for KAME src sync over BSD*'s.  XXX: FreeBSD (>=3) are VERY different from
 * others...
 */
#define in6p_ip6_nxt	inp_ipv6.ip6_nxt
#endif

extern struct domain inet6domain;
extern struct ip6protosw inet6sw[];
extern u_char ip6_protox[];

struct icmp6stat icmp6stat;

#if defined (__OpenBSD__)
extern struct inpcbtable rawin6pcbtable;
#elif !(defined(__FreeBSD__) && __FreeBSD__ >= 3)
extern struct in6pcb rawin6pcb;
#else
extern struct inpcbhead ripcb;
#endif
extern int icmp6errppslim;
static int icmp6errpps_count = 0;
static struct timeval icmp6errppslim_last;
extern int icmp6_nodeinfo;
#if defined(__NetBSD__) || defined(__OpenBSD__)
/*
 * List of callbacks to notify when Path MTU changes are made.
 */
struct icmp6_mtudisc_callback {
	LIST_ENTRY(icmp6_mtudisc_callback) mc_list;
	void (*mc_func) __P((struct in6_addr *));
};

LIST_HEAD(, icmp6_mtudisc_callback) icmp6_mtudisc_callbacks =
    LIST_HEAD_INITIALIZER(&icmp6_mtudisc_callbacks);

static struct rttimer_queue *icmp6_mtudisc_timeout_q = NULL;
extern int pmtu_expire;

/* XXX do these values make any sense? */
static int icmp6_mtudisc_hiwat = 1280;
static int icmp6_mtudisc_lowat = 256;

/*
 * keep track of # of redirect routes.
 */
static struct rttimer_queue *icmp6_redirect_timeout_q = NULL;

/* XXX do these values make any sense? */
static int icmp6_redirect_hiwat = 1280;
static int icmp6_redirect_lowat = 1024;
#endif

static void icmp6_errcount __P((struct icmp6errstat *, int, int));
static int icmp6_rip6_input __P((struct mbuf **, int));
static int icmp6_ratelimit __P((const struct in6_addr *, const int, const int));
static const char *icmp6_redirect_diag __P((struct in6_addr *,
	struct in6_addr *, struct in6_addr *));
#ifndef HAVE_PPSRATECHECK
static int ppsratecheck __P((struct timeval *, int *, int));
#endif
static struct mbuf *ni6_input __P((struct mbuf *, int));
static struct mbuf *ni6_nametodns __P((const char *, int, int));
static int ni6_dnsmatch __P((const char *, int, const char *, int));
static int ni6_addrs __P((struct icmp6_nodeinfo *, struct mbuf *,
			  struct ifnet **, char *));
static int ni6_store_addrs __P((struct icmp6_nodeinfo *, struct icmp6_nodeinfo *,
				struct ifnet *, int));
static int icmp6_notify_error __P((struct mbuf *, int, int, int));
static int icmp6_recover_src __P((struct mbuf *));
#if defined(__NetBSD__) || defined(__OpenBSD__)
static struct rtentry *icmp6_mtudisc_clone __P((struct sockaddr *));
static void icmp6_mtudisc_timeout __P((struct rtentry *, struct rttimer *));
static void icmp6_redirect_timeout __P((struct rtentry *, struct rttimer *));
#endif

void
icmp6_init()
{
	mld6_init();
#if defined(__NetBSD__) || defined(__OpenBSD__)
	icmp6_mtudisc_timeout_q = rt_timer_queue_create(pmtu_expire);
	icmp6_redirect_timeout_q = rt_timer_queue_create(icmp6_redirtimeout);
#endif
}

static void
icmp6_errcount(stat, type, code)
	struct icmp6errstat *stat;
	int type, code;
{
	switch (type) {
	case ICMP6_DST_UNREACH:
		switch (code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			stat->icp6errs_dst_unreach_noroute++;
			return;
		case ICMP6_DST_UNREACH_ADMIN:
			stat->icp6errs_dst_unreach_admin++;
			return;
		case ICMP6_DST_UNREACH_BEYONDSCOPE:
			stat->icp6errs_dst_unreach_beyondscope++;
			return;
		case ICMP6_DST_UNREACH_ADDR:
			stat->icp6errs_dst_unreach_addr++;
			return;
		case ICMP6_DST_UNREACH_NOPORT:
			stat->icp6errs_dst_unreach_noport++;
			return;
		}
		break;
	case ICMP6_PACKET_TOO_BIG:
		stat->icp6errs_packet_too_big++;
		return;
	case ICMP6_TIME_EXCEEDED:
		switch (code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
			stat->icp6errs_time_exceed_transit++;
			return;
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			stat->icp6errs_time_exceed_reassembly++;
			return;
		}
		break;
	case ICMP6_PARAM_PROB:
		switch (code) {
		case ICMP6_PARAMPROB_HEADER:
			stat->icp6errs_paramprob_header++;
			return;
		case ICMP6_PARAMPROB_NEXTHEADER:
			stat->icp6errs_paramprob_nextheader++;
			return;
		case ICMP6_PARAMPROB_OPTION:
			stat->icp6errs_paramprob_option++;
			return;
		}
		break;
	case ND_REDIRECT:
		stat->icp6errs_redirect++;
		return;
	}
	stat->icp6errs_unknown++;
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
/*
 * Register a Path MTU Discovery callback.
 */
void
icmp6_mtudisc_callback_register(func)
	void (*func) __P((struct in6_addr *));
{
	struct icmp6_mtudisc_callback *mc;

	for (mc = LIST_FIRST(&icmp6_mtudisc_callbacks); mc != NULL;
	     mc = LIST_NEXT(mc, mc_list)) {
		if (mc->mc_func == func)
			return;
	}

	mc = malloc(sizeof(*mc), M_PCB, M_NOWAIT);
	if (mc == NULL)
		panic("icmp6_mtudisc_callback_register");

	mc->mc_func = func;
	LIST_INSERT_HEAD(&icmp6_mtudisc_callbacks, mc, mc_list);
}
#endif

/*
 * Generate an error packet of type error in response to bad IP6 packet.
 */
void
icmp6_error(m, type, code, param)
	struct mbuf *m;
	int type, code, param;
{
	struct ip6_hdr *oip6, *nip6;
	struct icmp6_hdr *icmp6;
	struct mbuf *n;
	struct ip6aux *ip6a;
	struct in6_addr nip6_src, *nip6_srcp;
	u_int preplen;
	int off;
	int nxt;

	icmp6stat.icp6s_error++;

	/* count per-type-code statistics */
	icmp6_errcount(&icmp6stat.icp6s_outerrhist, type, code);

#ifdef M_DECRYPTED	/* not openbsd */
	if (m->m_flags & M_DECRYPTED) {
		icmp6stat.icp6s_canterror++;
		goto freeit;
	}
#endif

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, 0, sizeof(struct ip6_hdr), );
#else
	if (m->m_len < sizeof(struct ip6_hdr)) {
		m = m_pullup(m, sizeof(struct ip6_hdr));
		if (m == NULL)
			return;
	}
#endif
	oip6 = mtod(m, struct ip6_hdr *);

	/*
	 * If the destination address of the erroneous packet is a multicast
	 * address, or the packet was sent using link-layer multicast,
	 * we should basically suppress sending an error (RFC 2463, Section
	 * 2.4).
	 * We have two exceptions (the item e.2 in that section):
	 * - the Pakcet Too Big message can be sent for path MTU discovery.
	 * - the Parameter Problem Message that can be allowed an icmp6 error
	 *   in the option type field.  This check has been done in
	 *   ip6_unknown_opt(), so we can just check the type and code.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST) ||
	     IN6_IS_ADDR_MULTICAST(&oip6->ip6_dst)) &&
	    (type != ICMP6_PACKET_TOO_BIG &&
	     (type != ICMP6_PARAM_PROB ||
	      code != ICMP6_PARAMPROB_OPTION)))
		goto freeit;

	/*
	 * RFC 2463, 2.4 (e.5): source address check.
	 * XXX: the case of anycast source?
	 */
	if (IN6_IS_ADDR_UNSPECIFIED(&oip6->ip6_src) ||
	    IN6_IS_ADDR_MULTICAST(&oip6->ip6_src))
		goto freeit;

	/*
	 * If we are about to send ICMPv6 against ICMPv6 error/redirect,
	 * don't do it.
	 */
	nxt = -1;
	off = ip6_lasthdr(m, 0, IPPROTO_IPV6, &nxt);
	if (off >= 0 && nxt == IPPROTO_ICMPV6) {
		struct icmp6_hdr *icp;

#ifndef PULLDOWN_TEST
		IP6_EXTHDR_CHECK(m, 0, off + sizeof(struct icmp6_hdr), );
		icp = (struct icmp6_hdr *)(mtod(m, caddr_t) + off);
#else
		IP6_EXTHDR_GET(icp, struct icmp6_hdr *, m, off,
			sizeof(*icp));
		if (icp == NULL) {
			icmp6stat.icp6s_tooshort++;
			return;
		}
#endif
		if (icp->icmp6_type < ICMP6_ECHO_REQUEST ||
		    icp->icmp6_type == ND_REDIRECT) {
			/*
			 * ICMPv6 error
			 * Special case: for redirect (which is
			 * informational) we must not send icmp6 error.
			 */
			icmp6stat.icp6s_canterror++;
			goto freeit;
		} else {
			/* ICMPv6 informational - send the error */
		}
	}
#if 0 /* controversial */
	else if (off >= 0 && nxt == IPPROTO_ESP) {
		/*
		 * It could be ICMPv6 error inside ESP.  Take a safer side,
		 * don't respond.
		 */
		icmp6stat.icp6s_canterror++;
		goto freeit;
	}
#endif
	else {
		/* non-ICMPv6 - send the error */
	}

	oip6 = mtod(m, struct ip6_hdr *); /* adjust pointer */

	/* Finally, do rate limitation check. */
	if (icmp6_ratelimit(&oip6->ip6_src, type, code)) {
		icmp6stat.icp6s_toofreq++;
		goto freeit;
	}

	/*
	 * OK, ICMP6 can be generated.
	 */

	/*
	 * Recover the original ip6_src if the src and the homeaddr
	 * have been swapped while dest6 processing (see dest6.c).  To
	 * avoid m_pulldown, we had better to swap addresses before
	 * m_prepend below.
	 *
	 * XXX: We should consider the other candidate to keep the
	 * icmp6 error packet correct that not swapping ip6_src and
	 * homeaddr in the dest6 processing.
	 */
	nip6_srcp = &oip6->ip6_src;
	n = ip6_findaux(m);
	if (n != NULL) {
		ip6a = mtod(n, struct ip6aux *);
		if ((ip6a->ip6a_flags & IP6A_HASEEN) != 0 &&
		    (ip6a->ip6a_flags & IP6A_SWAP) != 0) {
			nip6_src = oip6->ip6_src;
			nip6_srcp = &nip6_src;
			if (icmp6_recover_src(m)) {
				/* mbuf is freed in icmp6_recover_src */
				return;
			}
			oip6 = mtod(m, struct ip6_hdr *); /* adjust pointer */
		}
	}

	if (m->m_pkthdr.len >= ICMPV6_PLD_MAXLEN)
		m_adj(m, ICMPV6_PLD_MAXLEN - m->m_pkthdr.len);

	preplen = sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr);
	M_PREPEND(m, preplen, M_DONTWAIT);
	if (m && m->m_len < preplen)
		m = m_pullup(m, preplen);
	if (m == NULL) {
		nd6log((LOG_DEBUG, "ENOBUFS in icmp6_error %d\n", __LINE__));
		return;
	}

	nip6 = mtod(m, struct ip6_hdr *);
	nip6->ip6_src  = *nip6_srcp;
	nip6->ip6_dst  = oip6->ip6_dst;

	if (IN6_IS_SCOPE_LINKLOCAL(&oip6->ip6_src))
		oip6->ip6_src.s6_addr16[1] = 0;
	if (IN6_IS_SCOPE_LINKLOCAL(&oip6->ip6_dst))
		oip6->ip6_dst.s6_addr16[1] = 0;

	icmp6 = (struct icmp6_hdr *)(nip6 + 1);
	icmp6->icmp6_type = type;
	icmp6->icmp6_code = code;
	icmp6->icmp6_pptr = htonl((u_int32_t)param);

	/*
	 * icmp6_reflect() is designed to be in the input path.
	 * icmp6_error() can be called from both input and outut path,
	 * and if we are in output path rcvif could contain bogus value.
	 * clear m->m_pkthdr.rcvif for safety, we should have enough scope
	 * information in ip header (nip6).
	 */
	m->m_pkthdr.rcvif = NULL;

	icmp6stat.icp6s_outhist[type]++;
	icmp6_reflect(m, sizeof(struct ip6_hdr)); /* header order: IPv6 - ICMPv6 */

	return;

  freeit:
	/*
	 * If we can't tell wheter or not we can generate ICMP6, free it.
	 */
	m_freem(m);
}

/*
 * Process a received ICMP6 message.
 */
int
icmp6_input(mp, offp, proto)
	struct mbuf **mp;
	int *offp, proto;
{
	struct mbuf *m = *mp, *n;
	struct ip6_hdr *ip6, *nip6;
	struct icmp6_hdr *icmp6, *nicmp6;
	int off = *offp;
	int icmp6len = m->m_pkthdr.len - *offp;
	int code, sum, noff;

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, sizeof(struct icmp6_hdr), IPPROTO_DONE);
	/* m might change if M_LOOP.  So, call mtod after this */
#endif

	/*
	 * Locate icmp6 structure in mbuf, and check
	 * that not corrupted and of at least minimum length
	 */

	ip6 = mtod(m, struct ip6_hdr *);
	if (icmp6len < sizeof(struct icmp6_hdr)) {
		icmp6stat.icp6s_tooshort++;
		goto freeit;
	}

	/*
	 * calculate the checksum
	 */
#ifndef PULLDOWN_TEST
	icmp6 = (struct icmp6_hdr *)((caddr_t)ip6 + off);
#else
	IP6_EXTHDR_GET(icmp6, struct icmp6_hdr *, m, off, sizeof(*icmp6));
	if (icmp6 == NULL) {
		icmp6stat.icp6s_tooshort++;
		return IPPROTO_DONE;
	}
#endif
	code = icmp6->icmp6_code;

	if ((sum = in6_cksum(m, IPPROTO_ICMPV6, off, icmp6len)) != 0) {
		nd6log((LOG_ERR,
		    "ICMP6 checksum error(%d|%x) %s\n",
		    icmp6->icmp6_type, sum, ip6_sprintf(&ip6->ip6_src)));
		icmp6stat.icp6s_checksum++;
		goto freeit;
	}

#if defined(NFAITH) && 0 < NFAITH
	if (faithprefix(&ip6->ip6_dst)) {
		/*
		 * Deliver very specific ICMP6 type only.
		 * This is important to deilver TOOBIG.  Otherwise PMTUD
		 * will not work.
		 */
		switch (icmp6->icmp6_type) {
		case ICMP6_DST_UNREACH:
		case ICMP6_PACKET_TOO_BIG:
		case ICMP6_TIME_EXCEEDED:
			break;
		default:
			goto freeit;
		}
	}
#endif

	icmp6stat.icp6s_inhist[icmp6->icmp6_type]++;
	icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_msg);
	if (icmp6->icmp6_type < ICMP6_INFOMSG_MASK)
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_error);

#ifdef MIP6
	if (mip6_icmp6_tunnel_input(m, off, icmp6len)) {
		goto freeit;
	}
#endif /* MIP6 */
	switch (icmp6->icmp6_type) {
	case ICMP6_DST_UNREACH:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_dstunreach);
#ifdef MIP6
		if (mip6_icmp6_input(m, off, icmp6len))
			goto freeit;
#endif /* MIP6 */
		switch (code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			code = PRC_UNREACH_NET;
			break;
		case ICMP6_DST_UNREACH_ADMIN:
			icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_adminprohib);
			code = PRC_UNREACH_PROTOCOL; /* is this a good code? */
			break;
		case ICMP6_DST_UNREACH_ADDR:
			code = PRC_HOSTDEAD;
			break;
#ifdef COMPAT_RFC1885
		case ICMP6_DST_UNREACH_NOTNEIGHBOR:
			code = PRC_UNREACH_SRCFAIL;
			break;
#else
		case ICMP6_DST_UNREACH_BEYONDSCOPE:
			/* I mean "source address was incorrect." */
			code = PRC_UNREACH_NET;
			break;
#endif
		case ICMP6_DST_UNREACH_NOPORT:
			code = PRC_UNREACH_PORT;
			break;
		default:
			goto badcode;
		}
		goto deliver;
		break;

	case ICMP6_PACKET_TOO_BIG:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_pkttoobig);
		if (code != 0)
			goto badcode;

		code = PRC_MSGSIZE;

		/*
		 * Updating the path MTU will be done after examining
		 * intermediate extension headers.
		 */
		goto deliver;
		break;

	case ICMP6_TIME_EXCEEDED:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_timeexceed);
		switch (code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
			code = PRC_TIMXCEED_INTRANS;
			break;
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			code = PRC_TIMXCEED_REASS;
			break;
		default:
			goto badcode;
		}
		goto deliver;
		break;

	case ICMP6_PARAM_PROB:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_paramprob);
		switch (code) {
		case ICMP6_PARAMPROB_NEXTHEADER:
			code = PRC_UNREACH_PROTOCOL;
			break;
		case ICMP6_PARAMPROB_HEADER:
		case ICMP6_PARAMPROB_OPTION:
#ifdef MIP6
			if (mip6_icmp6_input(m, off, icmp6len))
				goto freeit;
#endif /* MIP6 */
			code = PRC_PARAMPROB;
			break;
		default:
			goto badcode;
		}
		goto deliver;
		break;

	case ICMP6_ECHO_REQUEST:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_echo);
		if (code != 0)
			goto badcode;
		/*
		 * Copy mbuf to send to two data paths: userland socket(s),
		 * and to the querier (echo reply).
		 * m: a copy for socket, n: a copy for querier
		 */
		if ((n = m_copym(m, 0, M_COPYALL, M_DONTWAIT)) == NULL) {
			/* Give up local */
			n = m;
			m = NULL;
			goto deliverecho;
		}
		/*
		 * If the first mbuf is shared, or the first mbuf is too short,
		 * copy the first part of the data into a fresh mbuf.
		 * Otherwise, we will wrongly overwrite both copies.
		 */
		if ((n->m_flags & M_EXT) != 0 ||
		    n->m_len < off + sizeof(struct icmp6_hdr)) {
			struct mbuf *n0 = n;
			const int maxlen = sizeof(*nip6) + sizeof(*nicmp6);

			/*
			 * Prepare an internal mbuf.  m_pullup() doesn't
			 * always copy the length we specified.
			 */
			if (maxlen >= MCLBYTES) {
				/* Give up remote */
				m_freem(n0);
				break;
			}
			MGETHDR(n, M_DONTWAIT, n0->m_type);
			if (n && maxlen >= MHLEN) {
				MCLGET(n, M_DONTWAIT);
				if ((n->m_flags & M_EXT) == 0) {
					m_free(n);
					n = NULL;
				}
			}
			if (n == NULL) {
				/* Give up local */
				m_freem(n0);
				n = m;
				m = NULL;
				goto deliverecho;
			}
			M_COPY_PKTHDR(n, n0);
			/*
			 * Copy IPv6 and ICMPv6 only.
			 */
			nip6 = mtod(n, struct ip6_hdr *);
			bcopy(ip6, nip6, sizeof(struct ip6_hdr));
			nicmp6 = (struct icmp6_hdr *)(nip6 + 1);
			bcopy(icmp6, nicmp6, sizeof(struct icmp6_hdr));
			noff = sizeof(struct ip6_hdr);
			n->m_len = noff + sizeof(struct icmp6_hdr);
			/*
			 * Adjust mbuf.  ip6_plen will be adjusted in
			 * ip6_output().
			 * n->m_pkthdr.len == n0->m_pkthdr.len at this point.
			 */
			n->m_pkthdr.len += noff + sizeof(struct icmp6_hdr);
			n->m_pkthdr.len -= (off + sizeof(struct icmp6_hdr));
			m_adj(n0, off + sizeof(struct icmp6_hdr));
			n->m_next = n0;
			n0->m_flags &= ~M_PKTHDR;
		} else {
	 deliverecho:
			nip6 = mtod(n, struct ip6_hdr *);
			nicmp6 = (struct icmp6_hdr *)((caddr_t)nip6 + off);
			noff = off;
		}
		nicmp6->icmp6_type = ICMP6_ECHO_REPLY;
		nicmp6->icmp6_code = 0;
		if (n) {
			icmp6stat.icp6s_reflect++;
			icmp6stat.icp6s_outhist[ICMP6_ECHO_REPLY]++;
			icmp6_reflect(n, noff);
		}
		if (!m)
			goto freeit;
		break;

	case ICMP6_ECHO_REPLY:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_echoreply);
		if (code != 0)
			goto badcode;
		break;

#ifdef MIP6
	case ICMP6_HADISCOV_REQUEST:
	case ICMP6_HADISCOV_REPLY:
		if (code != 0)
			goto badcode;
		if (mip6_icmp6_input(m, off, icmp6len))
			goto freeit;
		break;
	case ICMP6_MOBILEPREFIX_SOLICIT:
	case ICMP6_MOBILEPREFIX_ADVERT:
		if (code != 0)
			goto badcode;
		if (mip6_icmp6_input(m, off, icmp6len))
			goto freeit;
		break;
#endif /* MIP6 */

	case MLD6_LISTENER_QUERY:
	case MLD6_LISTENER_REPORT:
		if (icmp6len < sizeof(struct mld6_hdr))
			goto badlen;
		if (icmp6->icmp6_type == MLD6_LISTENER_QUERY) /* XXX: ugly... */
			icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_mldquery);
		else
			icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_mldreport);
		if ((n = m_copym(m, 0, M_COPYALL, M_DONTWAIT)) == NULL) {
			/* give up local */
			mld6_input(m, off);
			m = NULL;
			goto freeit;
		}
		mld6_input(n, off);
		/* m stays. */
		break;

	case MLD6_LISTENER_DONE:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_mlddone);
		if (icmp6len < sizeof(struct mld6_hdr))	/* necessary? */
			goto badlen;
		break;		/* nothing to be done in kernel */

	case MLD6_MTRACE_RESP:
	case MLD6_MTRACE:
		/* XXX: these two are experimental.  not officially defined. */
		/* XXX: per-interface statistics? */
		break;		/* just pass it to applications */

	case ICMP6_WRUREQUEST:	/* ICMP6_FQDN_QUERY */
	    {
		enum { WRU, FQDN } mode;

		if (!icmp6_nodeinfo)
			break;

		if (icmp6len == sizeof(struct icmp6_hdr) + 4)
			mode = WRU;
		else if (icmp6len >= sizeof(struct icmp6_nodeinfo))
			mode = FQDN;
		else
			goto badlen;

#ifdef __FreeBSD__
#define hostnamelen	strlen(hostname)
#endif
		if (mode == FQDN) {
#ifndef PULLDOWN_TEST
			IP6_EXTHDR_CHECK(m, off, sizeof(struct icmp6_nodeinfo),
					 IPPROTO_DONE);
#endif
			n = m_copym(m, 0, M_COPYALL, M_DONTWAIT);
			if (n)
				n = ni6_input(n, off);
			/* XXX meaningless if n == NULL */
			noff = sizeof(struct ip6_hdr);
		} else {
			u_char *p;
			int maxlen, maxhlen;

			if ((icmp6_nodeinfo & 5) != 5) 
				break;

			if (code != 0)
				goto badcode;
			maxlen = sizeof(*nip6) + sizeof(*nicmp6) + 4;
			if (maxlen >= MCLBYTES) {
				/* Give up remote */
				break;
			}
			MGETHDR(n, M_DONTWAIT, m->m_type);
			if (n && maxlen > MHLEN) {
				MCLGET(n, M_DONTWAIT);
				if ((n->m_flags & M_EXT) == 0) {
					m_free(n);
					n = NULL;
				}
			}
			if (n == NULL) {
				/* Give up remote */
				break;
			}
			n->m_pkthdr.rcvif = NULL;
			n->m_len = 0;
			maxhlen = M_TRAILINGSPACE(n) - maxlen;
			if (maxhlen > hostnamelen)
				maxhlen = hostnamelen;
			/*
			 * Copy IPv6 and ICMPv6 only.
			 */
			nip6 = mtod(n, struct ip6_hdr *);
			bcopy(ip6, nip6, sizeof(struct ip6_hdr));
			nicmp6 = (struct icmp6_hdr *)(nip6 + 1);
			bcopy(icmp6, nicmp6, sizeof(struct icmp6_hdr));
			p = (u_char *)(nicmp6 + 1);
			bzero(p, 4);
			bcopy(hostname, p + 4, maxhlen); /* meaningless TTL */
			noff = sizeof(struct ip6_hdr);
			M_COPY_PKTHDR(n, m); /* just for recvif */
			n->m_pkthdr.len = n->m_len = sizeof(struct ip6_hdr) +
				sizeof(struct icmp6_hdr) + 4 + maxhlen;
			nicmp6->icmp6_type = ICMP6_WRUREPLY;
			nicmp6->icmp6_code = 0;
		}
#undef hostnamelen
		if (n) {
			icmp6stat.icp6s_reflect++;
			icmp6stat.icp6s_outhist[ICMP6_WRUREPLY]++;
			icmp6_reflect(n, noff);
		}
		break;
	    }

	case ICMP6_WRUREPLY:
		if (code != 0)
			goto badcode;
		break;

	case ND_ROUTER_SOLICIT:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_routersolicit);
		if (code != 0)
			goto badcode;
		if (icmp6len < sizeof(struct nd_router_solicit))
			goto badlen;
		if ((n = m_copym(m, 0, M_COPYALL, M_DONTWAIT)) == NULL) {
			/* give up local */
			nd6_rs_input(m, off, icmp6len);
			m = NULL;
			goto freeit;
		}
		nd6_rs_input(n, off, icmp6len);
		/* m stays. */
		break;

	case ND_ROUTER_ADVERT:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_routeradvert);
		if (code != 0)
			goto badcode;
		if (icmp6len < sizeof(struct nd_router_advert))
			goto badlen;
		if ((n = m_copym(m, 0, M_COPYALL, M_DONTWAIT)) == NULL) {
			/* give up local */
			nd6_ra_input(m, off, icmp6len);
			m = NULL;
			goto freeit;
		}
		nd6_ra_input(n, off, icmp6len);
		/* m stays. */
		break;

	case ND_NEIGHBOR_SOLICIT:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_neighborsolicit);
		if (code != 0)
			goto badcode;
		if (icmp6len < sizeof(struct nd_neighbor_solicit))
			goto badlen;
		if ((n = m_copym(m, 0, M_COPYALL, M_DONTWAIT)) == NULL) {
			/* give up local */
			nd6_ns_input(m, off, icmp6len);
			m = NULL;
			goto freeit;
		}
		nd6_ns_input(n, off, icmp6len);
		/* m stays. */
		break;

	case ND_NEIGHBOR_ADVERT:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_neighboradvert);
		if (code != 0)
			goto badcode;
		if (icmp6len < sizeof(struct nd_neighbor_advert))
			goto badlen;
		if ((n = m_copym(m, 0, M_COPYALL, M_DONTWAIT)) == NULL) {
			/* give up local */
			nd6_na_input(m, off, icmp6len);
			m = NULL;
			goto freeit;
		}
		nd6_na_input(n, off, icmp6len);
		/* m stays. */
		break;

	case ND_REDIRECT:
		icmp6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_redirect);
		if (code != 0)
			goto badcode;
		if (icmp6len < sizeof(struct nd_redirect))
			goto badlen;
		if ((n = m_copym(m, 0, M_COPYALL, M_DONTWAIT)) == NULL) {
			/* give up local */
			icmp6_redirect_input(m, off);
			m = NULL;
			goto freeit;
		}
		icmp6_redirect_input(n, off);
		/* m stays. */
		break;

	case ICMP6_ROUTER_RENUMBERING:
		if (code != ICMP6_ROUTER_RENUMBERING_COMMAND &&
		    code != ICMP6_ROUTER_RENUMBERING_RESULT)
			goto badcode;
		if (icmp6len < sizeof(struct icmp6_router_renum))
			goto badlen;
		break;

	default:
		nd6log((LOG_DEBUG,
		    "icmp6_input: unknown type %d(src=%s, dst=%s, ifid=%d)\n",
		    icmp6->icmp6_type, ip6_sprintf(&ip6->ip6_src),
		    ip6_sprintf(&ip6->ip6_dst),
		    m->m_pkthdr.rcvif ? m->m_pkthdr.rcvif->if_index : 0));
		if (icmp6->icmp6_type < ICMP6_ECHO_REQUEST) {
			/* ICMPv6 error: MUST deliver it by spec... */
			code = PRC_NCMDS;
			/* deliver */
		} else {
			/* ICMPv6 informational: MUST not deliver */
			break;
		}
	deliver:
		if (icmp6_notify_error(m, off, icmp6len, code)) {
			/* In this case, m should've been freed. */
			return(IPPROTO_DONE);
		}
		break;

	badcode:
		icmp6stat.icp6s_badcode++;
		break;

	badlen:
		icmp6stat.icp6s_badlen++;
		break;
	}

	/* deliver the packet to appropriate sockets */
	icmp6_rip6_input(&m, *offp);

	return IPPROTO_DONE;

 freeit:
	m_freem(m);
	return IPPROTO_DONE;
}

static int
icmp6_notify_error(m, off, icmp6len, code)
	struct mbuf *m;
	int off, icmp6len;
{
	struct icmp6_hdr *icmp6;
	struct ip6_hdr *eip6;
	u_int32_t notifymtu;
	int64_t zoneid;
	struct sockaddr_in6 icmp6src, icmp6dst;

	if (icmp6len < sizeof(struct icmp6_hdr) + sizeof(struct ip6_hdr)) {
		icmp6stat.icp6s_tooshort++;
		goto freeit;
	}
#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off,
			 sizeof(struct icmp6_hdr) + sizeof(struct ip6_hdr),
			 -1);
	icmp6 = (struct icmp6_hdr *)(mtod(m, caddr_t) + off);
#else
	IP6_EXTHDR_GET(icmp6, struct icmp6_hdr *, m, off,
		       sizeof(*icmp6) + sizeof(struct ip6_hdr));
	if (icmp6 == NULL) {
		icmp6stat.icp6s_tooshort++;
		return(-1);
	}
#endif
	eip6 = (struct ip6_hdr *)(icmp6 + 1);

	/* Detect the upper level protocol */
	{
		void (*ctlfunc) __P((int, struct sockaddr *, void *));
		u_int8_t nxt = eip6->ip6_nxt;
		int eoff = off + sizeof(struct icmp6_hdr) +
			sizeof(struct ip6_hdr);
		struct ip6ctlparam ip6cp;
		struct in6_addr *finaldst = NULL;
		int icmp6type = icmp6->icmp6_type;
		struct ip6_frag *fh;
		struct ip6_rthdr *rth;
		struct ip6_rthdr0 *rth0;
		int rthlen;

		while (1) { /* XXX: should avoid infinite loop explicitly? */
			struct ip6_ext *eh;

			switch (nxt) {
			case IPPROTO_HOPOPTS:
			case IPPROTO_DSTOPTS:
			case IPPROTO_AH:
#ifndef PULLDOWN_TEST
				IP6_EXTHDR_CHECK(m, 0, eoff +
						 sizeof(struct ip6_ext),
						 -1);
				eh = (struct ip6_ext *)(mtod(m, caddr_t)
							+ eoff);
#else
				IP6_EXTHDR_GET(eh, struct ip6_ext *, m,
					       eoff, sizeof(*eh));
				if (eh == NULL) {
					icmp6stat.icp6s_tooshort++;
					return(-1);
				}
#endif
				
				if (nxt == IPPROTO_AH)
					eoff += (eh->ip6e_len + 2) << 2;
				else
					eoff += (eh->ip6e_len + 1) << 3;
				nxt = eh->ip6e_nxt;
				break;
			case IPPROTO_ROUTING:
				/*
				 * When the erroneous packet contains a
				 * routing header, we should examine the
				 * header to determine the final destination.
				 * Otherwise, we can't properly update
				 * information that depends on the final
				 * destination (e.g. path MTU).
				 */
#ifndef PULLDOWN_TEST
				IP6_EXTHDR_CHECK(m, 0, eoff + sizeof(*rth),
						 -1);
				rth = (struct ip6_rthdr *)(mtod(m, caddr_t)
							   + eoff);
#else
				IP6_EXTHDR_GET(rth, struct ip6_rthdr *, m,
					       eoff, sizeof(*rth));
				if (rth == NULL) {
					icmp6stat.icp6s_tooshort++;
					return(-1);
				}
#endif
				rthlen = (rth->ip6r_len + 1) << 3;
				/*
				 * XXX: currently there is no
				 * officially defined type other
				 * than type-0.
				 * Note that if the segment left field
				 * is 0, all intermediate hops must
				 * have been passed.
				 */
				if (rth->ip6r_segleft &&
				    rth->ip6r_type == IPV6_RTHDR_TYPE_0) {
					int hops;

#ifndef PULLDOWN_TEST
					IP6_EXTHDR_CHECK(m, 0, eoff + rthlen,
							 -1);
					rth0 = (struct ip6_rthdr0 *)(mtod(m, caddr_t) + eoff);
#else
					IP6_EXTHDR_GET(rth0,
						       struct ip6_rthdr0 *, m,
						       eoff, rthlen);
					if (rth0 == NULL) {
						icmp6stat.icp6s_tooshort++;
						return(-1);
					}
#endif
					/* just ignore a bogus header */
					if ((rth0->ip6r0_len % 2) == 0 &&
					    (hops = rth0->ip6r0_len/2))
						finaldst = (struct in6_addr *)(rth0 + 1) + (hops - 1);
				}
				eoff += rthlen;
				nxt = rth->ip6r_nxt;
				break;
			case IPPROTO_FRAGMENT:
#ifndef PULLDOWN_TEST
				IP6_EXTHDR_CHECK(m, 0, eoff +
						 sizeof(struct ip6_frag),
						 -1);
				fh = (struct ip6_frag *)(mtod(m, caddr_t)
							 + eoff);
#else
				IP6_EXTHDR_GET(fh, struct ip6_frag *, m,
					       eoff, sizeof(*fh));
				if (fh == NULL) {
					icmp6stat.icp6s_tooshort++;
					return(-1);
				}
#endif
				/*
				 * Data after a fragment header is meaningless
				 * unless it is the first fragment, but
				 * we'll go to the notify label for path MTU
				 * discovery.
				 */
				if (fh->ip6f_offlg & IP6F_OFF_MASK)
					goto notify;

				eoff += sizeof(struct ip6_frag);
				nxt = fh->ip6f_nxt;
				break;
			default:
				/*
				 * This case includes ESP and the No Next
				 * Header.  In such cases going to the notify
				 * label does not have any meaning
				 * (i.e. ctlfunc will be NULL), but we go
				 * anyway since we might have to update
				 * path MTU information.
				 */
				goto notify;
			}
		}
	  notify:
#ifndef PULLDOWN_TEST
		icmp6 = (struct icmp6_hdr *)(mtod(m, caddr_t) + off);
#else
		IP6_EXTHDR_GET(icmp6, struct icmp6_hdr *, m, off,
			       sizeof(*icmp6) + sizeof(struct ip6_hdr));
		if (icmp6 == NULL) {
			icmp6stat.icp6s_tooshort++;
			return(-1);
		}
#endif

		eip6 = (struct ip6_hdr *)(icmp6 + 1);
		bzero(&icmp6dst, sizeof(icmp6dst));
		icmp6dst.sin6_len = sizeof(struct sockaddr_in6);
		icmp6dst.sin6_family = AF_INET6;
		if (finaldst == NULL)
			icmp6dst.sin6_addr = eip6->ip6_dst;
		else
			icmp6dst.sin6_addr = *finaldst;
		if ((zoneid = in6_addr2zoneid(m->m_pkthdr.rcvif,
					      &icmp6dst.sin6_addr)) < 0)
			goto freeit;
		icmp6dst.sin6_scope_id = zoneid;
#ifndef SCOPEDROUTING
		if (in6_embedscope(&icmp6dst.sin6_addr, &icmp6dst,
				   NULL, NULL)) {
			/* should be impossbile */
			nd6log((LOG_DEBUG,
			    "icmp6_notify_error: in6_embedscope failed\n"));
			goto freeit;
		}
#endif

		/*
		 * retrieve parameters from the inner IPv6 header, and convert
		 * them into sockaddr structures.
		 */
		bzero(&icmp6src, sizeof(icmp6src));
		icmp6src.sin6_len = sizeof(struct sockaddr_in6);
		icmp6src.sin6_family = AF_INET6;
		icmp6src.sin6_addr = eip6->ip6_src;
		if ((zoneid = in6_addr2zoneid(m->m_pkthdr.rcvif,
					      &icmp6src.sin6_addr)) < 0)
			goto freeit;
		icmp6src.sin6_scope_id = zoneid;
#ifndef SCOPEDROUTING
		if (in6_embedscope(&icmp6src.sin6_addr, &icmp6src,
				   NULL, NULL)) {
			/* should be impossbile */
			nd6log((LOG_DEBUG,
			    "icmp6_notify_error: in6_embedscope failed\n"));
			goto freeit;
		}
#endif
		icmp6src.sin6_flowinfo =
			(eip6->ip6_flow & IPV6_FLOWLABEL_MASK);

		if (finaldst == NULL)
			finaldst = &eip6->ip6_dst;
		ip6cp.ip6c_m = m;
		ip6cp.ip6c_icmp6 = icmp6;
		ip6cp.ip6c_ip6 = (struct ip6_hdr *)(icmp6 + 1);
		ip6cp.ip6c_off = eoff;
		ip6cp.ip6c_finaldst = finaldst;
		ip6cp.ip6c_src = &icmp6src;
		ip6cp.ip6c_nxt = nxt;

		if (icmp6type == ICMP6_PACKET_TOO_BIG) {
			notifymtu = ntohl(icmp6->icmp6_mtu);
			ip6cp.ip6c_cmdarg = (void *)&notifymtu;
#if !(defined(__NetBSD__) || defined(__OpenBSD__))
			icmp6_mtudisc_update(&ip6cp, 1);	/* XXX */
#endif
		}

		ctlfunc = (void (*) __P((int, struct sockaddr *, void *)))
			(inet6sw[ip6_protox[nxt]].pr_ctlinput);
		if (ctlfunc) {
			(void) (*ctlfunc)(code, (struct sockaddr *)&icmp6dst,
					  &ip6cp);
		}
	}
	return(0);

  freeit:
	m_freem(m);
	return(-1);
}

void
icmp6_mtudisc_update(ip6cp, validated)
	struct ip6ctlparam *ip6cp;
	int validated;
{
#if defined(__NetBSD__) || defined(__OpenBSD__)
	unsigned long rtcount;
	struct icmp6_mtudisc_callback *mc;
#endif
	struct in6_addr *dst = ip6cp->ip6c_finaldst;
	struct icmp6_hdr *icmp6 = ip6cp->ip6c_icmp6;
	struct mbuf *m = ip6cp->ip6c_m;	/* will be necessary for scope issue */
	u_int mtu = ntohl(icmp6->icmp6_mtu);
	struct rtentry *rt = NULL;
	struct sockaddr_in6 sin6;
#ifdef __bsdi__
#ifdef NEW_STRUCT_ROUTE
	struct route ro6;
#else
	struct route_in6 ro6;
#endif
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
	/*
	 * allow non-validated cases if memory is plenty, to make traffic
	 * from non-connected pcb happy.
	 */
	rtcount = rt_timer_count(icmp6_mtudisc_timeout_q);
	if (validated) {
		if (0 <= icmp6_mtudisc_hiwat && rtcount > icmp6_mtudisc_hiwat)
			return;
		else if (0 <= icmp6_mtudisc_lowat &&
		    rtcount > icmp6_mtudisc_lowat) {
			/*
			 * XXX nuke a victim, install the new one.
			 */
		}
	} else {
		if (0 <= icmp6_mtudisc_lowat && rtcount > icmp6_mtudisc_lowat)
			return;
	}
#else
	if (!validated)
		return;
#endif

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_family = PF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	sin6.sin6_addr = *dst;
	/* XXX normally, this won't happen */
	if (IN6_IS_ADDR_LINKLOCAL(dst)) {
		sin6.sin6_addr.s6_addr16[1] =
		    htons(m->m_pkthdr.rcvif->if_index);
	}
	/* sin6.sin6_scope_id = XXX: should be set if DST is a scoped addr */
#if defined(__NetBSD__) || defined(__OpenBSD__)
	rt = icmp6_mtudisc_clone((struct sockaddr *)&sin6);
#else
#ifdef __FreeBSD__
	rt = rtalloc1((struct sockaddr *)&sin6, 0,
		      RTF_CLONING | RTF_PRCLONING);
#else
#ifdef __bsdi__
	bcopy(&sin6, &ro6.ro_dst, sizeof(struct sockaddr_in6));
	ro6.ro_rt = 0;
	/* rtcalloc((struct route *)&ro6); */
	rtalloc((struct route *)&ro6);
	rt = ro6.ro_rt;
#else
#error no case for this particular operating system
#endif
#endif
#endif

	if (rt && (rt->rt_flags & RTF_HOST)
	    && !(rt->rt_rmx.rmx_locks & RTV_MTU)) {
		if (mtu < IPV6_MMTU) {
				/* xxx */
			rt->rt_rmx.rmx_locks |= RTV_MTU;
		} else if (mtu < rt->rt_ifp->if_mtu &&
			   rt->rt_rmx.rmx_mtu > mtu) {
			icmp6stat.icp6s_pmtuchg++;
			rt->rt_rmx.rmx_mtu = mtu;
		}
	}
	if (rt) { /* XXX: need braces to avoid conflict with else in RTFREE. */
		RTFREE(rt);
	}

#if defined(__NetBSD__) || defined(__OpenBSD__)
	/*
	 * Notify protocols that the MTU for this destination
	 * has changed.
	 */
	for (mc = LIST_FIRST(&icmp6_mtudisc_callbacks); mc != NULL;
	     mc = LIST_NEXT(mc, mc_list))
		(*mc->mc_func)(&sin6.sin6_addr);
#endif
}

/*
 * Process a Node Information Query packet, based on
 * draft-ietf-ipngwg-icmp-name-lookups-07.
 * 
 * Spec incompatibilities:
 * - IPv6 Subject address handling
 * - IPv4 Subject address handling support missing
 * - Proxy reply (answer even if it's not for me)
 * - joins NI group address at in6_ifattach() time only, does not cope
 *   with hostname changes by sethostname(3)
 */
#ifdef __FreeBSD__
#define hostnamelen	strlen(hostname)
#endif
#ifndef offsetof		/* XXX */
#define	offsetof(type, member)	((size_t)(&((type *)0)->member))
#endif
static struct mbuf *
ni6_input(m, off)
	struct mbuf *m;
	int off;
{
	struct icmp6_nodeinfo *ni6, *nni6;
	struct mbuf *n = NULL;
	u_int16_t qtype;
	int subjlen;
	int replylen = sizeof(struct ip6_hdr) + sizeof(struct icmp6_nodeinfo);
	struct ni_reply_fqdn *fqdn;
	int addrs;		/* for NI_QTYPE_NODEADDR */
	struct ifnet *ifp = NULL; /* for NI_QTYPE_NODEADDR */
	struct sockaddr_in6 sin6; /* double meaning; ip6_dst and subjectaddr */
	struct sockaddr_in6 sin6_d; /* XXX: we should retrieve this from m_aux */
	struct ip6_hdr *ip6;
	int oldfqdn = 0;	/* if 1, return pascal string (03 draft) */
	char *subj = NULL;
	struct in6_ifaddr *ia6 = NULL;
	int64_t zoneid;

	ip6 = mtod(m, struct ip6_hdr *);
#ifndef PULLDOWN_TEST
	ni6 = (struct icmp6_nodeinfo *)(mtod(m, caddr_t) + off);
#else
	IP6_EXTHDR_GET(ni6, struct icmp6_nodeinfo *, m, off, sizeof(*ni6));
	if (ni6 == NULL) {
		/* m is already reclaimed */
		return NULL;
	}
#endif

	/*
	 * Validate IPv6 destination address.
	 *
	 * The Responder must discard the Query without further processing
	 * unless it is one of the Responder's unicast or anycast addresses, or
	 * a link-local scope multicast address which the Responder has joined.
	 * [icmp-name-lookups-07, Section 4.]
	 */
	bzero(&sin6, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&ip6->ip6_dst, &sin6.sin6_addr, sizeof(sin6.sin6_addr));
	/* XXX scopeid */
	if ((ia6 = (struct in6_ifaddr *)ifa_ifwithaddr((struct sockaddr *)&sin6)) != NULL) {
		/* unicast/anycast, fine */
		if ((ia6->ia6_flags & IN6_IFF_TEMPORARY) != 0 &&
		    (icmp6_nodeinfo & 4) == 0) {
			nd6log((LOG_DEBUG, "ni6_input: ignore node info to "
				"a temporary address in %s:%d",
			       __FILE__, __LINE__));
			goto bad;
		}
	} else if (IN6_IS_ADDR_MC_LINKLOCAL(&sin6.sin6_addr))
		; /* link-local multicast, fine */
	else
		goto bad;

	/* validate query Subject field. */
	qtype = ntohs(ni6->ni_qtype);
	subjlen = m->m_pkthdr.len - off - sizeof(struct icmp6_nodeinfo);
	switch (qtype) {
	case NI_QTYPE_NOOP:
	case NI_QTYPE_SUPTYPES:
		/* 07 draft */
		if (ni6->ni_code == ICMP6_NI_SUBJ_FQDN && subjlen == 0)
			break;
		/* FALLTHROUGH */
	case NI_QTYPE_FQDN:
	case NI_QTYPE_NODEADDR:
		switch (ni6->ni_code) {
		case ICMP6_NI_SUBJ_IPV6:
#if ICMP6_NI_SUBJ_IPV6 != 0
		case 0:
#endif
			/*
			 * backward compatibility - try to accept 03 draft
			 * format, where no Subject is present.
			 */
			if (qtype == NI_QTYPE_FQDN && ni6->ni_code == 0 &&
			    subjlen == 0) {
				oldfqdn++;
				break;
			}
#if ICMP6_NI_SUBJ_IPV6 != 0
			if (ni6->ni_code != ICMP6_NI_SUBJ_IPV6)
				goto bad;
#endif

			if (subjlen != sizeof(sin6.sin6_addr))
				goto bad;

			/*
			 * Validate Subject address.
			 *
			 * Not sure what exactly "address belongs to the node"
			 * means in the spec, is it just unicast, or what?
			 *
			 * At this moment we consider Subject address as
			 * "belong to the node" if the Subject address equals
			 * to the IPv6 destination address; validation for
			 * IPv6 destination address should have done enough
			 * check for us.
			 *
			 * We do not do proxy at this moment.
			 */
			/* m_pulldown instead of copy? */
			m_copydata(m, off + sizeof(struct icmp6_nodeinfo),
			    subjlen, (caddr_t)&sin6.sin6_addr);
			if ((zoneid = in6_addr2zoneid(m->m_pkthdr.rcvif,
						      &sin6.sin6_addr)) < 0)
				goto bad;
			sin6.sin6_scope_id = zoneid;
							     
#ifndef SCOPEDROUTING
			in6_embedscope(&sin6.sin6_addr, &sin6, NULL, NULL);
#endif
			bzero(&sin6_d, sizeof(sin6_d));
			sin6_d.sin6_family = AF_INET6; /* not used, actually */
			sin6_d.sin6_len = sizeof(sin6_d); /* ditto */
			sin6_d.sin6_addr = ip6->ip6_dst;
			if ((zoneid = in6_addr2zoneid(m->m_pkthdr.rcvif,
						      &ip6->ip6_dst)) < 0)
				goto bad;
			sin6_d.sin6_scope_id = zoneid;
#ifndef SCOPEDROUTING
			in6_embedscope(&sin6_d.sin6_addr, &sin6_d, NULL, NULL);
#endif
			subj = (char *)&sin6;
			if (SA6_ARE_ADDR_EQUAL(&sin6, &sin6_d))
				break;

			/*
			 * XXX if we are to allow other cases, we should really
			 * be careful about scope here.
			 * basically, we should disallow queries toward IPv6
			 * destination X with subject Y, if scope(X) > scope(Y).
			 * if we allow scope(X) > scope(Y), it will result in
			 * information leakage across scope boundary.
			 */
			goto bad;

		case ICMP6_NI_SUBJ_FQDN:
			/*
			 * Validate Subject name with gethostname(3).
			 *
			 * The behavior may need some debate, since:
			 * - we are not sure if the node has FQDN as
			 *   hostname (returned by gethostname(3)).
			 * - the code does wildcard match for truncated names.
			 *   however, we are not sure if we want to perform
			 *   wildcard match, if gethostname(3) side has
			 *   truncated hostname.
			 */
			n = ni6_nametodns(hostname, hostnamelen, 0);
			if (!n || n->m_next || n->m_len == 0)
				goto bad;
			IP6_EXTHDR_GET(subj, char *, m,
			    off + sizeof(struct icmp6_nodeinfo), subjlen);
			if (subj == NULL)
				goto bad;
			if (!ni6_dnsmatch(subj, subjlen, mtod(n, const char *),
					n->m_len)) {
				goto bad;
			}
			m_freem(n);
			n = NULL;
			break;

		case ICMP6_NI_SUBJ_IPV4:	/* XXX: to be implemented? */
		default:
			goto bad;
		}
		break;
	}

	/* refuse based on configuration.  XXX ICMP6_NI_REFUSED? */
	switch (qtype) {
	case NI_QTYPE_FQDN:
		if ((icmp6_nodeinfo & 1) == 0)
			goto bad;
		break;
	case NI_QTYPE_NODEADDR:
		if ((icmp6_nodeinfo & 2) == 0)
			goto bad;
		break;
	}

	/* guess reply length */
	switch (qtype) {
	case NI_QTYPE_NOOP:
		break;		/* no reply data */
	case NI_QTYPE_SUPTYPES:
		replylen += sizeof(u_int32_t);
		break;
	case NI_QTYPE_FQDN:
		/* XXX will append an mbuf */
		replylen += offsetof(struct ni_reply_fqdn, ni_fqdn_namelen);
		break;
	case NI_QTYPE_NODEADDR:
		addrs = ni6_addrs(ni6, m, &ifp, subj);
		if ((replylen += addrs * (sizeof(struct in6_addr) +
					  sizeof(u_int32_t))) > MCLBYTES)
			replylen = MCLBYTES; /* XXX: will truncate pkt later */
		break;
	default:
		/*
		 * XXX: We must return a reply with the ICMP6 code
		 * `unknown Qtype' in this case.  However we regard the case
		 * as an FQDN query for backward compatibility.
		 * Older versions set a random value to this field,
		 * so it rarely varies in the defined qtypes.
		 * But the mechanism is not reliable...
		 * maybe we should obsolete older versions.
		 */
		qtype = NI_QTYPE_FQDN;
		/* XXX will append an mbuf */
		replylen += offsetof(struct ni_reply_fqdn, ni_fqdn_namelen);
		oldfqdn++;
		break;
	}

	/* allocate an mbuf to reply. */
	MGETHDR(n, M_DONTWAIT, m->m_type);
	if (n == NULL) {
		m_freem(m);
		return(NULL);
	}
	M_COPY_PKTHDR(n, m); /* just for recvif */
	if (replylen > MHLEN) {
		if (replylen > MCLBYTES) {
			/*
			 * XXX: should we try to allocate more? But MCLBYTES
			 * is probably much larger than IPV6_MMTU...
			 */
			goto bad;
		}
		MCLGET(n, M_DONTWAIT);
		if ((n->m_flags & M_EXT) == 0) {
			goto bad;
		}
	}
	n->m_pkthdr.len = n->m_len = replylen;

	/* copy mbuf header and IPv6 + Node Information base headers */
	bcopy(mtod(m, caddr_t), mtod(n, caddr_t), sizeof(struct ip6_hdr));
	nni6 = (struct icmp6_nodeinfo *)(mtod(n, struct ip6_hdr *) + 1);
	bcopy((caddr_t)ni6, (caddr_t)nni6, sizeof(struct icmp6_nodeinfo));

	/* qtype dependent procedure */
	switch (qtype) {
	case NI_QTYPE_NOOP:
		nni6->ni_code = ICMP6_NI_SUCCESS;
		nni6->ni_flags = 0;
		break;
	case NI_QTYPE_SUPTYPES:
	{
		u_int32_t v;
		nni6->ni_code = ICMP6_NI_SUCCESS;
		nni6->ni_flags = htons(0x0000);	/* raw bitmap */
		/* supports NOOP, SUPTYPES, FQDN, and NODEADDR */
		v = (u_int32_t)htonl(0x0000000f);
		bcopy(&v, nni6 + 1, sizeof(u_int32_t));
		break;
	}
	case NI_QTYPE_FQDN:
		nni6->ni_code = ICMP6_NI_SUCCESS;
		fqdn = (struct ni_reply_fqdn *)(mtod(n, caddr_t) +
						sizeof(struct ip6_hdr) +
						sizeof(struct icmp6_nodeinfo));
		nni6->ni_flags = 0; /* XXX: meaningless TTL */
		fqdn->ni_fqdn_ttl = 0;	/* ditto. */
		/*
		 * XXX do we really have FQDN in variable "hostname"?
		 */
		n->m_next = ni6_nametodns(hostname, hostnamelen, oldfqdn);
		if (n->m_next == NULL)
			goto bad;
		/* XXX we assume that n->m_next is not a chain */
		if (n->m_next->m_next != NULL)
			goto bad;
		n->m_pkthdr.len += n->m_next->m_len;
		break;
	case NI_QTYPE_NODEADDR:
	{
		int lenlim, copied;

		nni6->ni_code = ICMP6_NI_SUCCESS;
		n->m_pkthdr.len = n->m_len =
		    sizeof(struct ip6_hdr) + sizeof(struct icmp6_nodeinfo);
		lenlim = M_TRAILINGSPACE(n);
		copied = ni6_store_addrs(ni6, nni6, ifp, lenlim);
		/* XXX: reset mbuf length */
		n->m_pkthdr.len = n->m_len = sizeof(struct ip6_hdr) +
			sizeof(struct icmp6_nodeinfo) + copied;
		break;
	}
	default:
		break;		/* XXX impossible! */
	}

	nni6->ni_type = ICMP6_NI_REPLY;
	m_freem(m);
	return(n);

  bad:
	m_freem(m);
	if (n)
		m_freem(n);
	return(NULL);
}
#undef hostnamelen

/*
 * make a mbuf with DNS-encoded string.  no compression support.
 *
 * XXX names with less than 2 dots (like "foo" or "foo.section") will be
 * treated as truncated name (two \0 at the end).  this is a wild guess.
 */
static struct mbuf *
ni6_nametodns(name, namelen, old)
	const char *name;
	int namelen;
	int old;	/* return pascal string if non-zero */
{
	struct mbuf *m;
	char *cp, *ep;
	const char *p, *q;
	int i, len, nterm;

	if (old)
		len = namelen + 1;
	else
		len = MCLBYTES;

	/* because MAXHOSTNAMELEN is usually 256, we use cluster mbuf */
	MGET(m, M_DONTWAIT, MT_DATA);
	if (m && len > MLEN) {
		MCLGET(m, M_DONTWAIT);
		if ((m->m_flags & M_EXT) == 0)
			goto fail;
	}
	if (!m)
		goto fail;
	m->m_next = NULL;

	if (old) {
		m->m_len = len;
		*mtod(m, char *) = namelen;
		bcopy(name, mtod(m, char *) + 1, namelen);
		return m;
	} else {
		m->m_len = 0;
		cp = mtod(m, char *);
		ep = mtod(m, char *) + M_TRAILINGSPACE(m);

		/* if not certain about my name, return empty buffer */
		if (namelen == 0)
			return m;

		/*
		 * guess if it looks like shortened hostname, or FQDN.
		 * shortened hostname needs two trailing "\0".
		 */
		i = 0;
		for (p = name; p < name + namelen; p++) {
			if (*p && *p == '.')
				i++;
		}
		if (i < 2)
			nterm = 2;
		else
			nterm = 1;

		p = name;
		while (cp < ep && p < name + namelen) {
			i = 0;
			for (q = p; q < name + namelen && *q && *q != '.'; q++)
				i++;
			/* result does not fit into mbuf */
			if (cp + i + 1 >= ep)
				goto fail;
			/*
			 * DNS label length restriction, RFC1035 page 8.
			 * "i == 0" case is included here to avoid returning
			 * 0-length label on "foo..bar".
			 */
			if (i <= 0 || i >= 64)
				goto fail;
			*cp++ = i;
			bcopy(p, cp, i);
			cp += i;
			p = q;
			if (p < name + namelen && *p == '.')
				p++;
		}
		/* termination */
		if (cp + nterm >= ep)
			goto fail;
		while (nterm-- > 0)
			*cp++ = '\0';
		m->m_len = cp - mtod(m, char *);
		return m;
	}

	panic("should not reach here");
	/* NOTREACHED */

 fail:
	if (m)
		m_freem(m);
	return NULL;
}

/*
 * check if two DNS-encoded string matches.  takes care of truncated
 * form (with \0\0 at the end).  no compression support.
 * XXX upper/lowercase match (see RFC2065)
 */
static int
ni6_dnsmatch(a, alen, b, blen)
	const char *a;
	int alen;
	const char *b;
	int blen;
{
	const char *a0, *b0;
	int l;

	/* simplest case - need validation? */
	if (alen == blen && bcmp(a, b, alen) == 0)
		return 1;

	a0 = a;
	b0 = b;

	/* termination is mandatory */
	if (alen < 2 || blen < 2)
		return 0;
	if (a0[alen - 1] != '\0' || b0[blen - 1] != '\0')
		return 0;
	alen--;
	blen--;

	while (a - a0 < alen && b - b0 < blen) {
		if (a - a0 + 1 > alen || b - b0 + 1 > blen)
			return 0;

		if ((signed char)a[0] < 0 || (signed char)b[0] < 0)
			return 0;
		/* we don't support compression yet */
		if (a[0] >= 64 || b[0] >= 64)
			return 0;

		/* truncated case */
		if (a[0] == 0 && a - a0 == alen - 1)
			return 1;
		if (b[0] == 0 && b - b0 == blen - 1)
			return 1;
		if (a[0] == 0 || b[0] == 0)
			return 0;

		if (a[0] != b[0])
			return 0;
		l = a[0];
		if (a - a0 + 1 + l > alen || b - b0 + 1 + l > blen)
			return 0;
		if (bcmp(a + 1, b + 1, l) != 0)
			return 0;

		a += 1 + l;
		b += 1 + l;
	}

	if (a - a0 == alen && b - b0 == blen)
		return 1;
	else
		return 0;
}

/*
 * calculate the number of addresses to be returned in the node info reply.
 */
static int
ni6_addrs(ni6, m, ifpp, subj)
	struct icmp6_nodeinfo *ni6;
	struct mbuf *m;
	struct ifnet **ifpp;
	char *subj;
{
	struct ifnet *ifp;
	struct in6_ifaddr *ifa6;
	struct ifaddr *ifa;
	struct sockaddr_in6 *subj_ip6 = NULL; /* XXX pedant */
	int addrs = 0, addrsofif, iffound = 0;
	int niflags = ni6->ni_flags;

	if ((niflags & NI_NODEADDR_FLAG_ALL) == 0) {
		switch (ni6->ni_code) {
		case ICMP6_NI_SUBJ_IPV6:
			if (subj == NULL) /* must be impossible... */
				return(0);
			subj_ip6 = (struct sockaddr_in6 *)subj;
			break;
		default:
			/*
			 * XXX: we only support IPv6 subject address for
			 * this Qtype.
			 */
			return(0);
		}
	}

#if defined(__bsdi__) || (defined(__FreeBSD__) && __FreeBSD__ < 3)
	for (ifp = ifnet; ifp; ifp = ifp->if_next)
#else
	for (ifp = TAILQ_FIRST(&ifnet); ifp; ifp = TAILQ_NEXT(ifp, if_list))
#endif
	{
		addrsofif = 0;
#if defined(__bsdi__) || (defined(__FreeBSD__) && __FreeBSD__ < 3)
		for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
#elif defined(__FreeBSD__) && __FreeBSD__ >= 4
		TAILQ_FOREACH(ifa, &ifp->if_addrlist, ifa_list)
#else
		for (ifa = ifp->if_addrlist.tqh_first; ifa;
		     ifa = ifa->ifa_list.tqe_next)
#endif
		{
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			ifa6 = (struct in6_ifaddr *)ifa;

			if ((niflags & NI_NODEADDR_FLAG_ALL) == 0 &&
			    IN6_ARE_ADDR_EQUAL(&subj_ip6->sin6_addr,
					       &ifa6->ia_addr.sin6_addr))
				iffound = 1;

			/*
			 * IPv4-mapped addresses can only be returned by a
			 * Node Information proxy, since they represent
			 * addresses of IPv4-only nodes, which perforce do
			 * not implement this protocol.
			 * [icmp-name-lookups-07, Section 5.4]
			 * So we don't support NI_NODEADDR_FLAG_COMPAT in
			 * this function at this moment.
			 */

			/* What do we have to do about ::1? */
			switch (in6_addrscope(&ifa6->ia_addr.sin6_addr)) {
			case IPV6_ADDR_SCOPE_LINKLOCAL:
				if ((niflags & NI_NODEADDR_FLAG_LINKLOCAL) == 0)
					continue;
				break;
			case IPV6_ADDR_SCOPE_SITELOCAL:
				if ((niflags & NI_NODEADDR_FLAG_SITELOCAL) == 0)
					continue;
				break;
			case IPV6_ADDR_SCOPE_GLOBAL:
				if ((niflags & NI_NODEADDR_FLAG_GLOBAL) == 0)
					continue;
				break;
			default:
				continue;
			}

			/*
			 * check if anycast is okay.
			 * XXX: just experimental.  not in the spec.
			 */
			if ((ifa6->ia6_flags & IN6_IFF_ANYCAST) != 0 &&
			    (niflags & NI_NODEADDR_FLAG_ANYCAST) == 0)
				continue; /* we need only unicast addresses */
			if ((ifa6->ia6_flags & IN6_IFF_TEMPORARY) != 0 &&
			    (icmp6_nodeinfo & 4) == 0) {
				continue;
			}
			addrsofif++; /* count the address */
		}
		if (iffound) {
			*ifpp = ifp;
			return(addrsofif);
		}

		addrs += addrsofif;
	}

	return(addrs);
}

static int
ni6_store_addrs(ni6, nni6, ifp0, resid)
	struct icmp6_nodeinfo *ni6, *nni6;
	struct ifnet *ifp0;
	int resid;
{
#if defined(__bsdi__) || (defined(__FreeBSD__) && __FreeBSD__ < 3)
	struct ifnet *ifp = ifp0 ? ifp0 : ifnet;
#else
	struct ifnet *ifp = ifp0 ? ifp0 : TAILQ_FIRST(&ifnet);
#endif
	struct in6_ifaddr *ifa6;
	struct ifaddr *ifa;
	struct ifnet *ifp_dep = NULL;
	int copied = 0, allow_deprecated = 0;
	u_char *cp = (u_char *)(nni6 + 1);
	int niflags = ni6->ni_flags;
	u_int32_t ltime;
#if !(defined(__FreeBSD__) && __FreeBSD__ >= 3)
	long time_second = time.tv_sec;
#endif

	if (ifp0 == NULL && !(niflags & NI_NODEADDR_FLAG_ALL))
		return(0);	/* needless to copy */

  again:

#if defined(__bsdi__) || (defined(__FreeBSD__) && __FreeBSD__ < 3)
	for (; ifp; ifp = ifp->if_next)
#else
	for (; ifp; ifp = TAILQ_NEXT(ifp, if_list))
#endif
	{
#if defined(__bsdi__) || (defined(__FreeBSD__) && __FreeBSD__ < 3)
		for (ifa = ifp->if_addrlist; ifa; ifa = ifa->ifa_next)
#else
		for (ifa = ifp->if_addrlist.tqh_first; ifa;
		     ifa = ifa->ifa_list.tqe_next)
#endif
		{
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			ifa6 = (struct in6_ifaddr *)ifa;

			if (IFA6_IS_DEPRECATED(ifa6) &&
			    allow_deprecated == 0) {
				/*
				 * prefererred address should be put before
				 * deprecated addresses.
				 */

				/* record the interface for later search */
				if (ifp_dep == NULL)
					ifp_dep = ifp;

				continue;
			}
			else if (!IFA6_IS_DEPRECATED(ifa6) &&
				 allow_deprecated != 0)
				continue; /* we now collect deprecated addrs */

			/* What do we have to do about ::1? */
			switch (in6_addrscope(&ifa6->ia_addr.sin6_addr)) {
			case IPV6_ADDR_SCOPE_LINKLOCAL:
				if ((niflags & NI_NODEADDR_FLAG_LINKLOCAL) == 0)
					continue;
				break;
			case IPV6_ADDR_SCOPE_SITELOCAL:
				if ((niflags & NI_NODEADDR_FLAG_SITELOCAL) == 0)
					continue;
				break;
			case IPV6_ADDR_SCOPE_GLOBAL:
				if ((niflags & NI_NODEADDR_FLAG_GLOBAL) == 0)
					continue;
				break;
			default:
				continue;
			}

			/*
			 * check if anycast is okay.
			 * XXX: just experimental.  not in the spec.
			 */
			if ((ifa6->ia6_flags & IN6_IFF_ANYCAST) != 0 &&
			    (niflags & NI_NODEADDR_FLAG_ANYCAST) == 0)
				continue;
			if ((ifa6->ia6_flags & IN6_IFF_TEMPORARY) != 0 &&
			    (icmp6_nodeinfo & 4) == 0) {
				continue;
			}

			/* now we can copy the address */
			if (resid < sizeof(struct in6_addr) +
			    sizeof(u_int32_t)) {
				/*
				 * We give up much more copy.
				 * Set the truncate flag and return.
				 */
				nni6->ni_flags |=
					NI_NODEADDR_FLAG_TRUNCATE;
				return(copied);
			}

			/*
			 * Set the TTL of the address.
			 * The TTL value should be one of the following
			 * according to the specification:
			 *
			 * 1. The remaining lifetime of a DHCP lease on the
			 *    address, or
			 * 2. The remaining Valid Lifetime of a prefix from
			 *    which the address was derived through Stateless
			 *    Autoconfiguration.
			 *
			 * Note that we currently do not support stateful
			 * address configuration by DHCPv6, so the former
			 * case can't happen.
			 *
			 * TTL must be 2^31 > TTL >= 0.
			 */
			if (ifa6->ia6_lifetime.ia6t_expire == 0)
				ltime = ND6_INFINITE_LIFETIME;
			else {
				if (ifa6->ia6_lifetime.ia6t_expire >
				    time_second)
					ltime = ifa6->ia6_lifetime.ia6t_expire - time_second;
				else
					ltime = 0;
			}
			if (ltime > 0x7fffffff)
				ltime = 0x7fffffff;
			ltime = htonl(ltime);
			
			bcopy(&ltime, cp, sizeof(u_int32_t));
			cp += sizeof(u_int32_t);

			/* copy the address itself */
			bcopy(&ifa6->ia_addr.sin6_addr, cp,
			      sizeof(struct in6_addr));
			/* XXX: KAME link-local hack; remove ifindex */
			if (IN6_IS_ADDR_LINKLOCAL(&ifa6->ia_addr.sin6_addr))
				((struct in6_addr *)cp)->s6_addr16[1] = 0;
			cp += sizeof(struct in6_addr);
			
			resid -= (sizeof(struct in6_addr) + sizeof(u_int32_t));
			copied += (sizeof(struct in6_addr) +
				   sizeof(u_int32_t));
		}
		if (ifp0)	/* we need search only on the specified IF */
			break;
	}

	if (allow_deprecated == 0 && ifp_dep != NULL) {
		ifp = ifp_dep;
		allow_deprecated = 1;

		goto again;
	}

	return(copied);
}

/*
 * XXX almost dup'ed code with rip6_input.
 */
static int
icmp6_rip6_input(mp, off)
	struct	mbuf **mp;
	int	off;
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct in6pcb *in6p;
	struct in6pcb *last = NULL;
	struct sockaddr_in6 rip6src;
	struct icmp6_hdr *icmp6;
	struct ip6_recvpktopts opts;

#ifndef PULLDOWN_TEST
	/* this is assumed to be safe. */
	icmp6 = (struct icmp6_hdr *)((caddr_t)ip6 + off);
#else
	IP6_EXTHDR_GET(icmp6, struct icmp6_hdr *, m, off, sizeof(*icmp6));
	if (icmp6 == NULL) {
		/* m is already reclaimed */
		return IPPROTO_DONE;
	}
#endif

	bzero(&opts, sizeof(opts));
	bzero(&rip6src, sizeof(rip6src));
	rip6src.sin6_len = sizeof(struct sockaddr_in6);
	rip6src.sin6_family = AF_INET6;
	/* KAME hack: recover scopeid */
	(void)in6_recoverscope(&rip6src, &ip6->ip6_src, m->m_pkthdr.rcvif);

#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	LIST_FOREACH(in6p, &ripcb, inp_list)
#elif defined(__OpenBSD__)
	for (in6p = rawin6pcbtable.inpt_queue.cqh_first;
	     in6p != (struct inpcb *)&rawin6pcbtable.inpt_queue;
	     in6p = in6p->inp_queue.cqe_next)
#else
	for (in6p = rawin6pcb.in6p_next;
	     in6p != &rawin6pcb; in6p = in6p->in6p_next)
#endif
	{
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
		if ((in6p->inp_vflag & INP_IPV6) == 0)
			continue;
#endif
#ifdef HAVE_NRL_INPCB
		if (!(in6p->in6p_flags & INP_IPV6))
			continue;
#endif
		if (in6p->in6p_ip6_nxt != IPPROTO_ICMPV6)
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_laddr) &&
		   !IN6_ARE_ADDR_EQUAL(&in6p->in6p_laddr, &ip6->ip6_dst))
			continue;
		if (!IN6_IS_ADDR_UNSPECIFIED(&in6p->in6p_faddr) &&
		   !IN6_ARE_ADDR_EQUAL(&in6p->in6p_faddr, &ip6->ip6_src))
			continue;
		if (in6p->in6p_icmp6filt
		    && ICMP6_FILTER_WILLBLOCK(icmp6->icmp6_type,
				 in6p->in6p_icmp6filt))
			continue;
		if (last) {
			struct	mbuf *n;
			if ((n = m_copy(m, 0, (int)M_COPYALL)) != NULL) {
				if (last->in6p_flags & IN6P_CONTROLOPTS)
					ip6_savecontrol(last, ip6, n, &opts,
							NULL);
				/* strip intermediate headers */
				m_adj(n, off);
				if (sbappendaddr(&last->in6p_socket->so_rcv,
						 (struct sockaddr *)&rip6src,
						 n, opts.head) == 0) {
					/* should notify about lost packet */
					m_freem(n);
					if (opts.head) {
						m_freem(opts.head);
					}
				} else
					sorwakeup(last->in6p_socket);
				bzero(&opts, sizeof(opts));
			}
		}
		last = in6p;
	}
	if (last) {
		if (last->in6p_flags & IN6P_CONTROLOPTS)
			ip6_savecontrol(last, ip6, m, &opts, NULL);
		/* strip intermediate headers */
		m_adj(m, off);
		if (sbappendaddr(&last->in6p_socket->so_rcv,
				 (struct sockaddr *)&rip6src,
				 m, opts.head) == 0) {
			m_freem(m);
			if (opts.head)
				m_freem(opts.head);
		} else
			sorwakeup(last->in6p_socket);
	} else {
		m_freem(m);
		ip6stat.ip6s_delivered--;
	}
	return IPPROTO_DONE;
}

/*
 * Reflect the ip6 packet back to the source.
 * OFF points to the icmp6 header, counted from the top of the mbuf.
 *
 * Note: RFC 1885 required that an echo reply should be truncated if it
 * did not fit in with (return) path MTU, and KAME code supported the
 * behavior.  However, as a clarification after the RFC, this limitation
 * was removed in a revised version of the spec, RFC 2463.  We had kept the
 * old behavior, with a (non-default) ifdef block, while the new version of
 * the spec was an internet-draft status, and even after the new RFC was
 * published.  But it would rather make sense to clean the obsoleted part
 * up, and to make the code simpler at this stage.
 */
void
icmp6_reflect(m, off)
	struct	mbuf *m;
	size_t off;
{
	struct ip6_hdr *ip6;
	struct icmp6_hdr *icmp6;
	struct in6_ifaddr *ia;
	struct in6_addr t, *src = 0;
	int plen;
	int type, code;
	struct ifnet *outif = NULL;
	struct sockaddr_in6 sa6_src, sa6_dst;

	/* too short to reflect */
	if (off < sizeof(struct ip6_hdr)) {
		nd6log((LOG_DEBUG,
		    "sanity fail: off=%lx, sizeof(ip6)=%lx in %s:%d\n",
		    (u_long)off, (u_long)sizeof(struct ip6_hdr),
		    __FILE__, __LINE__));
		goto bad;
	}

	/*
	 * If there are extra headers between IPv6 and ICMPv6, strip
	 * off that header first.
	 */
#ifdef DIAGNOSTIC
	if (sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr) > MHLEN)
		panic("assumption failed in icmp6_reflect");
#endif
	if (off > sizeof(struct ip6_hdr)) {
		size_t l;
		struct ip6_hdr nip6;

		l = off - sizeof(struct ip6_hdr);
		m_copydata(m, 0, sizeof(nip6), (caddr_t)&nip6);
		m_adj(m, l);
		l = sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr);
		if (m->m_len < l) {
			if ((m = m_pullup(m, l)) == NULL)
				return;
		}
		bcopy((caddr_t)&nip6, mtod(m, caddr_t), sizeof(nip6));
	} else /* off == sizeof(struct ip6_hdr) */ {
		size_t l;
		l = sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr);
		if (m->m_len < l) {
			if ((m = m_pullup(m, l)) == NULL)
				return;
		}
	}
	plen = m->m_pkthdr.len - sizeof(struct ip6_hdr);
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	icmp6 = (struct icmp6_hdr *)(ip6 + 1);
	type = icmp6->icmp6_type; /* keep type for statistics */
	code = icmp6->icmp6_code; /* ditto. */

	t = ip6->ip6_dst;
	/*
	 * ip6_input() drops a packet if its src is multicast.
	 * So, the src is never multicast.
	 */
	ip6->ip6_dst = ip6->ip6_src;

	/*
	 * XXX: make sure to embed scope zone information, using
	 * already embedded IDs or the received interface (if any).
	 * Note that rcvif may be NULL.
	 * TODO: scoped routing case (XXX).
	 */
	bzero(&sa6_src, sizeof(sa6_src));
	sa6_src.sin6_family = AF_INET6;
	sa6_src.sin6_len = sizeof(sa6_src);
	sa6_src.sin6_addr = ip6->ip6_dst;
	in6_recoverscope(&sa6_src, &ip6->ip6_dst, m->m_pkthdr.rcvif);
	in6_embedscope(&sa6_src.sin6_addr, &sa6_src, NULL, NULL);
	ip6->ip6_dst = sa6_src.sin6_addr;

	bzero(&sa6_dst, sizeof(sa6_dst));
	sa6_dst.sin6_family = AF_INET6;
	sa6_dst.sin6_len = sizeof(sa6_dst);
	sa6_dst.sin6_addr = t;
	in6_recoverscope(&sa6_dst, &t, m->m_pkthdr.rcvif);
	in6_embedscope(&t, &sa6_dst, NULL, NULL);

	/*
	 * If the incoming packet was addressed directly to us (i.e. unicast),
	 * use dst as the src for the reply.
	 * The IN6_IFF_NOTREADY case would be VERY rare, but is possible
	 * (for example) when we encounter an error while forwarding procedure
	 * destined to a duplicated address of ours.
	 */
	for (ia = in6_ifaddr; ia; ia = ia->ia_next)
		if (IN6_ARE_ADDR_EQUAL(&t, &ia->ia_addr.sin6_addr) &&
		    (ia->ia6_flags & (IN6_IFF_ANYCAST|IN6_IFF_NOTREADY)) == 0) {
			src = &t;
			break;
		}
	if (ia == NULL && IN6_IS_ADDR_LINKLOCAL(&t) && (m->m_flags & M_LOOP)) {
		/*
		 * This is the case if the dst is our link-local address
		 * and the sender is also ourselves.
		 */
		src = &t;
	}

	if (src == 0) {
		int e;
#ifdef NEW_STRUCT_ROUTE
		struct route ro;
#else
		struct route_in6 ro;
#endif

		/*
		 * This case matches to multicasts, our anycast, or unicasts
		 * that we do not own.  Select a source address based on the
		 * source address of the erroneous packet.
		 */
		bzero(&ro, sizeof(ro));
		src = in6_selectsrc(&sa6_src, NULL, NULL, &ro, NULL, &e);
		if (ro.ro_rt) { /* XXX: see comments in icmp6_mtudisc_update */
			RTFREE(ro.ro_rt); /* XXX: we could use this */
		}
		if (src == NULL) {
			nd6log((LOG_DEBUG,
			    "icmp6_reflect: source can't be determined: "
			    "dst=%s, error=%d\n",
			    ip6_sprintf(&sa6_src.sin6_addr), e));
			goto bad;
		}
	}

	ip6->ip6_src = *src;

	ip6->ip6_flow = 0;
	ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc |= IPV6_VERSION;
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	if (m->m_pkthdr.rcvif) {
		/* XXX: This may not be the outgoing interface */
		ip6->ip6_hlim = nd_ifinfo[m->m_pkthdr.rcvif->if_index].chlim;
	} else
		ip6->ip6_hlim = ip6_defhlim;

	icmp6->icmp6_cksum = 0;
	icmp6->icmp6_cksum = in6_cksum(m, IPPROTO_ICMPV6,
					sizeof(struct ip6_hdr), plen);

	/*
	 * XXX option handling
	 */

	m->m_flags &= ~(M_BCAST|M_MCAST);
#ifdef IPSEC
	/* Don't lookup socket */
	(void)ipsec_setsocket(m, NULL);
#endif /* IPSEC */

	/*
	 * To avoid a "too big" situation at an intermediate router
	 * and the path MTU discovery process, specify the IPV6_MINMTU flag.
	 * Note that only echo and node information replies are affected,
	 * since the length of ICMP6 errors is limited to the minimum MTU.
	 */
	ip6_output(m, NULL, NULL, IPV6_MINMTU, NULL, &outif);

	if (outif)
		icmp6_ifoutstat_inc(outif, type, code);

	return;

 bad:
	m_freem(m);
	return;
}

void
icmp6_fasttimo()
{

	mld6_fasttimeo();
}

static const char *
icmp6_redirect_diag(src6, dst6, tgt6)
	struct in6_addr *src6;
	struct in6_addr *dst6;
	struct in6_addr *tgt6;
{
	static char buf[1024];
#ifndef __bsdi__
	snprintf(buf, sizeof(buf), "(src=%s dst=%s tgt=%s)",
		ip6_sprintf(src6), ip6_sprintf(dst6), ip6_sprintf(tgt6));
#else
	sprintf(buf, "(src=%s dst=%s tgt=%s)",
		ip6_sprintf(src6), ip6_sprintf(dst6), ip6_sprintf(tgt6));
#endif
	return buf;
}

void
icmp6_redirect_input(m, off)
	struct mbuf *m;
	int off;
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct nd_redirect *nd_rd;
	int icmp6len = ntohs(ip6->ip6_plen);
	char *lladdr = NULL;
	int lladdrlen = 0;
	u_char *redirhdr = NULL;
	int redirhdrlen = 0;
	struct rtentry *rt = NULL;
	int is_router;
	int is_onlink;
	struct in6_addr src6 = ip6->ip6_src;
	struct in6_addr redtgt6;
	struct in6_addr reddst6;
	union nd_opts ndopts;

	if (!m || !ifp)
		return;

	/* XXX if we are router, we don't update route by icmp6 redirect */
	if (ip6_forwarding)
		goto freeit;
	if (!icmp6_rediraccept)
		goto freeit;

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, icmp6len,);
	nd_rd = (struct nd_redirect *)((caddr_t)ip6 + off);
#else
	IP6_EXTHDR_GET(nd_rd, struct nd_redirect *, m, off, icmp6len);
	if (nd_rd == NULL) {
		icmp6stat.icp6s_tooshort++;
		return;
	}
#endif
	redtgt6 = nd_rd->nd_rd_target;
	reddst6 = nd_rd->nd_rd_dst;

	if (IN6_IS_ADDR_LINKLOCAL(&redtgt6))
		redtgt6.s6_addr16[1] = htons(ifp->if_index);
	if (IN6_IS_ADDR_LINKLOCAL(&reddst6))
		reddst6.s6_addr16[1] = htons(ifp->if_index);

	/* validation */
	if (!IN6_IS_ADDR_LINKLOCAL(&src6)) {
		nd6log((LOG_ERR,
			"ICMP6 redirect sent from %s rejected; "
			"must be from linklocal\n", ip6_sprintf(&src6)));
		goto bad;
	}
	if (ip6->ip6_hlim != 255) {
		nd6log((LOG_ERR,
			"ICMP6 redirect sent from %s rejected; "
			"hlim=%d (must be 255)\n",
			ip6_sprintf(&src6), ip6->ip6_hlim));
		goto bad;
	}
    {
	/* ip6->ip6_src must be equal to gw for icmp6->icmp6_reddst */
	struct sockaddr_in6 sin6;
	struct in6_addr *gw6;

	bzero(&sin6, sizeof(sin6));
	sin6.sin6_family = AF_INET6;
	sin6.sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&reddst6, &sin6.sin6_addr, sizeof(reddst6));
	rt = rtalloc1((struct sockaddr *)&sin6, 0
#ifdef __FreeBSD__
		      , 0UL
#endif
		      );
	if (rt) {
		if (rt->rt_gateway == NULL ||
		    rt->rt_gateway->sa_family != AF_INET6) {
			nd6log((LOG_ERR,
			    "ICMP6 redirect rejected; no route "
			    "with inet6 gateway found for redirect dst: %s\n",
			    icmp6_redirect_diag(&src6, &reddst6, &redtgt6)));
			RTFREE(rt);
			goto bad;
		}

		gw6 = &(((struct sockaddr_in6 *)rt->rt_gateway)->sin6_addr);
		if (bcmp(&src6, gw6, sizeof(struct in6_addr)) != 0) {
			nd6log((LOG_ERR,
				"ICMP6 redirect rejected; "
				"not equal to gw-for-src=%s (must be same): "
				"%s\n",
				ip6_sprintf(gw6),
				icmp6_redirect_diag(&src6, &reddst6, &redtgt6)));
			RTFREE(rt);
			goto bad;
		}
	} else {
		nd6log((LOG_ERR,
			"ICMP6 redirect rejected; "
			"no route found for redirect dst: %s\n",
			icmp6_redirect_diag(&src6, &reddst6, &redtgt6)));
		goto bad;
	}
	RTFREE(rt);
	rt = NULL;
    }
	if (IN6_IS_ADDR_MULTICAST(&reddst6)) {
		nd6log((LOG_ERR,
			"ICMP6 redirect rejected; "
			"redirect dst must be unicast: %s\n",
			icmp6_redirect_diag(&src6, &reddst6, &redtgt6)));
		goto bad;
	}

	is_router = is_onlink = 0;
	if (IN6_IS_ADDR_LINKLOCAL(&redtgt6))
		is_router = 1;	/* router case */
	if (bcmp(&redtgt6, &reddst6, sizeof(redtgt6)) == 0)
		is_onlink = 1;	/* on-link destination case */
	if (!is_router && !is_onlink) {
		nd6log((LOG_ERR,
			"ICMP6 redirect rejected; "
			"neither router case nor onlink case: %s\n",
			icmp6_redirect_diag(&src6, &reddst6, &redtgt6)));
		goto bad;
	}
	/* validation passed */

	icmp6len -= sizeof(*nd_rd);
	nd6_option_init(nd_rd + 1, icmp6len, &ndopts);
	if (nd6_options(&ndopts) < 0) {
		nd6log((LOG_INFO, "icmp6_redirect_input: "
			"invalid ND option, rejected: %s\n",
			icmp6_redirect_diag(&src6, &reddst6, &redtgt6)));
		/* nd6_options have incremented stats */
		goto freeit;
	}

	if (ndopts.nd_opts_tgt_lladdr) {
		lladdr = (char *)(ndopts.nd_opts_tgt_lladdr + 1);
		lladdrlen = ndopts.nd_opts_tgt_lladdr->nd_opt_len << 3;
	}

	if (ndopts.nd_opts_rh) {
		redirhdrlen = ndopts.nd_opts_rh->nd_opt_rh_len;
		redirhdr = (u_char *)(ndopts.nd_opts_rh + 1); /* xxx */
	}

	if (lladdr && ((ifp->if_addrlen + 2 + 7) & ~7) != lladdrlen) {
		nd6log((LOG_INFO,
			"icmp6_redirect_input: lladdrlen mismatch for %s "
			"(if %d, icmp6 packet %d): %s\n",
			ip6_sprintf(&redtgt6), ifp->if_addrlen, lladdrlen - 2,
			icmp6_redirect_diag(&src6, &reddst6, &redtgt6)));
		goto bad;
	}

	/* RFC 2461 8.3 */
	nd6_cache_lladdr(ifp, &redtgt6, lladdr, lladdrlen, ND_REDIRECT,
			 is_onlink ? ND_REDIRECT_ONLINK : ND_REDIRECT_ROUTER);

	if (!is_onlink) {	/* better router case.  perform rtredirect. */
		/* perform rtredirect */
		struct sockaddr_in6 sdst;
		struct sockaddr_in6 sgw;
		struct sockaddr_in6 ssrc;
#ifdef __bsdi__
		extern int icmp_redirtimeout;	/* XXX */
#endif
#if defined(__NetBSD__) || defined(__OpenBSD__)
		unsigned long rtcount;
		struct rtentry *newrt = NULL;
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
		/*
		 * do not install redirect route, if the number of entries
		 * is too much (> hiwat).  note that, the node (= host) will
		 * work just fine even if we do not install redirect route
		 * (there will be additional hops, though).
		 */
		rtcount = rt_timer_count(icmp6_redirect_timeout_q);
		if (0 <= icmp6_redirect_hiwat && rtcount > icmp6_redirect_hiwat)
			return;
		else if (0 <= icmp6_redirect_lowat &&
		    rtcount > icmp6_redirect_lowat) {
			/*
			 * XXX nuke a victim, install the new one.
			 */
		}
#endif

		bzero(&sdst, sizeof(sdst));
		bzero(&sgw, sizeof(sgw));
		bzero(&ssrc, sizeof(ssrc));
		sdst.sin6_family = sgw.sin6_family = ssrc.sin6_family = AF_INET6;
		sdst.sin6_len = sgw.sin6_len = ssrc.sin6_len =
			sizeof(struct sockaddr_in6);
		bcopy(&redtgt6, &sgw.sin6_addr, sizeof(struct in6_addr));
		bcopy(&reddst6, &sdst.sin6_addr, sizeof(struct in6_addr));
		bcopy(&src6, &ssrc.sin6_addr, sizeof(struct in6_addr));
		rtredirect((struct sockaddr *)&sdst, (struct sockaddr *)&sgw,
			   (struct sockaddr *)NULL, RTF_GATEWAY | RTF_HOST,
			   (struct sockaddr *)&ssrc,
#ifdef __bsdi__
			   icmp_redirtimeout
#elif defined(__NetBSD__) || defined(__OpenBSD__)
			   &newrt
#else
			   (struct rtentry **)NULL
#endif
			   );

#if defined(__NetBSD__) || defined(__OpenBSD__)
		if (newrt) {
			(void)rt_timer_add(newrt, icmp6_redirect_timeout,
			    icmp6_redirect_timeout_q);
			rtfree(newrt);
		}
#endif
	}
	/* finally update cached route in each socket via pfctlinput */
    {
	struct sockaddr_in6 sdst;

	bzero(&sdst, sizeof(sdst));
	sdst.sin6_family = AF_INET6;
	sdst.sin6_len = sizeof(struct sockaddr_in6);
	bcopy(&reddst6, &sdst.sin6_addr, sizeof(struct in6_addr));
	pfctlinput(PRC_REDIRECT_HOST, (struct sockaddr *)&sdst);
#ifdef IPSEC
	key_sa_routechange((struct sockaddr *)&sdst);
#endif
    }

 freeit:
	m_freem(m);
	return;

 bad:
	icmp6stat.icp6s_badredirect++;
	m_freem(m);
}

void
icmp6_redirect_output(m0, rt)
	struct mbuf *m0;
	struct rtentry *rt;
{
	struct ifnet *ifp;	/* my outgoing interface */
	struct in6_addr *ifp_ll6;
	struct in6_addr *router_ll6;
	struct ip6_hdr *sip6;	/* m0 as struct ip6_hdr */
	struct mbuf *m = NULL;	/* newly allocated one */
	struct ip6_hdr *ip6;	/* m as struct ip6_hdr */
	struct nd_redirect *nd_rd;
	size_t maxlen;
	u_char *p;
	struct sockaddr_in6 src_sa;
	int64_t zoneid;

	icmp6_errcount(&icmp6stat.icp6s_outerrhist, ND_REDIRECT, 0);

	/* if we are not router, we don't send icmp6 redirect */
	if (!ip6_forwarding || ip6_accept_rtadv)
		goto fail;

	/* sanity check */
	if (!m0 || !rt || !(rt->rt_flags & RTF_UP) || !(ifp = rt->rt_ifp))
		goto fail;

	/*
	 * Address check:
	 *  the source address must identify a neighbor, and
	 *  the destination address must not be a multicast address
	 *  [RFC 2461, sec 8.2]
	 */
	sip6 = mtod(m0, struct ip6_hdr *);
	bzero(&src_sa, sizeof(src_sa));
	src_sa.sin6_family = AF_INET6;
	src_sa.sin6_len = sizeof(src_sa);
	src_sa.sin6_addr = sip6->ip6_src;
	/* we don't currently use sin6_scope_id, but eventually use it */
	if ((zoneid = in6_addr2zoneid(ifp, &sip6->ip6_src)) < 0)
		goto fail;
	src_sa.sin6_scope_id = zoneid;
	if (nd6_is_addr_neighbor(&src_sa, ifp) == 0)
		goto fail;
	if (IN6_IS_ADDR_MULTICAST(&sip6->ip6_dst))
		goto fail;	/* what should we do here? */

	/* rate limit */
	if (icmp6_ratelimit(&sip6->ip6_src, ND_REDIRECT, 0))
		goto fail;

	/*
	 * Since we are going to append up to 1280 bytes (= IPV6_MMTU),
	 * we almost always ask for an mbuf cluster for simplicity.
	 * (MHLEN < IPV6_MMTU is almost always true)
	 */
#if IPV6_MMTU >= MCLBYTES
# error assumption failed about IPV6_MMTU and MCLBYTES
#endif
	MGETHDR(m, M_DONTWAIT, MT_HEADER);
	if (m && IPV6_MMTU >= MHLEN)
		MCLGET(m, M_DONTWAIT);
	if (!m)
		goto fail;
	m->m_pkthdr.rcvif = NULL;
	m->m_len = 0;
	maxlen = M_TRAILINGSPACE(m);
	maxlen = min(IPV6_MMTU, maxlen);
	/* just for safety */
	if (maxlen < sizeof(struct ip6_hdr) + sizeof(struct icmp6_hdr) +
	    ((sizeof(struct nd_opt_hdr) + ifp->if_addrlen + 7) & ~7)) {
		goto fail;
	}

	{
		/* get ip6 linklocal address for ifp(my outgoing interface). */
		struct in6_ifaddr *ia;
		if ((ia = in6ifa_ifpforlinklocal(ifp,
						 IN6_IFF_NOTREADY|
						 IN6_IFF_ANYCAST)) == NULL)
			goto fail;
		ifp_ll6 = &ia->ia_addr.sin6_addr;
	}

	/* get ip6 linklocal address for the router. */
	if (rt->rt_gateway && (rt->rt_flags & RTF_GATEWAY)) {
		struct sockaddr_in6 *sin6;
		sin6 = (struct sockaddr_in6 *)rt->rt_gateway;
		router_ll6 = &sin6->sin6_addr;
		if (!IN6_IS_ADDR_LINKLOCAL(router_ll6))
			router_ll6 = (struct in6_addr *)NULL;
	} else
		router_ll6 = (struct in6_addr *)NULL;

	/* ip6 */
	ip6 = mtod(m, struct ip6_hdr *);
	ip6->ip6_flow = 0;
	ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc |= IPV6_VERSION;
	/* ip6->ip6_plen will be set later */
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	ip6->ip6_hlim = 255;
	/* ip6->ip6_src must be linklocal addr for my outgoing if. */
	bcopy(ifp_ll6, &ip6->ip6_src, sizeof(struct in6_addr));
	bcopy(&sip6->ip6_src, &ip6->ip6_dst, sizeof(struct in6_addr));

	/* ND Redirect */
	nd_rd = (struct nd_redirect *)(ip6 + 1);
	nd_rd->nd_rd_type = ND_REDIRECT;
	nd_rd->nd_rd_code = 0;
	nd_rd->nd_rd_reserved = 0;
	if (rt->rt_flags & RTF_GATEWAY) {
		/*
		 * nd_rd->nd_rd_target must be a link-local address in
		 * better router cases.
		 */
		if (!router_ll6)
			goto fail;
		bcopy(router_ll6, &nd_rd->nd_rd_target,
		      sizeof(nd_rd->nd_rd_target));
		bcopy(&sip6->ip6_dst, &nd_rd->nd_rd_dst,
		      sizeof(nd_rd->nd_rd_dst));
	} else {
		/* make sure redtgt == reddst */
		bcopy(&sip6->ip6_dst, &nd_rd->nd_rd_target,
		      sizeof(nd_rd->nd_rd_target));
		bcopy(&sip6->ip6_dst, &nd_rd->nd_rd_dst,
		      sizeof(nd_rd->nd_rd_dst));
	}

	p = (u_char *)(nd_rd + 1);

	if (!router_ll6)
		goto nolladdropt;

    {
	/* target lladdr option */
	struct rtentry *rt_router = NULL;
	int len;
	struct sockaddr_dl *sdl;
	struct nd_opt_hdr *nd_opt;
	char *lladdr;

	rt_router = nd6_lookup(router_ll6, 0, ifp);
	if (!rt_router)
		goto nolladdropt;
	len = sizeof(*nd_opt) + ifp->if_addrlen;
	len = (len + 7) & ~7;	/* round by 8 */
	/* safety check */
	if (len + (p - (u_char *)ip6) > maxlen)
		goto nolladdropt;
	if (!(rt_router->rt_flags & RTF_GATEWAY) &&
	    (rt_router->rt_flags & RTF_LLINFO) &&
	    (rt_router->rt_gateway->sa_family == AF_LINK) &&
	    (sdl = (struct sockaddr_dl *)rt_router->rt_gateway) &&
	    sdl->sdl_alen) {
		nd_opt = (struct nd_opt_hdr *)p;
		nd_opt->nd_opt_type = ND_OPT_TARGET_LINKADDR;
		nd_opt->nd_opt_len = len >> 3;
		lladdr = (char *)(nd_opt + 1);
		bcopy(LLADDR(sdl), lladdr, ifp->if_addrlen);
		p += len;
	}
    }
nolladdropt:;

	m->m_pkthdr.len = m->m_len = p - (u_char *)ip6;

	/* just to be safe */
#ifdef M_DECRYPTED	/* not openbsd */
	if (m0->m_flags & M_DECRYPTED)
		goto noredhdropt;
#endif
	if (p - (u_char *)ip6 > maxlen)
		goto noredhdropt;

    {
	/* redirected header option */
	int len;
	struct nd_opt_rd_hdr *nd_opt_rh;

	/*
	 * compute the maximum size for icmp6 redirect header option.
	 * XXX room for auth header?
	 */
	len = maxlen - (p - (u_char *)ip6);
	len &= ~7;

	/* This is just for simplicity. */
	if (m0->m_pkthdr.len != m0->m_len) {
		if (m0->m_next) {
			m_freem(m0->m_next);
			m0->m_next = NULL;
		}
		m0->m_pkthdr.len = m0->m_len;
	}

	/*
	 * Redirected header option spec (RFC2461 4.6.3) talks nothing
	 * about padding/truncate rule for the original IP packet.
	 * From the discussion on IPv6imp in Feb 1999, the consensus was:
	 * - "attach as much as possible" is the goal
	 * - pad if not aligned (original size can be guessed by original
	 *   ip6 header)
	 * Following code adds the padding if it is simple enough,
	 * and truncates if not.
	 */
	if (m0->m_next || m0->m_pkthdr.len != m0->m_len)
		panic("assumption failed in %s:%d\n", __FILE__, __LINE__);

	if (len - sizeof(*nd_opt_rh) < m0->m_pkthdr.len) {
		/* not enough room, truncate */
		m0->m_pkthdr.len = m0->m_len = len - sizeof(*nd_opt_rh);
	} else {
		/* enough room, pad or truncate */
		size_t extra;

		extra = m0->m_pkthdr.len % 8;
		if (extra) {
			/* pad if easy enough, truncate if not */
			if (8 - extra <= M_TRAILINGSPACE(m0)) {
				/* pad */
				m0->m_len += (8 - extra);
				m0->m_pkthdr.len += (8 - extra);
			} else {
				/* truncate */
				m0->m_pkthdr.len -= extra;
				m0->m_len -= extra;
			}
		}
		len = m0->m_pkthdr.len + sizeof(*nd_opt_rh);
		m0->m_pkthdr.len = m0->m_len = len - sizeof(*nd_opt_rh);
	}

	nd_opt_rh = (struct nd_opt_rd_hdr *)p;
	bzero(nd_opt_rh, sizeof(*nd_opt_rh));
	nd_opt_rh->nd_opt_rh_type = ND_OPT_REDIRECTED_HEADER;
	nd_opt_rh->nd_opt_rh_len = len >> 3;
	p += sizeof(*nd_opt_rh);
	m->m_pkthdr.len = m->m_len = p - (u_char *)ip6;

	/* connect m0 to m */
	m->m_next = m0;
	m->m_pkthdr.len = m->m_len + m0->m_len;
    }
noredhdropt:;

	if (IN6_IS_ADDR_LINKLOCAL(&sip6->ip6_src))
		sip6->ip6_src.s6_addr16[1] = 0;
	if (IN6_IS_ADDR_LINKLOCAL(&sip6->ip6_dst))
		sip6->ip6_dst.s6_addr16[1] = 0;
#if 0
	if (IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_src))
		ip6->ip6_src.s6_addr16[1] = 0;
	if (IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_dst))
		ip6->ip6_dst.s6_addr16[1] = 0;
#endif
	if (IN6_IS_ADDR_LINKLOCAL(&nd_rd->nd_rd_target))
		nd_rd->nd_rd_target.s6_addr16[1] = 0;
	if (IN6_IS_ADDR_LINKLOCAL(&nd_rd->nd_rd_dst))
		nd_rd->nd_rd_dst.s6_addr16[1] = 0;

	ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(struct ip6_hdr));

	nd_rd->nd_rd_cksum = 0;
	nd_rd->nd_rd_cksum
		= in6_cksum(m, IPPROTO_ICMPV6, sizeof(*ip6), ntohs(ip6->ip6_plen));

	/* send the packet to outside... */
#ifdef IPSEC
	/* Don't lookup socket */
	(void)ipsec_setsocket(m, NULL);
#endif /* IPSEC */
	ip6_output(m, NULL, NULL, 0, NULL, NULL);

	icmp6_ifstat_inc(ifp, ifs6_out_msg);
	icmp6_ifstat_inc(ifp, ifs6_out_redirect);
	icmp6stat.icp6s_outhist[ND_REDIRECT]++;

	return;

fail:
	if (m)
		m_freem(m);
	if (m0)
		m_freem(m0);
}

#ifdef HAVE_NRL_INPCB
#define sotoin6pcb	sotoinpcb
#define in6pcb		inpcb
#define in6p_icmp6filt	inp_icmp6filt
#endif
/*
 * ICMPv6 socket option processing.
 */
int
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
icmp6_ctloutput(so, sopt)
	struct socket *so;
	struct sockopt *sopt;
#else
icmp6_ctloutput(op, so, level, optname, mp)
	int op;
	struct socket *so;
	int level, optname;
	struct mbuf **mp;
#endif
{
	int error = 0;
	int optlen;
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
	struct inpcb *inp = sotoinpcb(so);
	int level, op, optname;

	if (sopt) {
		level = sopt->sopt_level;
		op = sopt->sopt_dir;
		optname = sopt->sopt_name;
		optlen = sopt->sopt_valsize;
	} else
		level = op = optname = optlen = 0;
#else
	struct in6pcb *in6p = sotoin6pcb(so);
	struct mbuf *m = *mp;

	optlen = m ? m->m_len : 0;
#endif

	if (level != IPPROTO_ICMPV6) {
#if !(defined(__FreeBSD__) && __FreeBSD__ >= 3)
		if (op == PRCO_SETOPT && m)
			(void)m_free(m);
#endif
		return EINVAL;
	}

	switch (op) {
	case PRCO_SETOPT:
		switch (optname) {
		case ICMP6_FILTER:
		    {
			struct icmp6_filter *p;

			if (optlen != sizeof(*p)) {
				error = EMSGSIZE;
				break;
			}
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
			if (inp->in6p_icmp6filt == NULL) {
				error = EINVAL;
				break;
			}
			error = sooptcopyin(sopt, inp->in6p_icmp6filt, optlen,
				optlen);
#else
			p = mtod(m, struct icmp6_filter *);
			if (!p || !in6p->in6p_icmp6filt) {
				error = EINVAL;
				break;
			}
			bcopy(p, in6p->in6p_icmp6filt,
				sizeof(struct icmp6_filter));
			error = 0;
#endif
			break;
		    }

		default:
			error = ENOPROTOOPT;
			break;
		}
#if !(defined(__FreeBSD__) && __FreeBSD__ >= 3)
		if (m)
			(void)m_freem(m);
#endif		
		break;

	case PRCO_GETOPT:
		switch (optname) {
		case ICMP6_FILTER:
		    {
#if defined(__FreeBSD__) && __FreeBSD__ >= 3
			if (inp->in6p_icmp6filt == NULL) {
				error = EINVAL;
				break;
			}
			error = sooptcopyout(sopt, inp->in6p_icmp6filt,
				sizeof(struct icmp6_filter));
#else
			struct icmp6_filter *p;

			if (!in6p->in6p_icmp6filt) {
				error = EINVAL;
				break;
			}
			*mp = m = m_get(M_WAIT, MT_SOOPTS);
			m->m_len = sizeof(struct icmp6_filter);
			p = mtod(m, struct icmp6_filter *);
			bcopy(in6p->in6p_icmp6filt, p,
				sizeof(struct icmp6_filter));
			error = 0;
#endif
			break;
		    }

		default:
			error = ENOPROTOOPT;
			break;
		}
		break;
	}

	return(error);
}
#ifdef HAVE_NRL_INPCB
#undef sotoin6pcb
#undef in6pcb
#undef in6p_icmp6filt
#endif

#ifndef HAVE_PPSRATECHECK
#ifndef timersub
#define	timersub(tvp, uvp, vvp)						\
	do {								\
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
		if ((vvp)->tv_usec < 0) {				\
			(vvp)->tv_sec--;				\
			(vvp)->tv_usec += 1000000;			\
		}							\
	} while (0)
#endif

/*
 * ppsratecheck(): packets (or events) per second limitation.
 */
static int
ppsratecheck(lasttime, curpps, maxpps)
	struct timeval *lasttime;
	int *curpps;
	int maxpps;	/* maximum pps allowed */
{
	struct timeval tv, delta;
	int s, rv;

	s = splclock(); 
#ifndef __FreeBSD__
	tv = mono_time;
#else
	microtime(&tv);
#endif
	splx(s);

	timersub(&tv, lasttime, &delta);

	/*
	 * check for 0,0 is so that the message will be seen at least once.
	 * if more than one second have passed since the last update of
	 * lasttime, reset the counter.
	 *
	 * we do increment *curpps even in *curpps < maxpps case, as some may
	 * try to use *curpps for stat purposes as well.
	 */
	if ((lasttime->tv_sec == 0 && lasttime->tv_usec == 0) ||
	    delta.tv_sec >= 1) {
		*lasttime = tv;
		*curpps = 0;
		rv = 1;
	} else if (maxpps < 0)
		rv = 1;
	else if (*curpps < maxpps)
		rv = 1;
	else
		rv = 0;

#if 1 /* DIAGNOSTIC? */
	/* be careful about wrap-around */
	if (*curpps + 1 > *curpps)
		*curpps = *curpps + 1;
#else
	/*
	 * assume that there's not too many calls to this function.
	 * not sure if the assumption holds, as it depends on *caller's*
	 * behavior, not the behavior of this function.
	 * IMHO it is wrong to make assumption on the caller's behavior,
	 * so the above #if is #if 1, not #ifdef DIAGNOSTIC.
	 */
	*curpps = *curpps + 1;
#endif

	return (rv);
}
#endif

/*
 * Perform rate limit check.
 * Returns 0 if it is okay to send the icmp6 packet.
 * Returns 1 if the router SHOULD NOT send this icmp6 packet due to rate
 * limitation.
 *
 * XXX per-destination/type check necessary?
 */
static int
icmp6_ratelimit(dst, type, code)
	const struct in6_addr *dst;	/* not used at this moment */
	const int type;			/* not used at this moment */
	const int code;			/* not used at this moment */
{
	int ret;

	ret = 0;	/* okay to send */

	/* PPS limit */
	if (!ppsratecheck(&icmp6errppslim_last, &icmp6errpps_count,
	    icmp6errppslim)) {
		/* The packet is subject to rate limit */
		ret++;
	}

	return ret;
}

/*
 * Swap ip6_src and the address in the homeaddress destination option.
 * Since the dest6 processing routine swaps ip6_src and homeaddr (if
 * any), we must swap them again when we send back the ip datagram to
 * the sender (ex. icmp6_error).
 *
 * returns 0 if succeeded.
 */
static int
icmp6_recover_src(m)
	struct mbuf *m; /* original ip6 packet */
{
	int off, nxt, finished = 0;
	struct ip6_hdr *oip6;
	struct ip6_ext *exts;
	struct ip6_dest *dstopts;
	int dstoptlen;
	u_int8_t *opt;
	int optlen;
	struct ip6_opt_home_address *haopt;
	struct in6_addr t;
	int error = 0;

#ifndef PULLDOWN_TEST 
	IP6_EXTHDR_CHECK(m, 0, sizeof(struct ip6_hdr), ENOBUFS);
	oip6 = mtod(m, struct ip6_hdr *);
#else
	IP6_EXTHDR_GET(oip6, struct ip6_hdr *, m, 0, sizeof(*oip6));
	if (oip6 == NULL) {
		error = ENOBUFS;
		goto bad;
	}
#endif

	off = sizeof(struct ip6_hdr);
	nxt = oip6->ip6_nxt;
	while (off + 2 < m->m_pkthdr.len) {
		switch (nxt) {
		case IPPROTO_DSTOPTS:
#ifndef PULLDOWN_TEST 
			IP6_EXTHDR_CHECK(m, off, sizeof(struct ip6_dest),
					 ENOBUFS);
			dstopts = (struct ip6_dest *)(mtod(m, caddr_t) + off);
#else
			IP6_EXTHDR_GET(dstopts, struct ip6_dest *,
				       m, off, sizeof(*dstopts));
			if (dstopts == NULL) {
				error = ENOBUFS;
				goto bad;
			}
#endif

			dstoptlen = (dstopts->ip6d_len + 1) << 3;
#ifndef PULLDOWN_TEST 
			IP6_EXTHDR_CHECK(m, off, dstoptlen, ENOBUFS);
			dstopts = (struct ip6_dest *)(mtod(m, caddr_t) + off);
#else
			IP6_EXTHDR_GET(dstopts, struct ip6_dest *,
				       m, off, dstoptlen);
			if (dstopts == NULL) {
				error = ENOBUFS;
				goto bad;
			}
#endif
			off += dstoptlen;
			nxt = dstopts->ip6d_nxt;

			/* find homeaddress dstopt */
			dstoptlen -= sizeof(struct ip6_dest);
			opt = (u_int8_t *)dstopts + sizeof(struct ip6_dest);
			for (optlen = 0; (dstoptlen > 0 && finished == 0);
			     dstoptlen -= optlen, opt += optlen) {
				if ((*opt != IP6OPT_PAD1) &&
				    (dstoptlen < IP6OPT_MINLEN || *(opt + 1) + 2 > dstoptlen)) {
					error = EINVAL;
					goto bad;
				    }
				    switch (*opt) {
				    case IP6OPT_PAD1:
					    optlen = 1;
					    break;
				    case IP6OPT_PADN:
					    optlen = *(opt + 1) + 2;
					    break;
				    case IP6OPT_HOME_ADDRESS:
					    haopt = (struct ip6_opt_home_address *)opt;
					    optlen = haopt->ip6oh_len + 2;
					    if (optlen < sizeof(*haopt)) {
						    error = EINVAL;
						    goto bad;
					    }

					    /* swap */
					    bcopy(&haopt->ip6oh_addr,
						  &t,
						  sizeof(haopt->ip6oh_addr));
					    bcopy(&oip6->ip6_src,
						  &haopt->ip6oh_addr,
						  sizeof(oip6->ip6_src));
					    bcopy(&t,
						  &oip6->ip6_src,
						  sizeof(t));
					    finished = 1;
					    break;
				    default:
					    optlen = ip6_unknown_opt(opt, m,
						opt - mtod(m, u_int8_t *));
					    if (optlen == -1) {
						    error = EINVAL;
						    goto bad;
					    }
					    optlen += 2;
					    break;
				    }
			}
			break;

		default:
#ifndef PULLDOWN_TEST 
			IP6_EXTHDR_CHECK(m, off, sizeof(struct ip6_ext),
					 ENOBUFS);
			exts = (struct ip6_ext *)(mtod(m, caddr_t) + off);
#else
			IP6_EXTHDR_GET(exts, struct ip6_ext *,
				       m, off, sizeof(*exts));
			if (exts == NULL) {
				error = ENOBUFS;
				goto bad;
			}
#endif
			nxt = exts->ip6e_nxt;
			off += (exts->ip6e_len + 1) << 3;
			break;
		}
		if (finished)
			break;
	}

	return (0);
 bad:
	ip6_delaux(m);
	m_freem(m);
	return (error);
}

#if defined(__NetBSD__) || defined(__OpenBSD__)
static struct rtentry *
icmp6_mtudisc_clone(dst)
	struct sockaddr *dst;
{
	struct rtentry *rt;
	int    error;

	rt = rtalloc1(dst, 1);
	if (rt == 0)
		return NULL;

	/* If we didn't get a host route, allocate one */
	if ((rt->rt_flags & RTF_HOST) == 0) {
		struct rtentry *nrt;

		error = rtrequest((int) RTM_ADD, dst,
		    (struct sockaddr *) rt->rt_gateway,
		    (struct sockaddr *) 0,
		    RTF_GATEWAY | RTF_HOST | RTF_DYNAMIC, &nrt);
		if (error) {
			rtfree(rt);
			return NULL;
		}
		nrt->rt_rmx = rt->rt_rmx;
		rtfree(rt);
		rt = nrt;
	}
	error = rt_timer_add(rt, icmp6_mtudisc_timeout,
			icmp6_mtudisc_timeout_q);
	if (error) {
		rtfree(rt);
		return NULL;
	}

	return rt;	/* caller need to call rtfree() */
}

static void
icmp6_mtudisc_timeout(rt, r)
	struct rtentry *rt;
	struct rttimer *r;
{
	if (rt == NULL)
		panic("icmp6_mtudisc_timeout: bad route to timeout");
	if ((rt->rt_flags & (RTF_DYNAMIC | RTF_HOST)) ==
	    (RTF_DYNAMIC | RTF_HOST)) {
		rtrequest((int) RTM_DELETE, (struct sockaddr *)rt_key(rt),
		    rt->rt_gateway, rt_mask(rt), rt->rt_flags, 0);
	} else {
		if ((rt->rt_rmx.rmx_locks & RTV_MTU) == 0)
			rt->rt_rmx.rmx_mtu = rt->rt_ifp->if_mtu;
	}
}

static void
icmp6_redirect_timeout(rt, r)
	struct rtentry *rt;
	struct rttimer *r;
{
	if (rt == NULL)
		panic("icmp6_redirect_timeout: bad route to timeout");
	if ((rt->rt_flags & (RTF_GATEWAY | RTF_DYNAMIC | RTF_HOST)) ==
	    (RTF_GATEWAY | RTF_DYNAMIC | RTF_HOST)) {
		rtrequest((int) RTM_DELETE, (struct sockaddr *)rt_key(rt),
		    rt->rt_gateway, rt_mask(rt), rt->rt_flags, 0);
	}
}
#endif /* __NetBSD__ || __OpenBSD__ */

#ifdef __bsdi__
void
icmp6_mtuexpire(rt, rtt)
	struct rtentry *rt;
	struct rttimer *rtt;
{
	rt->rt_flags |= RTF_PROBEMTU;
	Free(rtt);
}

int *icmp6_sysvars[] = ICMPV6CTL_VARS;

int
icmp6_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	if (name[0] >= ICMPV6CTL_MAXID)
		return (EOPNOTSUPP);
	switch (name[0]) {
#if 0
	ICMPV6CTL_ND6_PRUNE:
	ICMPV6CTL_ND6_DELAY:
	ICMPV6CTL_ND6_UMAXTRIES:
	ICMPV6CTL_ND6_MMAXTRIES:
	ICMPV6CTL_ND6_USELOOPBACK:
		/* need to check the value. */
#endif
	case ICMPV6CTL_STATS:
		return sysctl_rdtrunc(oldp, oldlenp, newp, &icmp6stat,
		    sizeof(icmp6stat));
	case ICMPV6CTL_ND6_DRLIST:
	case ICMPV6CTL_ND6_PRLIST:
		return nd6_sysctl(name[0], oldp, oldlenp, newp, newlen);
	default:
		return (sysctl_int_arr(icmp6_sysvars, name, namelen,
		    oldp, oldlenp, newp, newlen));
	}
}
#endif /* __bsdi__ */

#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <vm/vm.h>
#include <sys/sysctl.h>
int
icmp6_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return ENOTDIR;

	switch (name[0]) {

	case ICMPV6CTL_REDIRACCEPT:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&icmp6_rediraccept);
	case ICMPV6CTL_REDIRTIMEOUT:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&icmp6_redirtimeout);
	case ICMPV6CTL_STATS:
		return sysctl_rdstruct(oldp, oldlenp, newp,
				&icmp6stat, sizeof(icmp6stat));
	case ICMPV6CTL_ND6_PRUNE:
		return sysctl_int(oldp, oldlenp, newp, newlen, &nd6_prune);
	case ICMPV6CTL_ND6_DELAY:
		return sysctl_int(oldp, oldlenp, newp, newlen, &nd6_delay);
	case ICMPV6CTL_ND6_UMAXTRIES:
		return sysctl_int(oldp, oldlenp, newp, newlen, &nd6_umaxtries);
	case ICMPV6CTL_ND6_MMAXTRIES:
		return sysctl_int(oldp, oldlenp, newp, newlen, &nd6_mmaxtries);
	case ICMPV6CTL_ND6_USELOOPBACK:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&nd6_useloopback);
	case ICMPV6CTL_NODEINFO:
		return sysctl_int(oldp, oldlenp, newp, newlen, &icmp6_nodeinfo);
	case ICMPV6CTL_ERRPPSLIMIT:
		return sysctl_int(oldp, oldlenp, newp, newlen, &icmp6errppslim);
	case ICMPV6CTL_ND6_MAXNUDHINT:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&nd6_maxnudhint);
	case ICMPV6CTL_MTUDISC_HIWAT:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&icmp6_mtudisc_hiwat);
	case ICMPV6CTL_MTUDISC_LOWAT:
		return sysctl_int(oldp, oldlenp, newp, newlen,
				&icmp6_mtudisc_lowat);
	case ICMPV6CTL_ND6_DEBUG:
		return sysctl_int(oldp, oldlenp, newp, newlen, &nd6_debug);
	case ICMPV6CTL_ND6_DRLIST:
	case ICMPV6CTL_ND6_PRLIST:
		return nd6_sysctl(name[0], oldp, oldlenp, newp, newlen);
	default:
		return ENOPROTOOPT;
	}
	/* NOTREACHED */
}
#endif /* __NetBSD__ */
