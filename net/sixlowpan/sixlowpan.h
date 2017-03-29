/****************************************************************************
 * net/sixlowpan/sixlowpan.h
 *
 *   Copyright (C) 2016 Gregory Nutt. All rights reserved.
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

#ifndef _NET_SIXLOWPAN_SIXLOWPAN_H
#define _NET_SIXLOWPAN_SIXLOWPAN_H

/****************************************************************************
 * Included Files
 ****************************************************************************/

#include <nuttx/config.h>
#include <sys/types.h>

#ifdef CONFIG_NET_6LOWPAN

/****************************************************************************
 * Pre-processor Definitions
 ****************************************************************************/

/* Rime addres macros */
/* Copy a Rime address */

#define rimeaddr_copy(dest,src) \
  memcpy(dest, src, CONFIG_NET_6LOWPAN_RIMEADDR_SIZE)

/* Compare two Rime addresses */

#define rimeaddr_cmp(addr1,addr2) \
  (memcmp(addr1, addr2, CONFIG_NET_6LOWPAN_RIMEADDR_SIZE) == 0)

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Data
 ****************************************************************************/

/* A pointer to the optional, architecture-specific compressor */

struct sixlowpan_nhcompressor_s; /* Foward reference */
extern FAR struct sixlowpan_nhcompressor_s *g_sixlowpan_compressor;

#ifdef CONFIG_NET_6LOWPAN_SNIFFER
/* Rime Sniffer support for one single listener to enable trace of IP */

struct sixlowpan_rime_sniffer_s; /* Foward reference */
extern FAR struct sixlowpan_rime_sniffer_s *g_sixlowpan_sniffer;
#endif

/****************************************************************************
 * Public Types
 ****************************************************************************/

/****************************************************************************
 * Public Function Prototypes
 ****************************************************************************/

struct net_driver_s;         /* Forward reference */
struct ieee802154_driver_s;  /* Forward reference */
struct rimeaddr_s;           /* Forward reference */
struct socket;               /* Forward reference */

/****************************************************************************
 * Name: sixlowpan_initialize
 *
 * Description:
 *   sixlowpan_initialize() is called during OS initialization at power-up
 *   reset.  It is called from the common net_setup() function.
 *   sixlowpan_initialize() configures 6loWPAN networking data structures.
 *   It is called prior to platform-specific driver initialization so that
 *   the 6loWPAN networking subsystem is prepared to deal with network
 *   driver initialization actions.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

void sixlowpan_initialize(void);

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
                               size_t len);
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
                               size_t len);
#endif

/****************************************************************************
 * Name: sixlowpan_hc06_initialize
 *
 * Description:
 *   sixlowpan_hc06_initialize() is called during OS initialization at power-up
 *   reset.  It is called from the common sixlowpan_initialize() function.
 *   sixlowpan_hc06_initialize() configures HC06 networking data structures.
 *   It is called prior to platform-specific driver initialization so that
 *   the 6loWPAN networking subsystem is prepared to deal with network
 *   driver initialization actions.
 *
 * Input Parameters:
 *   None
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

#ifdef CONFIG_NET_6LOWPAN_COMPRESSION_HC06
void sixlowpan_hc06_initialize(void);
#endif

/****************************************************************************
 * Name: sixlowpan_hc06_initialize
 *
 * Description:
 *   Compress IP/UDP header
 *
 *   This function is called by the 6lowpan code to create a compressed
 *   6lowpan packet in the packetbuf buffer from a full IPv6 packet in the
 *   uip_buf buffer.
 *
 *     HC-06 (draft-ietf-6lowpan-hc, version 6)
 *     http://tools.ietf.org/html/draft-ietf-6lowpan-hc-06
 *
 *   NOTE: sixlowpan_compresshdr_hc06() does not support ISA100_UDP header
 *   compression
 *
 * Input Parameters:
 *   dev      - A reference to the IEE802.15.4 network device state
 *   destaddr - L2 destination address, needed to compress IP dest
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

#ifdef CONFIG_NET_6LOWPAN_COMPRESSION_HC06
void sixlowpan_compresshdr_hc06(FAR struct net_driver_s *dev,
                                FAR struct rimeaddr_s *destaddr);
#endif

/****************************************************************************
 * Name: sixlowpan_hc06_initialize
 *
 * Description:
 *   Uncompress HC06 (i.e., IPHC and LOWPAN_UDP) headers and put them in
 *   sixlowpan_buf
 *
 *   This function is called by the input function when the dispatch is HC06.
 *   We process the packet in the rime buffer, uncompress the header fields,
 *   and copy the result in the sixlowpan buffer.  At the end of the
 *   decompression, g_rime_hdrlen and g_uncompressed_hdrlen are set to the
 *   appropriate values
 *
 * Input Parmeters:
 *   dev   - A reference to the IEE802.15.4 network device state
 *   iplen - Equal to 0 if the packet is not a fragment (IP length is then
 *           inferred from the L2 length), non 0 if the packet is a 1st
 *           fragment.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

#ifdef CONFIG_NET_6LOWPAN_COMPRESSION_HC06
void sixlowpan_uncompresshdr_hc06(FAR struct net_driver_s *dev,
                                  uint16_t iplen);
#endif

/****************************************************************************
 * Name: sixlowpan_compresshdr_hc1
 *
 * Description:
 *   Compress IP/UDP header using HC1 and HC_UDP
 *
 *   This function is called by the 6lowpan code to create a compressed
 *   6lowpan packet in the packetbuf buffer from a full IPv6 packet in the
 *   uip_buf buffer.
 *
 * Input Parmeters:
 *   dev      - A reference to the IEE802.15.4 network device state
 *   destaddr - L2 destination address, needed to compress the IP
 *              destination field
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

#ifdef CONFIG_NET_6LOWPAN_COMPRESSION_HC1
void sixlowpan_compresshdr_hc1(FAR struct net_driver_s *dev,
                               FAR struct rimeaddr_s *destaddr);
#endif

/****************************************************************************
 * Name: sixlowpan_uncompresshdr_hc1
 *
 * Description:
 *   Uncompress HC1 (and HC_UDP) headers and put them in sixlowpan_buf
 *
 *   This function is called by the input function when the dispatch is
 *   HC1.  It processes the packet in the rime buffer, uncompresses the
 *   header fields, and copies the result in the sixlowpan buffer.  At the
 *   end of the decompression, g_rime_hdrlen and uncompressed_hdr_len
 *   are set to the appropriate values
 *
 * Input Parameters:
 *   dev   - A reference to the IEE802.15.4 network device state
 *   iplen - Equal to 0 if the packet is not a fragment (IP length is then
 *           inferred from the L2 length), non 0 if the packet is a 1st
 *           fragment.
 *
 * Returned Value:
 *   None
 *
 ****************************************************************************/

#ifdef CONFIG_NET_6LOWPAN_COMPRESSION_HC1
void sixlowpan_uncompresshdr_hc1(FAR struct net_driver_s *dev,
                                 uint16_t ip_len);
#endif

/****************************************************************************
 * Name: sixlowpan_pktbuf_reset
 *
 * Description:
 *   Reset all attributes and addresses in the packet buffer metadata in the
 *   provided IEEE802.15.4 MAC driver structure.
 *
 ****************************************************************************/

void sixlowpan_pktbuf_reset(FAR struct ieee802154_driver_s *ieee);

#endif /* CONFIG_NET_6LOWPAN */
#endif /* _NET_SIXLOWPAN_SIXLOWPAN_H */
