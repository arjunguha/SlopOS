#include "floppy.h"
#include "ports.h"

#define FLOPPY_DOR 0x3F2
#define FLOPPY_MSR 0x3F4
#define FLOPPY_FIFO 0x3F5
#define FLOPPY_CCR 0x3F7

#define FLOPPY_SECTORS_PER_TRACK 18
#define FLOPPY_HEADS 2

#define DMA_MASK_REG 0x0A
#define DMA_MODE_REG 0x0B
#define DMA_CLEAR_FF_REG 0x0C
#define DMA_CH2_ADDR 0x04
#define DMA_CH2_COUNT 0x05
#define DMA_CH2_PAGE 0x81

static unsigned char dma_buf[512] __attribute__((aligned(16)));
static int floppy_ready = 0;

static void floppy_wait_ready(void) {
    while ((inb(FLOPPY_MSR) & 0x80) == 0) {
    }
}

static void floppy_send_byte(unsigned char val) {
    while ((inb(FLOPPY_MSR) & 0xC0) != 0x80) {
    }
    outb(FLOPPY_FIFO, val);
}

static unsigned char floppy_read_byte(void) {
    while ((inb(FLOPPY_MSR) & 0xD0) != 0xD0) {
    }
    return inb(FLOPPY_FIFO);
}

static void floppy_motor_on(void) {
    outb(FLOPPY_DOR, 0x1C);
}

static void floppy_set_data_rate(void) {
    outb(FLOPPY_CCR, 0x00);
}

static void floppy_init(void) {
    if (floppy_ready) {
        return;
    }
    outb(FLOPPY_DOR, 0x00);
    outb(FLOPPY_DOR, 0x1C);
    floppy_set_data_rate();
    floppy_send_byte(0x03);
    floppy_send_byte(0xDF);
    floppy_send_byte(0x02);
    floppy_ready = 1;
}

static void floppy_lba_to_chs(unsigned int lba, unsigned char *c, unsigned char *h, unsigned char *s) {
    unsigned int tmp = lba;
    *s = (unsigned char)(tmp % FLOPPY_SECTORS_PER_TRACK + 1);
    tmp /= FLOPPY_SECTORS_PER_TRACK;
    *h = (unsigned char)(tmp % FLOPPY_HEADS);
    *c = (unsigned char)(tmp / FLOPPY_HEADS);
}

static int dma_setup_write(const unsigned char *data) {
    unsigned int addr = (unsigned int)dma_buf;
    if ((addr & 0xFFFF) + 512 > 0x10000) {
        return -1;
    }
    for (int i = 0; i < 512; i++) {
        dma_buf[i] = data[i];
    }
    outb(DMA_MASK_REG, 0x06);
    outb(DMA_CLEAR_FF_REG, 0x00);
    outb(DMA_CH2_ADDR, addr & 0xFF);
    outb(DMA_CH2_ADDR, (addr >> 8) & 0xFF);
    outb(DMA_CH2_PAGE, (addr >> 16) & 0xFF);
    outb(DMA_CLEAR_FF_REG, 0x00);
    unsigned short count = 512 - 1;
    outb(DMA_CH2_COUNT, count & 0xFF);
    outb(DMA_CH2_COUNT, (count >> 8) & 0xFF);
    outb(DMA_MODE_REG, 0x4A);
    outb(DMA_MASK_REG, 0x02);
    return 0;
}

int floppy_write_sector_lba(unsigned int lba, const unsigned char *data) {
    unsigned char c;
    unsigned char h;
    unsigned char s;
    floppy_lba_to_chs(lba, &c, &h, &s);

    floppy_init();
    floppy_motor_on();
    floppy_wait_ready();
    if (dma_setup_write(data) != 0) {
        return -1;
    }

    floppy_send_byte(0x45);
    floppy_send_byte((unsigned char)(h << 2));
    floppy_send_byte(c);
    floppy_send_byte(h);
    floppy_send_byte(s);
    floppy_send_byte(2);
    floppy_send_byte(FLOPPY_SECTORS_PER_TRACK);
    floppy_send_byte(0x1B);
    floppy_send_byte(0xFF);

    for (int i = 0; i < 7; i++) {
        (void)floppy_read_byte();
    }

    return 0;
}
