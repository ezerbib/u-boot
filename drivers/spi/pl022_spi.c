/*
 * (C) Copyright 2012
 * Armando Visconti, ST Microelectronics, armando.visconti at st.com.
 *
 * Driver for ARM PL022 SPI Controller. Based on atmel_spi.c
 * by Atmel Corporation.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <common.h>
#include <malloc.h>
#include <spi.h>
#include <asm/io.h>

//EZ - Special implementation for LPC178x
#include <asm/arch/lpc18xx_scu.h>
#include <asm/arch/lpc18xx_gpio.h>
#include <asm/arch/lpc178x_gpio.h>
//#include <asm/arch/hardware.h>

#define CONFIG_LPC_CS_GPIO {0, 5}

/* SPI pins configuration */
static const struct lpc18xx_pin_config
lpc_spi_pins_config[] = {
	{{0, 5}, LPC178X_GPIO_CONFIG_D(0, LPC178X_NO_PULLUP, 0, 0, 0, 0)},
	{{0, 7}, LPC178X_GPIO_CONFIG_W(2, LPC178X_NO_PULLUP, 0, 0, 1, 0, 0)},
	{{0, 8}, LPC178X_GPIO_CONFIG_W(2, LPC178X_NO_PULLUP, 0, 0, 1, 0, 0)},
	{{0, 9}, LPC178X_GPIO_CONFIG_W(2, LPC178X_NO_PULLUP, 0, 0, 1, 0, 0)}
};

static const struct lpc18xx_iomux_dsc lpc_cs_gpio = CONFIG_LPC_CS_GPIO;


#define CONFIG_SYS_SPI_BASE 0x40030000
#define CONFIG_SYS_SPI_CLK  120000000

/* SSP registers mapping */
struct pl022 {
       u32     ssp_cr0;        /* 0x000 */
       u32     ssp_cr1;        /* 0x004 */
       u32     ssp_dr;         /* 0x008 */
       u32     ssp_sr;         /* 0x00c */
       u32     ssp_cpsr;       /* 0x010 */
       u32     ssp_imsc;       /* 0x014 */
       u32     ssp_ris;        /* 0x018 */
       u32     ssp_mis;        /* 0x01c */
       u32     ssp_icr;        /* 0x020 */
       u32     ssp_dmacr;      /* 0x024 */
       u8      reserved_1[0x080 - 0x028];
       u32     ssp_itcr;       /* 0x080 */
       u32     ssp_itip;       /* 0x084 */
       u32     ssp_itop;       /* 0x088 */
       u32     ssp_tdr;        /* 0x08c */
       u8      reserved_2[0xFE0 - 0x090];
       u32     ssp_pid0;       /* 0xfe0 */
       u32     ssp_pid1;       /* 0xfe4 */
       u32     ssp_pid2;       /* 0xfe8 */
       u32     ssp_pid3;       /* 0xfec */
       u32     ssp_cid0;       /* 0xff0 */
       u32     ssp_cid1;       /* 0xff4 */
       u32     ssp_cid2;       /* 0xff8 */
       u32     ssp_cid3;       /* 0xffc */
};

/* SSP Control Register 0  - SSP_CR0 */
#define SSP_CR0_SPO            (0x1 << 6)
#define SSP_CR0_SPH            (0x1 << 7)
#define SSP_CR0_8BIT_MODE      (0x07)
#define SSP_SCR_MAX            (0xFF)
#define SSP_SCR_SHFT           8

/* SSP Control Register 0  - SSP_CR1 */
#define SSP_CR1_MASK_SSE       (0x1 << 1)

#define SSP_CPSR_MAX           (0xFE)

/* SSP Status Register - SSP_SR */
#define SSP_SR_MASK_TFE                (0x1 << 0) /* Transmit FIFO empty */
#define SSP_SR_MASK_TNF                (0x1 << 1) /* Transmit FIFO not full */
#define SSP_SR_MASK_RNE                (0x1 << 2) /* Receive FIFO not empty */
#define SSP_SR_MASK_RFF                (0x1 << 3) /* Receive FIFO full */
#define SSP_SR_MASK_BSY                (0x1 << 4) /* Busy Flag */

struct pl022_spi_slave {
       struct spi_slave slave;
       void *regs;
       unsigned int freq;
};

static inline struct pl022_spi_slave *to_pl022_spi(struct spi_slave *slave)
{
       return container_of(slave, struct pl022_spi_slave, slave);
}

#define __weak

/*
 * Following three functions should be provided by the
 * board support package.
 */
int __weak spi_cs_is_valid(unsigned int bus, unsigned int cs)
{
       return 1;
}

void __weak spi_cs_activate(struct spi_slave *slave)
{
	lpc_gpio_clear(lpc_cs_gpio);
       /* do nothing */
}

void __weak spi_cs_deactivate(struct spi_slave *slave)
{
	lpc_gpio_set(lpc_cs_gpio);
       /* do nothing */
}

int b_spi_init=0;
void spi_init(void)
{
	/* Configure PINs */
	lpc178x_gpio_config_table(lpc_spi_pins_config,ARRAY_SIZE(lpc_spi_pins_config));
	//lpc18xx_pin_config_table(lpc_spi_pins_config,
	//			 ARRAY_SIZE(lpc_spi_pins_config));

	/* Configure Chip Select GPIO */
	lpc_gpio_set(lpc_cs_gpio);
	lpc_gpio_dir(lpc_cs_gpio, 1);
	lpc_gpio_set(lpc_cs_gpio);
	b_spi_init=1;
}

/*
 * ARM PL022 exists in different 'flavors'.
 * This drivers currently support the standard variant (0x00041022), that has a
 * 16bit wide and 8 locations deep TX/RX FIFO.
 */
static int pl022_is_supported(struct pl022_spi_slave *ps)
{
       struct pl022 *pl022 = (struct pl022 *)ps->regs;

       /* PL022 version is 0x00041022 */
       if ((readl(&pl022->ssp_pid0) == 0x22) &&
                       (readl(&pl022->ssp_pid1) == 0x10) &&
                       ((readl(&pl022->ssp_pid2) & 0xf) == 0x04) &&
                       (readl(&pl022->ssp_pid3) == 0x00))
       {
    	       //printf("pl022 is well supported!\n");
               return 1;
       }
       printf("pl022 is not supported!\n");
       return 0;
}

struct pl022_spi_slave *spi_alloc_slave(unsigned int bus, unsigned int cs)
{
	struct pl022_spi_slave *s;
	s = malloc(sizeof(struct pl022_spi_slave));
	if (!s) {
		return NULL;
	}
	s->slave.bus=bus;
	s->slave.cs=cs;
	return s;
}

struct spi_slave *spi_setup_slave(unsigned int bus, unsigned int cs,
                       unsigned int max_hz, unsigned int mode)
{
       struct pl022_spi_slave *ps;
       struct pl022 *pl022;
       u16 scr = 1, prescaler, cr0 = 0, cpsr = 0;

       printf("spi_setup_slave(bus=%d, cs=%d,hz=%d,mode=%d)\n",bus,cs,max_hz,mode);

       if (!b_spi_init)
    	   spi_init();

       if (!spi_cs_is_valid(bus, cs))
               return NULL;

       ps = spi_alloc_slave(bus, cs);
       if (!ps)
               return NULL;

       ps->freq = max_hz;

       switch (bus) {
       case 0:
               ps->regs = (void *)CONFIG_SYS_SPI_BASE;
               break;
#ifdef CONFIG_SYS_SPI_BASE1
       case 1:
               ps->regs = (void *)CONFIG_SYS_SPI_BASE1;
               break;
#endif
#ifdef CONFIG_SYS_SPI_BASE2
       case 2:
               ps->regs = (void *)CONFIG_SYS_SPI_BASE2;
               break;
#endif
#ifdef CONFIG_SYS_SPI_BASE3
       case 3:
               ps->regs = (void *)CONFIG_SYS_SPI_BASE3;
               break;
#endif
       default:
               free(ps);
               return NULL;
       }

       pl022 = (struct pl022 *)ps->regs;

       /* Check the PL022 version */
       if (!pl022_is_supported(ps)) {
               free(ps);
               return NULL;
       }

       /* Set requested polarity and 8bit mode */
       cr0 = SSP_CR0_8BIT_MODE;
       cr0 |= (mode & SPI_CPHA) ? SSP_CR0_SPH : 0;
       cr0 |= (mode & SPI_CPOL) ? SSP_CR0_SPO : 0;

       writel(cr0, &pl022->ssp_cr0);

       /* Program the SSPClk frequency */
       prescaler = CONFIG_SYS_SPI_CLK / ps->freq;
       printf("prescaler=(%d/%d) => %d\n",CONFIG_SYS_SPI_CLK , ps->freq,prescaler);
       if (prescaler <= 0xFF) {
               cpsr = prescaler;
       } else {
               for (scr = 1; scr <= SSP_SCR_MAX; scr++) {
                       if (!(prescaler % scr)) {
                               cpsr = prescaler / scr;
                               if (cpsr <= SSP_CPSR_MAX)
                                       break;
                       }
               }

               if (scr > SSP_SCR_MAX) {
                       scr = SSP_SCR_MAX;
                       cpsr = prescaler / scr;
                       cpsr &= SSP_CPSR_MAX;
               }
       }

       if (cpsr & 0x1)
               cpsr++;
       printf("SPI CPSR=%d scr=%d\n",cpsr,scr);
       writel(cpsr, &pl022->ssp_cpsr);
       cr0 = readl(&pl022->ssp_cr0);
       writel(cr0 | (scr - 1) << SSP_SCR_SHFT, &pl022->ssp_cr0);
       cr0 = readl(&pl022->ssp_cr0);
       printf("SPI CR0=0x%X \n",cr0);
       return &ps->slave;
}

void spi_free_slave(struct spi_slave *slave)
{
       struct pl022_spi_slave *ps = to_pl022_spi(slave);

       free(ps);
}

int spi_claim_bus(struct spi_slave *slave)
{
       struct pl022_spi_slave *ps = to_pl022_spi(slave);
       struct pl022 *pl022 = (struct pl022 *)ps->regs;

       //printf("pl022 =0x%p\n",pl022);
       /* Enable the SPI hardware */
       setbits_le32(&pl022->ssp_cr1, SSP_CR1_MASK_SSE);

       return 0;
}

void spi_release_bus(struct spi_slave *slave)
{
       struct pl022_spi_slave *ps = to_pl022_spi(slave);
       struct pl022 *pl022 = (struct pl022 *)ps->regs;

       /* Disable the SPI hardware */
       writel(0x0, &pl022->ssp_cr1);
}

int spi_xfer(struct spi_slave *slave, unsigned int bitlen,
               const void *dout, void *din, unsigned long flags)
{
       struct pl022_spi_slave *ps = to_pl022_spi(slave);
       struct pl022 *pl022 = (struct pl022 *)ps->regs;
       u32             len_tx = 0, len_rx = 0, len;
       u32             ret = 0;
       const u8        *txp = dout;
       u8              *rxp = din, value;

       if (bitlen == 0)
               /* Finish any previously submitted transfers */
               goto out;

       /*
        * TODO: The controller can do non-multiple-of-8 bit
        * transfers, but this driver currently doesn't support it.
        *
        * It's also not clear how such transfers are supposed to be
        * represented as a stream of bytes...this is a limitation of
        * the current SPI interface.
        */
       if (bitlen % 8) {
               ret = -1;

               /* Errors always terminate an ongoing transfer */
               flags |= SPI_XFER_END;
               goto out;
       }

       len = bitlen / 8;

       if (flags & SPI_XFER_BEGIN)
               spi_cs_activate(slave);

       while (len_tx < len) {
               if (readl(&pl022->ssp_sr) & SSP_SR_MASK_TNF) {
                       value = (txp != NULL) ? *txp++ : 0;
                       //printf("spi write=%02X\n",value);
                       writel(value, &pl022->ssp_dr);
                       len_tx++;
               }

               if (readl(&pl022->ssp_sr) & SSP_SR_MASK_RNE) {
                       value = readl(&pl022->ssp_dr);
                       //printf("spi write&read=%02X\n",value);
                       if (rxp)
                               *rxp++ = value;
                       len_rx++;
               }
       }

       while (len_rx < len_tx) {
               if (readl(&pl022->ssp_sr) & SSP_SR_MASK_RNE) {
                       value = readl(&pl022->ssp_dr);
                       //printf("spi read=%02X\n",value);
                       if (rxp)
                               *rxp++ = value;
                       len_rx++;
               }
       }

out:
       if (flags & SPI_XFER_END)
               spi_cs_deactivate(slave);

       return ret;
}
