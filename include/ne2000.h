#ifndef __NE2000_DOT_H__
#define __NE2000_DOT_H__
/*
Ported to U-Boot  by Christian Pellegrin <chri@ascensit.com>

Based on sources from the Linux kernel (pcnet_cs.c, 8390.h) and
eCOS(if_dp83902a.c, if_dp83902a.h). Both of these 2 wonderful world
are GPL, so this is, of course, GPL.


==========================================================================

      dev/dp83902a.h

      National Semiconductor DP83902a ethernet chip

==========================================================================
####ECOSGPLCOPYRIGHTBEGIN####
 -------------------------------------------
 This file is part of eCos, the Embedded Configurable Operating System.
 Copyright (C) 1998, 1999, 2000, 2001, 2002 Red Hat, Inc.

 eCos is free software; you can redistribute it and/or modify it under
 the terms of the GNU General Public License as published by the Free
 Software Foundation; either version 2 or (at your option) any later version.

 eCos is distributed in the hope that it will be useful, but WITHOUT ANY
 WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with eCos; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.

 As a special exception, if other files instantiate templates or use macros
 or inline functions from this file, or you compile this file and link it
 with other works to produce a work based on this file, this file does not
 by itself cause the resulting work to be covered by the GNU General Public
 License. However the source code for this file must still be made available
 in accordance with section (3) of the GNU General Public License.

 This exception does not invalidate any other reasons why a work based on
 this file might be covered by the GNU General Public License.

 Alternative licenses for eCos may be arranged by contacting Red Hat, Inc.
 at http://sources.redhat.com/ecos/ecos-license/ */
/*-------------------------------------------
####ECOSGPLCOPYRIGHTEND####
####BSDCOPYRIGHTBEGIN####

 -------------------------------------------

 Portions of this software may have been derived from OpenBSD or other sources,
 and are covered by the appropriate copyright disclaimers included herein.

 -------------------------------------------

####BSDCOPYRIGHTEND####
==========================================================================
#####DESCRIPTIONBEGIN####

 Author(s):    gthomas
 Contributors: gthomas, jskov
 Date:         2001-06-13
 Purpose:
 Description:

####DESCRIPTIONEND####

==========================================================================

*/

#include <types.h>

typedef struct dp83902a_priv_data {
    uint16_t base;
    uint16_t data;
    bool rtl8019;
    int tx_next;           /* First free Tx page */
    int tx_int;            /* Expecting interrupt from this buffer */
    int rx_next;           /* First free Rx page */
    int tx1, tx2;          /* Page numbers for Tx buffers */
    /* unsigned long tx1_key, tx2_key;   // Used to ack when packet sent */
    int tx1_len, tx2_len;
    bool tx_started, running;
    uint8_t esa[6];
    void* plf_priv;

    /* Buffer allocation */
    int tx_buf1, tx_buf2;
    int rx_buf_start, rx_buf_end;
} dp83902a_priv_data_t;

/*
 ------------------------------------------------------------------------
 Some forward declarations
*/
static void dp83902a_poll(void);

/* ------------------------------------------------------------------------ */
/* Register offsets */

#define DP_CR          0x00
#define DP_CLDA0       0x01
#define DP_PSTART      0x01             /* write */
#define DP_CLDA1       0x02
#define DP_PSTOP       0x02             /* write */
#define DP_BNDRY       0x03
#define DP_TSR         0x04
#define DP_TPSR        0x04             /* write */
#define DP_NCR         0x05
#define DP_TBCL        0x05             /* write */
#define DP_FIFO        0x06
#define DP_TBCH        0x06             /* write */
#define DP_ISR         0x07
#define DP_CRDA0       0x08
#define DP_RSAL        0x08             /* write */
#define DP_CRDA1       0x09
#define DP_RSAH        0x09             /* write */
#define DP_RBCL        0x0a             /* write */
#define DP_RBCH        0x0b             /* write */
#define DP_RSR         0x0c
#define DP_RCR         0x0c             /* write */
#define DP_FER         0x0d
#define DP_TCR         0x0d             /* write */
#define DP_CER         0x0e
#define DP_DCR         0x0e             /* write */
#define DP_MISSED      0x0f
#define DP_IMR         0x0f             /* write */
#define DP_DATAPORT    0x10             /* "eprom" data port */

#define DP_P1_CR       0x00
#define DP_P1_PAR0     0x01
#define DP_P1_PAR1     0x02
#define DP_P1_PAR2     0x03
#define DP_P1_PAR3     0x04
#define DP_P1_PAR4     0x05
#define DP_P1_PAR5     0x06
#define DP_P1_CURP     0x07
#define DP_P1_MAR0     0x08
#define DP_P1_MAR1     0x09
#define DP_P1_MAR2     0x0a
#define DP_P1_MAR3     0x0b
#define DP_P1_MAR4     0x0c
#define DP_P1_MAR5     0x0d
#define DP_P1_MAR6     0x0e
#define DP_P1_MAR7     0x0f

#define DP_P2_CR       0x00
#define DP_P2_PSTART   0x01
#define DP_P2_CLDA0    0x01             /* write */
#define DP_P2_PSTOP    0x02
#define DP_P2_CLDA1    0x02             /* write */
#define DP_P2_RNPP     0x03
#define DP_P2_TPSR     0x04
#define DP_P2_LNPP     0x05
#define DP_P2_ACH      0x06
#define DP_P2_ACL      0x07
#define DP_P2_RCR      0x0c
#define DP_P2_TCR      0x0d
#define DP_P2_DCR      0x0e
#define DP_P2_IMR      0x0f

/* Command register - common to all pages */

#define DP_CR_STOP    0x01   /* Stop: software reset */
#define DP_CR_START   0x02   /* Start: initialize device */
#define DP_CR_TXPKT   0x04   /* Transmit packet */
#define DP_CR_RDMA    0x08   /* Read DMA  (recv data from device) */
#define DP_CR_WDMA    0x10   /* Write DMA (send data to device) */
#define DP_CR_SEND    0x18   /* Send packet */
#define DP_CR_NODMA   0x20   /* Remote (or no) DMA */
#define DP_CR_PAGE0   0x00   /* Page select */
#define DP_CR_PAGE1   0x40
#define DP_CR_PAGE2   0x80
#define DP_CR_PAGEMSK 0x3F   /* Used to mask out page bits */

/* Data configuration register */

#define DP_DCR_WTS    0x01   /* 1=16 bit word transfers */
#define DP_DCR_BOS    0x02   /* 1=Little Endian */
#define DP_DCR_LAS    0x04   /* 1=Single 32 bit DMA mode */
#define DP_DCR_LS     0x08   /* 1=normal mode, 0=loopback */
#define DP_DCR_ARM    0x10   /* 0=no send command (program I/O) */
#define DP_DCR_FIFO_1 0x00   /* FIFO threshold */
#define DP_DCR_FIFO_2 0x20
#define DP_DCR_FIFO_4 0x40
#define DP_DCR_FIFO_6 0x60

/* Interrupt status register */

#define DP_ISR_RxP    0x01   /* Packet received */
#define DP_ISR_TxP    0x02   /* Packet transmitted */
#define DP_ISR_RxE    0x04   /* Receive error */
#define DP_ISR_TxE    0x08   /* Transmit error */
#define DP_ISR_OFLW   0x10   /* Receive overflow */
#define DP_ISR_CNT    0x20   /* Tally counters need emptying */
#define DP_ISR_RDC    0x40   /* Remote DMA complete */
#define DP_ISR_RESET  0x80   /* Device has reset (shutdown, error) */

/* Interrupt mask register */

#define DP_IMR_RxP    0x01   /* Packet received */
#define DP_IMR_TxP    0x02   /* Packet transmitted */
#define DP_IMR_RxE    0x04   /* Receive error */
#define DP_IMR_TxE    0x08   /* Transmit error */
#define DP_IMR_OFLW   0x10   /* Receive overflow */
#define DP_IMR_CNT    0x20   /* Tall counters need emptying */
#define DP_IMR_RDC    0x40   /* Remote DMA complete */

#define DP_IMR_All    0x3F   /* Everything but remote DMA */

/* Receiver control register */

#define DP_RCR_SEP    0x01   /* Save bad(error) packets */
#define DP_RCR_AR     0x02   /* Accept runt packets */
#define DP_RCR_AB     0x04   /* Accept broadcast packets */
#define DP_RCR_AM     0x08   /* Accept multicast packets */
#define DP_RCR_PROM   0x10   /* Promiscuous mode */
#define DP_RCR_MON    0x20   /* Monitor mode - 1=accept no packets */

/* Receiver status register */

#define DP_RSR_RxP    0x01   /* Packet received */
#define DP_RSR_CRC    0x02   /* CRC error */
#define DP_RSR_FRAME  0x04   /* Framing error */
#define DP_RSR_FO     0x08   /* FIFO overrun */
#define DP_RSR_MISS   0x10   /* Missed packet */
#define DP_RSR_PHY    0x20   /* 0=pad match, 1=mad match */
#define DP_RSR_DIS    0x40   /* Receiver disabled */
#define DP_RSR_DFR    0x80   /* Receiver processing deferred */

/* Transmitter control register */

#define DP_TCR_NOCRC  0x01   /* 1=inhibit CRC */
#define DP_TCR_NORMAL 0x00   /* Normal transmitter operation */
#define DP_TCR_LOCAL  0x02   /* Internal NIC loopback */
#define DP_TCR_INLOOP 0x04   /* Full internal loopback */
#define DP_TCR_OUTLOOP 0x08  /* External loopback */
#define DP_TCR_ATD    0x10   /* Auto transmit disable */
#define DP_TCR_OFFSET 0x20   /* Collision offset adjust */

/* Transmit status register */

#define DP_TSR_TxP    0x01   /* Packet transmitted */
#define DP_TSR_COL    0x04   /* Collision (at least one) */
#define DP_TSR_ABT    0x08   /* Aborted because of too many collisions */
#define DP_TSR_CRS    0x10   /* Lost carrier */
#define DP_TSR_FU     0x20   /* FIFO underrun */
#define DP_TSR_CDH    0x40   /* Collision Detect Heartbeat */
#define DP_TSR_OWC    0x80   /* Collision outside normal window */

#define IEEE_8023_MAX_FRAME         1518    /* Largest possible ethernet frame */
#define IEEE_8023_MIN_FRAME           64    /* Smallest possible ethernet frame */

/* Some generic ethernet register configurations. */
#define E8390_TX_IRQ_MASK	0xa	/* For register EN0_ISR */
#define E8390_RX_IRQ_MASK	0x5
#define E8390_RXCONFIG		0x4	/* EN0_RXCR: broadcasts, no multicast,errors */
#define E8390_RXOFF		0x20	/* EN0_RXCR: Accept no packets */
#define E8390_TXCONFIG		0x00	/* EN0_TXCR: Normal transmit mode */
#define E8390_TXOFF		0x02	/* EN0_TXCR: Transmitter off */

/*  Register accessed at EN_CMD, the 8390 base addr.  */
#define E8390_STOP	0x01	/* Stop and reset the chip */
#define E8390_START	0x02	/* Start the chip, clear reset */
#define E8390_TRANS	0x04	/* Transmit a frame */
#define E8390_RREAD	0x08	/* Remote read */
#define E8390_RWRITE	0x10	/* Remote write  */
#define E8390_NODMA	0x20	/* Remote DMA */
#define E8390_PAGE0	0x00	/* Select page chip registers */
#define E8390_PAGE1	0x40	/* using the two high-order bits */
#define E8390_PAGE2	0x80	/* Page 3 is invalid. */

#define E8390_CMD	0x00  /* The command register (for all pages) */
/* Page 0 register offsets. */
#define EN0_CLDALO	0x01	/* Low byte of current local dma addr  RD */
#define EN0_STARTPG	0x01	/* Starting page of ring bfr WR */
#define EN0_CLDAHI	0x02	/* High byte of current local dma addr  RD */
#define EN0_STOPPG	0x02	/* Ending page +1 of ring bfr WR */
#define EN0_BOUNDARY	0x03	/* Boundary page of ring bfr RD WR */
#define EN0_TSR		0x04	/* Transmit status reg RD */
#define EN0_TPSR	0x04	/* Transmit starting page WR */
#define EN0_NCR		0x05	/* Number of collision reg RD */
#define EN0_TCNTLO	0x05	/* Low  byte of tx byte count WR */
#define EN0_FIFO	0x06	/* FIFO RD */
#define EN0_TCNTHI	0x06	/* High byte of tx byte count WR */
#define EN0_ISR		0x07	/* Interrupt status reg RD WR */
#define EN0_CRDALO	0x08	/* low byte of current remote dma address RD */
#define EN0_RSARLO	0x08	/* Remote start address reg 0 */
#define EN0_CRDAHI	0x09	/* high byte, current remote dma address RD */
#define EN0_RSARHI	0x09	/* Remote start address reg 1 */
#define EN0_RCNTLO	0x0a	/* Remote byte count reg WR */
#define EN0_RCNTHI	0x0b	/* Remote byte count reg WR */
#define EN0_RSR		0x0c	/* rx status reg RD */
#define EN0_RXCR	0x0c	/* RX configuration reg WR */
#define EN0_TXCR	0x0d	/* TX configuration reg WR */
#define EN0_COUNTER0	0x0d	/* Rcv alignment error counter RD */
#define EN0_DCFG	0x0e	/* Data configuration reg WR */
#define EN0_COUNTER1	0x0e	/* Rcv CRC error counter RD */
#define EN0_IMR		0x0f	/* Interrupt mask reg WR */
#define EN0_COUNTER2	0x0f	/* Rcv missed frame error counter RD */

/* Bits in EN0_ISR - Interrupt status register */
#define ENISR_RX	0x01	/* Receiver, no error */
#define ENISR_TX	0x02	/* Transmitter, no error */
#define ENISR_RX_ERR	0x04	/* Receiver, with error */
#define ENISR_TX_ERR	0x08	/* Transmitter, with error */
#define ENISR_OVER	0x10	/* Receiver overwrote the ring */
#define ENISR_COUNTERS	0x20	/* Counters need emptying */
#define ENISR_RDC	0x40	/* remote dma complete */
#define ENISR_RESET	0x80	/* Reset completed */
#define ENISR_ALL	0x3f	/* Interrupts we will enable */

/* Bits in EN0_DCFG - Data config register */
#define ENDCFG_WTS	0x01	/* word transfer mode selection */
#define ENDCFG_BOS	0x02	/* byte order selection */
#define ENDCFG_AUTO_INIT   0x10        /* Auto-init to remove packets from ring */
#define ENDCFG_FIFO		0x40	/* 8 bytes */

/* Page 1 register offsets. */
#define EN1_PHYS   0x01	/* This board's physical enet addr RD WR */
#define EN1_PHYS_SHIFT(i)  i+1 /* Get and set mac address */
#define EN1_CURPAG 0x07	/* Current memory page RD WR */
#define EN1_MULT   0x08	/* Multicast filter mask array (8 bytes) RD WR */
#define EN1_MULT_SHIFT(i)  8+i /* Get and set multicast filter */

/* Bits in received packet status byte and EN0_RSR*/
#define ENRSR_RXOK	0x01	/* Received a good packet */
#define ENRSR_CRC	0x02	/* CRC error */
#define ENRSR_FAE	0x04	/* frame alignment error */
#define ENRSR_FO	0x08	/* FIFO overrun */
#define ENRSR_MPA	0x10	/* missed pkt */
#define ENRSR_PHY	0x20	/* physical/multicast address */
#define ENRSR_DIS	0x40	/* receiver disable. set in monitor mode */
#define ENRSR_DEF	0x80	/* deferring */

/* Transmitted packet status, EN0_TSR. */
#define ENTSR_PTX 0x01	/* Packet transmitted without error */
#define ENTSR_ND  0x02	/* The transmit wasn't deferred. */
#define ENTSR_COL 0x04	/* The transmit collided at least once. */
#define ENTSR_ABT 0x08  /* The transmit collided 16 times, and was deferred. */
#define ENTSR_CRS 0x10	/* The carrier sense was lost. */
#define ENTSR_FU  0x20  /* A "FIFO underrun" occurred during transmit. */
#define ENTSR_CDH 0x40	/* The collision detect "heartbeat" signal was lost. */
#define ENTSR_OWC 0x80  /* There was an out-of-window collision. */

#define NIC_RECEIVE_MONITOR_MODE 0x20

#define PCNET_RESET     0x1f    /* Issue a read to reset, a write to clear. */
#define PCNET_MISC      0x18    /* For IBM CCAE and Socket EA cards */

#endif
