/*
 * dbox2_i2c.c - mpc8xx i2c driver (dbox-II-project)
 *
 * Copyright (C) 2000-2001 Tmbinc, Gillem (htoa@gmx.net)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/threads.h>	/* NR_CPUS */
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <asm/commproc.h>
#include <asm/pgtable.h>

static void i2c_interrupt(void *, struct pt_regs *regs);

/* ------------------------------------------------------------------------- */

/* parameter stuff */
static int debug;

#define dprintk(fmt, args...) if (debug) printk( fmt, ## args )

/* mutex stuff */
#define I2C_NONE	0
#define I2C_INT_EVENT	1

static DECLARE_MUTEX(i2c_mutex);

/* interrupt stuff */
static wait_queue_head_t i2c_wait;

/* ------------------------------------------------------------------------- */

#define TXBD_R		0x8000  /* Transmit buffer ready to send */
#define TXBD_W		0x2000  /* Wrap, last buffer in buffer circle */
#define TXBD_I		0x1000
#define TXBD_L		0x0800  /* Last, this buffer is the last in this frame */
				/* This bit causes the STOP condition to be sent */
#define TXBD_S		0x0400  /* Start condition.  Causes this BD to transmit a start */

#define TXBD_NAK	0x0004	/* indicates nak */
#define TXBD_UN		0x0002  /* indicates underrun */
#define TXBD_CO		0x0001  /* indicates collision */

#define RXBD_E		0x8000  /* Receive buffer is empty and can be used by CPM */
#define RXBD_W		0x2000  /* Wrap, last receive buffer in buffer circle */
#define RXBD_I		0x1000  /* Int. */
#define RXBD_L		0x0800  /* Last */

#define RXBD_OV		0x0002	/* indicates overrun */

/* ------------------------------------------------------------------------- */

#define I2C_PPC_MASTER	1
#define I2C_PPC_SLAVE	0
#define I2C_BUF_LEN     128
#define I2C_INTR_TIMOUT 500

#define I2C_MAXBD	4

/* ------------------------------------------------------------------------- */

typedef volatile struct I2C_BD {
	unsigned short status;
	unsigned short length;
	unsigned char  *addr;
} I2C_BD;

typedef volatile struct RX_TX_BD {
	I2C_BD *rxbd;
	I2C_BD *txbd;
	unsigned char *rxbuf[I2C_MAXBD];
	unsigned char *txbuf[I2C_MAXBD];
	int rxnum;
	int txnum;
} RX_TX_BD;

static struct RX_TX_BD I2CBD;

static volatile i2c8xx_t *i2c;
static volatile iic_t *iip;
static volatile cpm8xx_t *cp;

/* ------------------------------------------------------------------------- */

static inline int i2c_roundrate(int hz, int speed, int filter, int modval,
				int *brgval, int *totspeed)
{
	int moddiv = 1 << (5 - (modval & 3)), brgdiv, div;

	brgdiv = hz / (moddiv * speed);

	*brgval = brgdiv / 2 - 3 - 2 * filter;

	if ((*brgval < 0) || (*brgval > 255))
		return -1;

	brgdiv = 2 * (*brgval + 3 + 2 * filter);
	div = moddiv * brgdiv;
	*totspeed = hz / div;

	return  0;
}

/* ------------------------------------------------------------------------- */

static int i2c_setrate (int hz, int speed)
{
	volatile immap_t *immap = (volatile immap_t *)IMAP_ADDR ;
	volatile i2c8xx_t *i2c = (volatile i2c8xx_t *)&immap->im_i2c;

	int brgval;
	int modval;	   // 0-3
	int bestspeed_diff = speed;
	int bestspeed_brgval=0;
	int bestspeed_modval=0;
	int bestspeed_filter=0;
	int totspeed;
	int filter=0;	 // Use this fixed value

	for (modval = 0; modval < 4; modval++) {
		if (i2c_roundrate(hz, speed, filter, modval, &brgval, &totspeed) == 0) {
			int diff = speed - totspeed;
			if ((diff >= 0) && (diff < bestspeed_diff)) {
				bestspeed_diff=diff;
				bestspeed_modval=modval;
				bestspeed_brgval=brgval;
				bestspeed_filter=filter;
			}
		}
	}

	dprintk("[i2c-8xx]: Best is:\n");
	dprintk("[i2c-8xx]: CPU=%dhz RATE=%d F=%d I2MOD=%08x I2BRG=%08x DIFF=%dhz\n",
		hz, speed, bestspeed_filter, bestspeed_modval, bestspeed_brgval,
		bestspeed_diff);

	i2c->i2c_i2mod |= ((bestspeed_modval & 3) << 1) | (bestspeed_filter << 3);
	i2c->i2c_i2brg = bestspeed_brgval & 0xff;

	dprintk("[i2c-8xx]: i2mod=%08x i2brg=%08x\n", i2c->i2c_i2mod, i2c->i2c_i2brg);

	return 1 ;
}

/* ------------------------------------------------------------------------- */

static int i2c_init(int speed)
{
	volatile cpic8xx_t *cpic;
	int i;
	int ret = 0;

	/* get immap addr */
	volatile immap_t *immap = (volatile immap_t *)IMAP_ADDR;

	/* get cpm */
	cp = (volatile cpm8xx_t *)&immap->im_cpm;

	/* get i2c */
	iip = (volatile iic_t *)&cp->cp_dparam[PROFF_IIC];

	/* get i2c ppc */
	i2c = (volatile i2c8xx_t *)&immap->im_i2c;

	cpic = (volatile cpic8xx_t *)&immap->im_cpic;

	/* disable relocation */
	iip->iic_rbase = 0 ;

	/* Initialize Port B I2C pins. */
	cp->cp_pbpar |= 0x00000030;
	cp->cp_pbdir |= 0x00000030;
	cp->cp_pbodr |= 0x00000030;

	/* Disable interrupts. */
	i2c->i2c_i2mod = 0;
	i2c->i2c_i2cmr = 0;
	i2c->i2c_i2cer = 0xff;

	/* Set the I2C BRG Clock division factor from desired i2c rate
	 * and current CPU rate (we assume sccr dfbgr field is 0;
	 * divide BRGCLK by 1)
	 */
	i2c_setrate(66 * 1024 * 1024, speed);

	/* Set I2C controller in master mode */
	i2c->i2c_i2com = I2C_PPC_MASTER;

	/* Set SDMA bus arbitration level to 5 (SDCR) */
	immap->im_siu_conf.sc_sdcr = 0x0001;

	iip->iic_rbptr = iip->iic_rbase = m8xx_cpm_dpalloc (I2C_MAXBD*sizeof(I2C_BD)*2);
	iip->iic_tbptr = iip->iic_tbase = iip->iic_rbase + (I2C_MAXBD*sizeof(I2C_BD));

	I2CBD.rxbd = (I2C_BD *)((unsigned char *)&cp->cp_dpmem[iip->iic_rbase]);
	I2CBD.txbd = (I2C_BD *)((unsigned char *)&cp->cp_dpmem[iip->iic_tbase]);

	dprintk("[i2c-8xx]: RBASE = %04x\n", iip->iic_rbase);
	dprintk("[i2c-8xx]: TBASE = %04x\n", iip->iic_tbase);
	dprintk("[i2c-8xx]: RXBD1 = %08x\n", (int)I2CBD.rxbd);
	dprintk("[i2c-8xx]: TXBD1 = %08x\n", (int)I2CBD.txbd);

	for (i = 0; i < I2C_MAXBD; i++) {
		I2CBD.rxbuf[i] = (unsigned char*)m8xx_cpm_hostalloc(I2C_BUF_LEN);
		if (!I2CBD.rxbuf[i]) {
			dprintk("[i2c-8xx]: No more mem available! Restart Kernel!\n");
			return -ENOMEM;
		}

		I2CBD.txbuf[i] = (unsigned char*)m8xx_cpm_hostalloc(I2C_BUF_LEN);
		if (!I2CBD.txbuf[i]) {
			dprintk("[i2c-8xx]: No more mem available! Restart Kernel!\n");
			return -ENOMEM;
		}

		I2CBD.rxbd[i].addr = (unsigned char*)iopa((unsigned long)I2CBD.rxbuf[i]);
		I2CBD.txbd[i].addr = (unsigned char*)iopa((unsigned long)I2CBD.txbuf[i]);

		I2CBD.rxbd[i].status = RXBD_E;
		I2CBD.txbd[i].status = 0;
		I2CBD.rxbd[i].length = 0;
		I2CBD.txbd[i].length = 0;

		dprintk("[i2c-8xx]: RXBD%d->ADDR = %08X\n",i,(uint)I2CBD.rxbd[i].addr);
		dprintk("[i2c-8xx]: TXBD%d->ADDR = %08X\n",i,(uint)I2CBD.txbd[i].addr);
	}

	I2CBD.rxbd[i-1].status |= RXBD_W;
	I2CBD.txbd[i-1].status |= TXBD_W;

	/* Set big endian byte order */
	iip->iic_tfcr = 0x15;
	iip->iic_rfcr = 0x15;

	/* Set maximum receive size. */
	iip->iic_mrblr = I2C_BUF_LEN;

	/* i2c->i2c_i2mod |= ((bestspeed_modval & 3) << 1) | (bestspeed_filter << 3); */
	i2c->i2c_i2brg = 7;

	/* Initialize the BD's */
	while (cp->cp_cpcr & CPM_CR_FLG);

	cp->cp_cpcr = mk_cr_cmd(CPM_CR_CH_I2C, CPM_CR_INIT_TRX) | CPM_CR_FLG;

	while (cp->cp_cpcr & CPM_CR_FLG);

	/* Clear events */
	i2c->i2c_i2cer = 0xff ;

	i2c->i2c_i2cmr = 0x17;

	init_waitqueue_head(&i2c_wait);

	/* Install Interrupt handler */
	cpm_install_handler(CPMVEC_I2C, i2c_interrupt, (void *)iip);

	/* Enable i2c interrupt */
	cpic->cpic_cimr |= 0x10000;

	/* cpic->cpic_cicr |= 0x8080; */

	/* XPC823ZT66B2 bugfix (read errata) */
	i2c->i2c_i2add = 0;

	return ret;
}

/* ------------------------------------------------------------------------- */

static void i2c_interrupt(void *dev_id, struct pt_regs *regs)
{
	volatile immap_t *immap = (volatile immap_t *)IMAP_ADDR;
	volatile i2c8xx_t *i2c = (volatile i2c8xx_t *)&immap->im_i2c;

	dprintk("[i2c-8xx]: (interrupt) MOD: %04X CER: %04X\n", i2c->i2c_i2mod, i2c->i2c_i2cer);

	i2c->i2c_i2mod &= ~1;

	/* Clear interrupt. */
	i2c->i2c_i2cer = 0xff;

	/* Get 'me going again. */
	wake_up_interruptible(&i2c_wait);
}

/* ------------------------------------------------------------------------- */

static int parse_send_msg(unsigned char address, unsigned short size,
			  unsigned char *dataout, int last)
{
	int i,j;

	/* Trying to send message larger than BD */
	if (size > I2C_BUF_LEN) {
		dprintk("[i2c-8xx]: size > maxsize (SEND)\n");
		return -1;
	}

	if (!dataout) {
		dprintk("[i2c-8xx]: NULL POINTER DETECT (SEND)\n");
		return -1;
	}

	if (size == 0) {
		dprintk("[i2c-8xx]: ZERO BYTE WRITE (SEND)\n");
		size++;
	}

	/* Length of message plus dest address */
	I2CBD.txbd[I2CBD.txnum].length = size + 1;

	/* Write destination address to BD */
	I2CBD.txbuf[I2CBD.txnum][0] = (address&(~1));

	i = 1;

	for (j = 0; j < size; i++, j++)
		I2CBD.txbuf[I2CBD.txnum][i] = dataout[j];

	I2CBD.txbd[I2CBD.txnum].status = TXBD_R | TXBD_S/*| TXBD_I*/ /*| TXBD_W*/;

	if (last) {
		dprintk("[i2c-8xx]: LAST SEND\n");
		I2CBD.txbd[I2CBD.txnum].status |= TXBD_L | TXBD_I | TXBD_W;
	}

	I2CBD.txnum++;

	dprintk("[i2c-8xx]: PARSE SEND MSG OK\n");

	return 0;
}

/* ------------------------------------------------------------------------- */

static int parse_recv_msg(unsigned char address, unsigned short size,
			  unsigned char *datain, int last)
{
	/* Trying to send message larger than BD */
	if (size > I2C_BUF_LEN) {
		dprintk("[i2c-8xx]: size > maxsize (RECV)\n");
		return -1;
	}

	if (!datain) {
		dprintk("[i2c-8xx]: NULL POINTER DETECT (RECV)\n");
		return -1;
	}

	if (size == 0) {
		dprintk("[i2c-8xx]: ZERO BYTE WRITE (RECV)\n");
		size++;
	}

	I2CBD.txbuf[I2CBD.txnum][0] = (address | 0x01);
	I2CBD.txbuf[I2CBD.txnum][1] = 0;

	I2CBD.txbd[I2CBD.txnum].length = 1 + size;
	I2CBD.rxbd[I2CBD.rxnum].length = 0;

	I2CBD.txbd[I2CBD.txnum].status = TXBD_R | TXBD_S/*| TXBD_W*/ /*| TXBD_I*/;
	I2CBD.rxbd[I2CBD.txnum].status = RXBD_E | RXBD_I;

	if (last) {
		dprintk("[i2c-8xx]: LAST RECV\n");
		I2CBD.rxbd[I2CBD.rxnum].status |= RXBD_I | RXBD_W;
		I2CBD.txbd[I2CBD.txnum].status |= TXBD_L | TXBD_W;
	}

	I2CBD.rxnum++;
	I2CBD.txnum++;

	return 0;
}

/* ------------------------------------------------------------------------- */

static int dbox2_i2c_xfer(struct i2c_adapter *i2c_adap,
		    const struct i2c_msg msgs[], int num)
{
	unsigned long flags;
	const struct i2c_msg *pmsg;
	int i,last,ret;

	/* HACK HACK HACK */
	if (in_interrupt()) {
		dprintk("[i2c-8xx]: Bad day for you.\n");
		return -EREMOTEIO;
	}

	if (num > (I2C_MAXBD * 2))
		return -EREMOTEIO;

	down(&i2c_mutex);

	ret = num;

	for (i = 0; i < I2C_MAXBD; i++) {
		I2CBD.rxbd[i].status = RXBD_E;
		I2CBD.txbd[i].status = 0;
		I2CBD.rxbd[i].length = 0;
		I2CBD.txbd[i].length = 0;
	}

	I2CBD.rxbd[i-1].status |= RXBD_W;
	I2CBD.txbd[i-1].status |= TXBD_W;

	/* reset buffer pointer */
	I2CBD.rxnum = 0;
	I2CBD.txnum = 0;

	iip->iic_rstate = 0;
	iip->iic_tstate = 0;

	last = 0;

	/* parse msg's */
	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];

		if (i == (num - 1))
			last++;

		dprintk("[i2c-8xx]: addr:=%x, flags:=%x, len:=%x num:=%d\n",
				pmsg->addr, pmsg->flags, pmsg->len, num);

		I2CBD.rxbd[I2CBD.rxnum].length = 0;
		I2CBD.txbd[I2CBD.txnum].length = 0;

		if (pmsg->flags & I2C_M_RD) {
			if (parse_recv_msg(pmsg->addr << 1, pmsg->len, pmsg->buf, last) < 0) {
				up(&i2c_mutex);
				return -EREMOTEIO;
			}
		}
		else {
			if (parse_send_msg(pmsg->addr << 1, pmsg->len, pmsg->buf, last) < 0) {
				up(&i2c_mutex);
				return -EREMOTEIO;
			}
		}
	}

	save_flags(flags);
	cli();

	/* Clear interrupt. */
	i2c->i2c_i2cer = 0xff;

	/* Transmit disable */
	i2c->i2c_i2com &= ~0x80;

	/* Enable I2C */
	i2c->i2c_i2mod |= 1;

	/*
	dprintk("[i2c-8xx]: ON  MOD: %04X CER: %04X COM: %04X ADD: %04X BRG: %04X CMR: %04X\n",\
		i2c->i2c_i2mod,\
		i2c->i2c_i2cer,\
		i2c->i2c_i2com,\
		i2c->i2c_i2add,\
		i2c->i2c_i2brg,\
		i2c->i2c_i2cmr);
	*/

	/* Transmit */
	i2c->i2c_i2com |= 0x80;

	i = interruptible_sleep_on_timeout(&i2c_wait, I2C_INTR_TIMOUT);
	dprintk("[i2c-8xx]: intrspeed:=%d\n", I2C_INTR_TIMOUT - i);
	restore_flags(flags);

	/*
	dprintk("[i2c-8xx]: OFF MOD: %04X CER: %04X COM: %04X ADD: %04X BRG: %04X CMR: %04X\n", \
		i2c->i2c_i2mod,\
		i2c->i2c_i2cer,\
		i2c->i2c_i2com,\
		i2c->i2c_i2add,\
		i2c->i2c_i2brg,\
		i2c->i2c_i2cmr);
	*/

	/* copy rx-buffer */
	I2CBD.rxnum = 0;
	I2CBD.txnum = 0;

	for (i = 0; i < num; i++) {
		pmsg = &msgs[i];

		if (pmsg->flags & I2C_M_RD) {
			if (I2CBD.rxbd[I2CBD.rxnum].status & RXBD_E) {
				dprintk("[i2c-8xx]: RXBD: RX DATA IS EMPTY\n");
				ret = -EREMOTEIO;
			}
			else {
				if (I2CBD.rxbd[I2CBD.rxnum].length == 0) {
					dprintk("[i2c-8xx]: RXBD: NO DATA\n");
					ret = -EREMOTEIO;
				}
			}

			if (I2CBD.rxbd[I2CBD.rxnum].status & RXBD_OV) {
				dprintk("[i2c-8xx]: RXBD: OVERRUN\n");
				I2CBD.rxbd[I2CBD.rxnum].status &= (~RXBD_OV);
				ret = -EREMOTEIO;
			}

			memcpy(pmsg->buf, I2CBD.rxbuf[I2CBD.rxnum], I2CBD.rxbd[I2CBD.rxnum].length);
			I2CBD.rxnum++;
		}
		else {
			if (I2CBD.txbd[I2CBD.txnum].status & TXBD_NAK) {
				dprintk("[i2c-8xx]: TXBD: NAK\n");
				ret = -EREMOTEIO;
			}
			if (I2CBD.txbd[I2CBD.txnum].status & TXBD_UN) {
				dprintk("[i2c-8xx]: TXBD: UNDERRUN\n");
				ret = -EREMOTEIO;
			}
			if (I2CBD.txbd[I2CBD.txnum].status & TXBD_CO) {
				dprintk("[i2c-8xx]: TXBD: COLLISION\n");
				ret = -EREMOTEIO;
			}

			/* clear flags */
			I2CBD.txbd[I2CBD.rxnum].status &= ~(TXBD_NAK|TXBD_UN|TXBD_CO);

			I2CBD.txnum++;
		}
	}

	/* Transmit disable */
	i2c->i2c_i2com &= ~0x80;

	/* Turn off I2C */
	i2c->i2c_i2mod &= ~1;

	up(&i2c_mutex);
	return ret;
}

/* ------------------------------------------------------------------------- */

static int dbox2_i2c_xfer_safe(struct i2c_adapter *i2c_adap,
			       struct i2c_msg msgs[], int num)
{
	if (!i2c_adap)	{
		dprintk("[i2c-8xx]: No i2c adapter.\n");
		return -EREMOTEIO;
	}

	return dbox2_i2c_xfer(i2c_adap, msgs, num);
}

static int algo_control(struct i2c_adapter *adapter, unsigned int cmd,
			unsigned long arg)
{
	return 0;
}

static u32 p8xx_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_SMBUS_EMUL; //  | I2C_FUNC_10BIT_ADDR;  10bit auch erstmal nicht.
}

static struct i2c_algorithm i2c_8xx_algo = {
	.name = "PowerPC 8xx Algo",
	.id = I2C_ALGO_EXP,
	.master_xfer = dbox2_i2c_xfer_safe,
	.algo_control = algo_control,
	.functionality = p8xx_func,
};

static struct i2c_adapter adap = {
	.name = "PowerPC 8xx I2C adapter",
	.algo = &i2c_8xx_algo,
	.timeout = 100,
	.retries = 3,
};

static int __init i2c_algo_8xx_init (void)
{
	printk(KERN_INFO "[i2c-8xx]: mpc 8xx i2c init\n");

	if (i2c_init(100000) < 0) {
		printk(KERN_ERR "[i2c-8xx]: init failed\n");
		return -EIO;
	}

	adap.id = i2c_8xx_algo.id;

	printk(KERN_INFO "[i2c-8xx]: adapter: %x\n", i2c_add_adapter(&adap));

	return 0;
}

static void __exit i2c_8xx_del_bus(struct i2c_adapter *adap)
{
	cpm_free_handler(CPMVEC_I2C);
	i2c_del_adapter(adap);
}

static int __init i2c_8xx_module_init(void)
{
	return i2c_algo_8xx_init();
}

static void __exit i2c_8xx_module_exit(void)
{
	i2c_8xx_del_bus(&adap);
}

module_init(i2c_8xx_module_init);
module_exit(i2c_8xx_module_exit);

MODULE_AUTHOR("Felix Domke <tmbinc@gmx.net>, Gillem <htoa@gmx.net>");
MODULE_DESCRIPTION("I2C-Bus MPC8xx Intgrated I2C");
MODULE_LICENSE("GPL");
MODULE_PARM(debug,"i");
MODULE_PARM_DESC(debug, "debug level - 0 off; 1 on");
