/****************************************************************************
 * net/sixlowpan/sixlowpan_send.c
 *
 *   Copyright (C) 2017 Gregory Nutt. All rights reserved.
 *   Author: Gregory Nutt <gnutt@nuttx.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>

#include <string.h>
#include <assert.h>
#include <errno.h>
#include <debug.h>

#include "nuttx/net/netdev.h"
#include "nuttx/net/ip.h"
#include "nuttx/net/tcp.h"
#include "nuttx/net/udp.h"
#include "nuttx/net/icmpv6.h"
#include "nuttx/net/sixlowpan.h"

#include "netdev/netdev.h"
#include "socket/socket.h"
#include "tcp/tcp.h"
#include "udp/udp.h"
#include "sixlowpan/sixlowpan.h"

#ifdef CONFIG_NET_6LOWPAN

/****************************************************************************
 * Private Types
 ****************************************************************************/

/* IPv6 + TCP header */

struct ipv6tcp_hdr_s
{
  struct ipv6_hdr_s     ipv6;
  struct tcp_hdr_s      tcp;
};

/* IPv6 + UDP header */

struct ipv6udp_hdr_s
{
  struct ipv6_hdr_s     ipv6;
  struct udp_hdr_s      udp;
};

/* IPv6 + ICMPv6 header */

struct ipv6icmp_hdr_s
{
  struct ipv6_hdr_s     ipv6;
  struct icmpv6_iphdr_s icmp;
};

/****************************************************************************
 * Private Functions
 ****************************************************************************/

/****************************************************************************
 * Name: sixlowpan_set_pktattrs
 *
 * Description:
 *   Setup some packet buffer attributes
 * Input Parameters:
 *   ieee - Pointer to IEEE802.15.4 MAC driver structure.
 *   ipv6 - Pointer to the IPv6 header to "compress"
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void sixlowpan_set_pktattrs(FAR struct ieee802154_driver_s *ieee,
                                   FAR const struct ipv6_hdr_s *ipv6)
{
  int attr = 0;

  /* Set protocol in NETWORK_ID */

  ieee->i_pktattrs[PACKETBUF_ATTR_NETWORK_ID] = ipv6->proto;

  /* Assign values to the channel attribute (port or type + code) */

  if (ipv6->proto == IP_PROTO_UDP)
    {
      FAR struct udp_hdr_s *udp = &((FAR struct ipv6udp_hdr_s *)ipv6)->udp;

      attr = udp->srcport;
      if (udp->destport < attr)
        {
          attr = udp->destport;
        }
    }
  else if (ipv6->proto == IP_PROTO_TCP)
    {
      FAR struct tcp_hdr_s *tcp = &((FAR struct ipv6tcp_hdr_s *)ipv6)->tcp;

      attr = tcp->srcport;
      if (tcp->destport < attr)
        {
          attr = tcp->destport;
        }
    }
  else if (ipv6->proto == IP_PROTO_ICMP6)
    {
      FAR struct icmpv6_iphdr_s *icmp = &((FAR struct ipv6icmp_hdr_s *)ipv6)->icmp;

      attr = icmp->type << 8 | icmp->code;
    }

  ieee->i_pktattrs[PACKETBUF_ATTR_CHANNEL] = attr;
}

/****************************************************************************
 * Name: sixlowpan_compress_ipv6hdr
 *
 * Description:
 *   IPv6 dispatch "compression" function.  Packets "Compression" when only
 *   IPv6 dispatch is used
 *
 *   There is no compression in this case, all fields are sent
 *   inline. We just add the IPv6 dispatch byte before the packet.
 *
 *   0               1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *   | IPv6 Dsp      | IPv6 header and payload ...
 *   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 * Input Parameters:
 *   ieee - Pointer to IEEE802.15.4 MAC driver structure.
 *   ipv6 - Pointer to the IPv6 header to "compress"
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

static void sixlowpan_compress_ipv6hdr(FAR struct ieee802154_driver_s *ieee,
                                       FAR const struct ipv6_hdr_s *ipv6)
{
  /* Indicate the IPv6 dispatch and length */

  *ieee->i_rimeptr       = SIXLOWPAN_DISPATCH_IPV6;
  ieee->i_rime_hdrlen   += SIXLOWPAN_IPV6_HDR_LEN;

  /* Copy the IPv6 header and adjust pointers */

  memcpy(ieee->i_rimeptr + ieee->i_rime_hdrlen, ipv6, IPv6_HDRLEN);
  ieee->i_rime_hdrlen   += IPv6_HDRLEN;
  ieee->i_uncomp_hdrlen += IPv6_HDRLEN;
}

/****************************************************************************
 * Name: sixlowpan_send
 *
 * Description:
 *   Process an outgoing UDP or TCP packet.  Takes an IP packet and formats
 *   it to be sent on an 802.15.4 network using 6lowpan.  Called from common
 *   UDP/TCP send logic.
 *
 *  The payload data is in the caller 'buf' and is of length 'len'.
 *  Compressed headers will be added and if necessary the packet is
 *  fragmented. The resulting packet/fragments are put in dev->d_buf and
 *  the first frame will be delivered to the 802.15.4 MAC. via ieee->i_frame.
 *
 * Input Parmeters:
 *
 * Input Parameters:
 *   dev   - The IEEE802.15.4 MAC network driver interface.
 *   ipv6  - IPv6 plus TCP or UDP headers.
 *   buf   - Data to send
 *   len   - Length of data to send
 *   raddr - The MAC address of the destination
 *
 * Returned Value:
 *   Ok is returned on success; Othewise a negated errno value is returned.
 *   This function is expected to fail if the driver is not an IEEE802.15.4
 *   MAC network driver.  In that case, the UDP/TCP will fall back to normal
 *   IPv4/IPv6 formatting.
 *
 * Assumptions:
 *   Called with the network locked.
 *
 ****************************************************************************/

static int sixlowpan_send(FAR struct net_driver_s *dev,
                          FAR const struct ipv6_hdr_s *ipv6, FAR const void *buf,
                          size_t len, net_ipv6addr_t raddr)
{
  FAR struct ieee802154_driver_s *ieee = (FAR struct ieee802154_driver_s *)dev;

  int framer_hdrlen;       /* Framer header length */
  struct rimeaddr_s dest;  /* The MAC address of the destination of the packet */
  uint16_t outlen;         /* Number of bytes processed. */

  /* Initialize device-specific data */

  ieee->i_uncomp_hdrlen = 0;
  ieee->i_rime_hdrlen   = 0;

  /* Reset rime buffer, packet buffer metatadata */

  sixlowpan_pktbuf_reset(ieee);

  ieee->i_rimeptr = &dev->d_buf[PACKETBUF_HDR_SIZE];
  ieee->i_pktattrs[PACKETBUF_ATTR_MAX_MAC_TRANSMISSIONS] = CONFIG_NET_6LOWPAN_MAX_MACTRANSMITS;

#ifdef CONFIG_NET_6LOWPAN_SNIFFER
  if (g_sixlowpan_sniffer != NULL)
    {
      /* Call the attribution when the callback comes, but set attributes here */

      sixlowpan_set_pktattrs(ieee, ipv6);
    }
#endif

  /* Set stream mode for all TCP packets, except FIN packets. */

  if (ipv6->proto == IP_PROTO_TCP)
    {
      FAR const struct tcp_hdr_s *tcp = &((FAR const struct ipv6tcp_hdr_s *)ipv6)->tcp;

      if ((tcp->flags & TCP_FIN) == 0 &&
          (tcp->flags & TCP_CTL) != TCP_ACK)
        {
          ieee->i_pktattrs[PACKETBUF_ATTR_PACKET_TYPE] = PACKETBUF_ATTR_PACKET_TYPE_STREAM;
        }
      else if ((tcp->flags & TCP_FIN) == TCP_FIN)
        {
          ieee->i_pktattrs[PACKETBUF_ATTR_PACKET_TYPE] = PACKETBUF_ATTR_PACKET_TYPE_STREAM_END;
        }
    }

  /* The destination address will be tagged to each outbound packet. If the
   * argument raddr is NULL, we are sending a broadcast packet.
   */

#warning Missing logic

  ninfo("Sending packet len %d\n", len);

#ifndef CONFIG_NET_6LOWPAN_COMPRESSION_IPv6
  if (len >= CONFIG_NET_6LOWPAN_COMPRESSION_THRESHOLD)
    {
      /* Try to compress the headers */

#if defined(CONFIG_NET_6LOWPAN_COMPRESSION_HC1)
      sixlowpan_compresshdr_hc1(dev, &dest);
#elif defined(CONFIG_NET_6LOWPAN_COMPRESSION_HC06)
      sixlowpan_compresshdr_hc06(dev, &dest);
#else
#  error No compression specified
#endif
    }
  else
#endif /* !CONFIG_NET_6LOWPAN_COMPRESSION_IPv6 */
    {
      /* Small.. use IPv6 dispatch (no compression) */

      sixlowpan_compress_ipv6hdr(ieee, ipv6);
    }

  ninfo("Header of len %d\n", ieee->i_rime_hdrlen);

  /* Calculate frame header length. */
#warning Missing logic
  return -ENOSYS;
}

/****************************************************************************
 * Public Functions
 ****************************************************************************/

/****************************************************************************
 * Function: psock_6lowpan_tcp_send
 *
 * Description:
 *   psock_6lowpan_tcp_send() call may be used only when the TCP socket is in a
 *   connected state (so that the intended recipient is known).
 *
 * Parameters:
 *   psock - An instance of the internal socket structure.
 *   buf   - Data to send
 *   len   - Length of data to send
 *
 * Returned Value:
 *   On success, returns the number of characters sent.  On  error,
 *   -1 is returned, and errno is set appropriately.  Returned error numbers
 *   must be consistent with definition of errors reported by send() or
 *   sendto().
 *
 * Assumptions:
 *   Called with the network locked.
 *
 ****************************************************************************/

#ifdef CONFIG_NET_TCP
ssize_t psock_6lowpan_tcp_send(FAR struct socket *psock, FAR const void *buf,
                               size_t len)
{
  FAR struct tcp_conn_s *conn;
  FAR struct net_driver_s *dev;
  struct ipv6tcp_hdr_s ipv6tcp;
  int ret;

  DEBUGASSERT(psock != NULL && psock->s_crefs > 0);
  DEBUGASSERT(psock->s_type == SOCK_STREAM);

  /* Make sure that this is a valid socket */

  if (psock != NULL || psock->s_crefs <= 0)
    {
      nerr("ERROR: Invalid socket\n");
      return (ssize_t)-EBADF;
    }

  /* Make sure that this is a connected TCP socket */

  if (psock->s_type != SOCK_STREAM || !_SS_ISCONNECTED(psock->s_flags))
    {
      nerr("ERROR: Not connected\n");
      return (ssize_t)-ENOTCONN;
    }

  /* Get the underlying TCP connection structure */

  conn = (FAR struct tcp_conn_s *)psock->s_conn;
  DEBUGASSERT(conn != NULL);

#if defined(CONFIG_NET_IPv4) && defined(CONFIG_NET_IPv6)
  /* Ignore if not IPv6 domain */

  if (conn->domain != PF_INET6)
    {
      nwarn("WARNING: Not IPv6\n");
      return (ssize_t)-EPROTOTYPE;
    }
#endif

  /* Route outgoing message to the correct device */

#ifdef CONFIG_NETDEV_MULTINIC
  dev = netdev_findby_ipv6addr(conn->u.ipv6.laddr, conn->u.ipv6.raddr);
  if (dev == NULL || dev->d_lltype != NET_LL_IEEE805154)
    {
      nwarn("WARNING: Not routable or not IEEE802.15.4 MAC\n");
      return (ssize_t)-ENETUNREACH;
    }
#else
  dev = netdev_findby_ipv6addr(conn->u.ipv6.raddr);
  if (dev == NULL)
    {
      nwarn("WARNING: Not routable\n");
      return (ssize_t)-ENETUNREACH;
    }
#endif

#ifdef CONFIG_NET_ICMPv6_NEIGHBOR
  /* Make sure that the IP address mapping is in the Neighbor Table */

  ret = icmpv6_neighbor(conn->u.ipv6.raddr);
  if (ret < 0)
    {
      nerr("ERROR: Not reachable\n");
      return (ssize_t)-ENETUNREACH;
    }
#endif

  /* Initialize the IPv6/TCP headers */
#warning Missing logic

  /* Set the socket state to sending */

  psock->s_flags = _SS_SETSTATE(psock->s_flags, _SF_SEND);

  /* If routable, then call sixlowpan_send() to format and send the 6loWPAN
   * packet.
   */

  ret = sixlowpan_send(dev, (FAR const struct ipv6_hdr_s *)&ipv6tcp,
                       buf, len, conn->u.ipv6.raddr);
  if (ret < 0)
    {
      nerr("ERROR: sixlowpan_send() failed: %d\n", ret);
    }

  return ret;
}
#endif

/****************************************************************************
 * Function: psock_6lowpan_udp_send
 *
 * Description:
 *   psock_6lowpan_udp_send() call may be used with connectionlesss UDP
 *   sockets.
 *
 * Parameters:
 *   psock - An instance of the internal socket structure.
 *   buf   - Data to send
 *   len   - Length of data to send
 *
 * Returned Value:
 *   On success, returns the number of characters sent.  On  error,
 *   -1 is returned, and errno is set appropriately.  Returned error numbers
 *   must be consistent with definition of errors reported by send() or
 *   sendto().
 *
 * Assumptions:
 *   Called with the network locked.
 *
 ****************************************************************************/

#ifdef CONFIG_NET_UDP
ssize_t psock_6lowpan_udp_send(FAR struct socket *psock, FAR const void *buf,
                               size_t len)
{
  FAR struct udp_conn_s *conn;
  FAR struct net_driver_s *dev;
  struct ipv6udp_hdr_s ipv6udp;
  int ret;

  DEBUGASSERT(psock != NULL && psock->s_crefs > 0);
  DEBUGASSERT(psock->s_type == SOCK_DGRAM);

  /* Make sure that this is a valid socket */

  if (psock != NULL || psock->s_crefs <= 0)
    {
      nerr("ERROR: Invalid socket\n");
      return (ssize_t)-EBADF;
    }

  /* Was the UDP socket connected via connect()? */

  if (psock->s_type != SOCK_DGRAM || !_SS_ISCONNECTED(psock->s_flags))
    {
      /* No, then it is not legal to call send() with this socket. */

      return -ENOTCONN;
    }

  /* Get the underlying UDP "connection" structure */

  conn = (FAR struct udp_conn_s *)psock->s_conn;
  DEBUGASSERT(conn != NULL);

#if defined(CONFIG_NET_IPv4) && defined(CONFIG_NET_IPv6)
  /* Ignore if not IPv6 domain */

  if (conn->domain != PF_INET6)
    {
      nwarn("WARNING: Not IPv6\n");
      return (ssize_t)-EPROTOTYPE;
    }
#endif

  /* Route outgoing message to the correct device */

#ifdef CONFIG_NETDEV_MULTINIC
  dev = netdev_findby_ipv6addr(conn->u.ipv6.laddr, conn->u.ipv6.raddr);
  if (dev == NULL || dev->d_lltype != NET_LL_IEEE805154)
    {
      nwarn("WARNING: Not routable or not IEEE802.15.4 MAC\n");
      return (ssize_t)-ENETUNREACH;
    }
#else
  dev = netdev_findby_ipv6addr(conn->u.ipv6.raddr);
  if (dev == NULL)
    {
      nwarn("WARNING: Not routable\n");
      return (ssize_t)-ENETUNREACH;
    }
#endif

#ifdef CONFIG_NET_ICMPv6_NEIGHBOR
  /* Make sure that the IP address mapping is in the Neighbor Table */

  ret = icmpv6_neighbor(conn->u.ipv6.raddr);
  if (ret < 0)
    {
      nerr("ERROR: Not reachable\n");
      return (ssize_t)-ENETUNREACH;
    }
#endif

  /* Initialize the IPv6/UDP headers */
#warning Missing logic

  /* Set the socket state to sending */

  psock->s_flags = _SS_SETSTATE(psock->s_flags, _SF_SEND);

  /* If routable, then call sixlowpan_send() to format and send the 6loWPAN
   * packet.
   */

  ret = sixlowpan_send(dev, (FAR const struct ipv6_hdr_s *)&ipv6udp,
                       buf, len, conn->u.ipv6.raddr);
  if (ret < 0)
    {
      nerr("ERROR: sixlowpan_send() failed: %d\n", ret);
    }

  return ret;
}
#endif

#endif /* CONFIG_NET_6LOWPAN */
