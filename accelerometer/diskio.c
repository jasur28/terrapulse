/* FatFs diskio.c — SPI1 SD card, register-only
   CS=PA4, SCK=PA5, MISO=PA6, MOSI=PA7 (APB2 = 72 MHz) */

#include "ff.h"       /* defines BYTE, WORD, DWORD, LBA_t etc. */
#include "diskio.h"
#include "stm32f10x.h"

/* ---- CS pin via BSRR/BRR ---- */
#define CS_LOW()   (GPIOA->BRR  = (1UL<<4))
#define CS_HIGH()  (GPIOA->BSRR = (1UL<<4))

/* ---- SPI byte exchange ---- */
static uint8_t spi_xchg(uint8_t data) {
    while (!(SPI1->SR & (1UL<<1))) {}  /* wait TXE */
    SPI1->DR = data;
    while (!(SPI1->SR & (1UL<<0))) {}  /* wait RXNE */
    return (uint8_t)SPI1->DR;
}

static void spi_recv_block(uint8_t *buf, uint32_t len) {
    while (len--) *buf++ = spi_xchg(0xFF);
}

static void spi_send_block(const uint8_t *buf, uint32_t len) {
    while (len--) spi_xchg(*buf++);
}

/* ---- SPI speed control ---- */
/* CR1[5:3] = BR[2:0].  PCLK2=72 MHz.
   111 = /256 → 281 kHz (init)
   001 = /4   → 18 MHz  (data) */
static void spi_speed_low(void) {
    SPI1->CR1 = (SPI1->CR1 & ~(7UL<<3)) | (7UL<<3);
}
static void spi_speed_high(void) {
    SPI1->CR1 = (SPI1->CR1 & ~(7UL<<3)) | (1UL<<3);
}

/* ---- SD card commands ---- */
#define CMD0    0
#define CMD8    8
#define CMD17   17
#define CMD24   24
#define CMD55   55
#define ACMD41  41
#define CMD58   58

static volatile uint8_t  g_card_type;
static volatile DSTATUS  g_status = STA_NOINIT;

/* Wait until card releases MISO (not busy). Must be called with CS low. */
static int sd_wait_ready(void) {
    uint32_t t = 500000;
    while (spi_xchg(0xFF) != 0xFF) {
        if (--t == 0) return -1; /* timeout — card hung */
    }
    return 0;
}

static uint8_t sd_send_cmd(uint8_t cmd, uint32_t arg) {
    uint8_t  crc, r;
    uint32_t n;

    if (cmd == ACMD41) {
        sd_send_cmd(CMD55, 0);
        cmd = ACMD41;
    }

    CS_HIGH(); spi_xchg(0xFF);  /* deselect + 1 dummy byte */
    CS_LOW();  spi_xchg(0xFF);  /* select   + 1 dummy byte */

    /* Wait for card to be ready before issuing next command.
       Without this, sending CMD while card is still writing FAT
       corrupts the filesystem. */
    if (sd_wait_ready() != 0) {
        CS_HIGH(); spi_xchg(0xFF);
        return 0xFF; /* error */
    }

    spi_xchg(cmd | 0x40u);
    spi_xchg((uint8_t)(arg >> 24));
    spi_xchg((uint8_t)(arg >> 16));
    spi_xchg((uint8_t)(arg >> 8));
    spi_xchg((uint8_t) arg);
    crc = (cmd == CMD0) ? 0x95u : (cmd == CMD8) ? 0x87u : 0x01u;
    spi_xchg(crc);

    /* Wait for response R1 (up to 10 bytes) */
    n = 10;
    do { r = spi_xchg(0xFF); } while ((r & 0x80u) && --n);
    return r;
}

/* ---- diskio interface ---- */

DSTATUS disk_initialize(BYTE pdrv) {
    uint8_t  n, ty, resp;
    uint32_t retry;

    if (pdrv) return STA_NOINIT;

    /* Enable GPIOA (bit2) and SPI1 (bit12) clocks on APB2 */
    RCC->APB2ENR |= (1UL<<2) | (1UL<<12);

    /* PA4 = 50 MHz PP output (CS)
       PA5 = AF PP 50 MHz (SCK)
       PA6 = floating input (MISO)
       PA7 = AF PP 50 MHz (MOSI)
       CRL bits: PA4[19:16]=0x3, PA5[23:20]=0xB, PA6[27:24]=0x4, PA7[31:28]=0xB */
    GPIOA->CRL = (GPIOA->CRL & 0x0000FFFFul)
               | (0x3ul<<16) | (0xBul<<20) | (0x4ul<<24) | (0xBul<<28);

    CS_HIGH();

    /* SPI1: master, CPOL=0, CPHA=0, 8-bit, SW NSS, BR=/256 */
    SPI1->CR1 = (1UL<<9)  /* SSM */
              | (1UL<<8)  /* SSI */
              | (7UL<<3)  /* BR=111 → /256 */
              | (1UL<<2); /* MSTR */
    SPI1->CR1 |= (1UL<<6); /* SPE */

    /* >74 dummy clocks with CS high */
    CS_HIGH();
    for (n = 0; n < 10; n++) spi_xchg(0xFF);

    ty = 0;
    if (sd_send_cmd(CMD0, 0) == 0x01u) {
        /* Check SD version */
        if (sd_send_cmd(CMD8, 0x1AAu) == 0x01u) {
            uint8_t buf[4];
            spi_recv_block(buf, 4);
            if (buf[2] == 0x01u && buf[3] == 0xAAu) {
                /* SDv2 */
                retry = 2000;
                do { resp = sd_send_cmd(ACMD41, 0x40000000ul); } while (resp && --retry);
                if (!resp && sd_send_cmd(CMD58, 0) == 0) {
                    uint8_t ocr[4];
                    spi_recv_block(ocr, 4);
                    ty = (ocr[0] & 0x40u) ? 3u : 2u; /* SDHC=3, SDv2=2 */
                }
            }
        } else {
            /* SDv1 */
            retry = 2000;
            do { resp = sd_send_cmd(ACMD41, 0); } while (resp && --retry);
            if (!resp) ty = 1u;
        }
    }

    g_card_type = ty;
    CS_HIGH(); spi_xchg(0xFF);

    if (ty) {
        spi_speed_high();
        g_status = 0;
    }
    return g_status;
}

DSTATUS disk_status(BYTE pdrv) {
    return pdrv ? STA_NOINIT : g_status;
}

void disk_force_reinit(void) {
    g_status = STA_NOINIT;
    g_card_type = 0;
}

DRESULT disk_read(BYTE pdrv, BYTE *buf, LBA_t sector, UINT count) {
    if (pdrv || !count || (g_status & STA_NOINIT)) return RES_NOTRDY;
    if (g_card_type != 3) sector <<= 9; /* byte address for non-SDHC */

    if (count == 1) {
        if (sd_send_cmd(CMD17, (uint32_t)sector) == 0) {
            uint32_t retry = 40000;
            uint8_t  tok;
            do { tok = spi_xchg(0xFF); } while (tok == 0xFF && --retry);
            if (tok == 0xFEu) {
                spi_recv_block(buf, 512);
                spi_xchg(0xFF); spi_xchg(0xFF); /* CRC */
                CS_HIGH(); spi_xchg(0xFF);
                return RES_OK;
            }
        }
    }
    CS_HIGH(); spi_xchg(0xFF);
    return RES_ERROR;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buf, LBA_t sector, UINT count) {
    if (pdrv || !count || (g_status & STA_NOINIT)) return RES_NOTRDY;
    if (g_card_type != 3) sector <<= 9;

    if (count == 1) {
        if (sd_send_cmd(CMD24, (uint32_t)sector) == 0) {
            uint32_t retry;
            uint8_t  resp;
            spi_xchg(0xFF);
            spi_xchg(0xFEu);            /* data token */
            spi_send_block(buf, 512);
            spi_xchg(0xFF); spi_xchg(0xFF); /* CRC */
            resp = spi_xchg(0xFF) & 0x1Fu;
            spi_xchg(0xFF); /* one extra byte so card can go busy before we poll */
            /* Wait card not busy: poll until card returns 0xFF (idle) */
            retry = 500000;
            while (spi_xchg(0xFF) != 0xFFu && --retry) {}
            CS_HIGH(); spi_xchg(0xFF);
            if (resp == 0x05u) return RES_OK;
        }
    }
    CS_HIGH(); spi_xchg(0xFF);
    return RES_ERROR;
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buf) {
    if (pdrv || (g_status & STA_NOINIT)) return RES_NOTRDY;
    switch (cmd) {
    case CTRL_SYNC:
        CS_LOW();
        { uint32_t r = 500000; while (spi_xchg(0xFF) == 0 && --r) {} }
        CS_HIGH();
        return RES_OK;
    case GET_SECTOR_SIZE:
        *(WORD *)buf = 512;
        return RES_OK;
    case GET_BLOCK_SIZE:
        *(DWORD *)buf = 1;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
