/*
 st7735_text_stable.c
 ST7735 文本显示（稳定版）
 - 自动换行
 - 默认旋转 90° 顺时针（软件旋转）
 - 使用最小 5x7 字体（字符格 6x8），尽量显示更多内容
 - 修复：只在初始化时设置 MADCTL（默认 0xC8），避免重复设置导致坐标偏移/左侧留白
 - 先在内存组装好整屏物理帧，再以分块方式通过 SPI 发送，避免闪烁
 - 支持可选参数覆盖 MADCTL 与字节交换（如果你的模块需要）

 硬件接线（按你给的）:
  SCLK -> GPIO11 (SPI0 SCLK)
  MOSI -> GPIO10 (SPI0 MOSI)
  CS   -> GPIO8  (SPI0 CE0)
  DC   -> GPIO25
  RST  -> GPIO27
  BL   -> GPIO24

 目标：Raspberry Pi 1B (BCM2835), libbcm2835

 编译:
   sudo apt-get install libbcm2835-dev
   gcc -O2 -o st7735_text_stable st7735_text_stable.c -lbcm2835

 运行示例:
   sudo ./st7735_text_stable file.txt        # 从文件读取并显示（程序退出前内容保持）
   cat file.txt | sudo ./st7735_text_stable  # 从 stdin 读取并显示
   sudo ./st7735_text_stable -l 1000 file.txt  # 每 1000ms 重绘一次（用于动态内容）

 默认 MADCTL = 0xC8 （你测试时 0x00/0x08/0xC0/0xC8 都显示正常；选 0xC8 作为默认）
 如果你确认另一个 MADCTL 更稳定，请用 -m 0x?? 指定。
 如果你的模块需要字节对换（高低字节交换），用 --swap 开启。
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <stdarg.h>

#include <bcm2835.h>

/* ST7735 commands */
#define CMD_SWRESET 0x01
#define CMD_SLPOUT  0x11
#define CMD_DISPON  0x29
#define CMD_CASET   0x2A
#define CMD_RASET   0x2B
#define CMD_RAMWR   0x2C
#define CMD_MADCTL  0x36
#define CMD_COLMOD  0x3A
#define CMD_NORON   0x13

/* Physical display size */
#define ST_PHY_W 128
#define ST_PHY_H 160

/* Logical buffer (we render text into this and rotate 90° CW) */
#define LOG_W (ST_PHY_H) /* 160 */
#define LOG_H (ST_PHY_W) /* 128 */

/* Character cell */
#define CHAR_W 6
#define CHAR_H 8

/* BCM pin macros (bcm2835 mapping) */
#define PIN_DC   RPI_V2_GPIO_P1_22  /* GPIO25 */
#define PIN_RST  RPI_V2_GPIO_P1_13  /* GPIO27 */
#define PIN_BL   RPI_V2_GPIO_P1_18  /* GPIO24 */

/* Default MADCTL and SPI settings */
static uint8_t default_madctl = 0xC8;
static int default_swap = 0; /* 0 = no swap, 1 = swap high/low bytes when sending pixels */

static void msleep(unsigned int ms) {
    struct timespec ts = { ms/1000, (ms%1000) * 1000000UL };
    nanosleep(&ts, NULL);
}

/* SPI helpers */
static void spi_cmd_buf(const uint8_t *b, size_t n) {
    bcm2835_gpio_write(PIN_DC, LOW);
    bcm2835_spi_writenb((char*)b, n);
}
static void spi_data_buf(const uint8_t *b, size_t n) {
    bcm2835_gpio_write(PIN_DC, HIGH);
    bcm2835_spi_writenb((char*)b, n);
}
static void spi_cmd1(uint8_t c) { spi_cmd_buf(&c, 1); }
static void spi_data1(uint8_t d) { spi_data_buf(&d, 1); }

/* Reset and init (set MADCTL once here) */
static void st_reset(void) {
    bcm2835_gpio_write(PIN_RST, HIGH);
    msleep(5);
    bcm2835_gpio_write(PIN_RST, LOW);
    msleep(10);
    bcm2835_gpio_write(PIN_RST, HIGH);
    msleep(120);
}

static void st_init(uint8_t madctl_val) {
    st_reset();
    spi_cmd1(CMD_SWRESET); msleep(150);
    spi_cmd1(CMD_SLPOUT);  msleep(120);

    spi_cmd1(CMD_COLMOD);
    spi_data1(0x05); /* 16-bit/pixel (RGB565) */
    msleep(10);

    /* set MADCTL once (do not change later) */
    spi_cmd1(CMD_MADCTL);
    spi_data1(madctl_val);
    msleep(10);

    spi_cmd1(CMD_NORON); msleep(10);
    spi_cmd1(CMD_DISPON); msleep(100);
}

/* Set full window (use 2-byte hi/lo for coords) */
static void st_set_full_window(void) {
    uint8_t buf[4];
    spi_cmd1(CMD_CASET);
    buf[0] = (0 >> 8) & 0xFF; buf[1] = (0) & 0xFF;
    buf[2] = (ST_PHY_W - 1) >> 8 & 0xFF; buf[3] = (ST_PHY_W - 1) & 0xFF;
    spi_data_buf(buf, 4);

    spi_cmd1(CMD_RASET);
    buf[0] = (0 >> 8) & 0xFF; buf[1] = (0) & 0xFF;
    buf[2] = (ST_PHY_H - 1) >> 8 & 0xFF; buf[3] = (ST_PHY_H - 1) & 0xFF;
    spi_data_buf(buf, 4);

    spi_cmd1(CMD_RAMWR);
}

/* convert rgb8 to rgb565 */
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* 5x7 font (ASCII 32..127) */
static const uint8_t font5x7[96][5] = {
{0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},
{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},{0x23,0x13,0x08,0x64,0x62},
{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},
{0x00,0x41,0x22,0x1C,0x00},{0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},
{0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
{0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},
{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},{0x18,0x14,0x12,0x7F,0x10},
{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},
{0x36,0x49,0x49,0x49,0x36},{0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},
{0x00,0x56,0x36,0x00,0x00},{0x08,0x14,0x22,0x41,0x00},{0x14,0x14,0x14,0x14,0x14},
{0x00,0x41,0x22,0x14,0x08},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},
{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},{0x3E,0x41,0x41,0x41,0x22},
{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
{0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},
{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
{0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},
{0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},
{0x1F,0x20,0x40,0x20,0x1F},{0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
{0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},
{0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},{0x20,0x54,0x54,0x54,0x78},
{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},
{0x38,0x54,0x54,0x54,0x18},{0x08,0x7E,0x09,0x01,0x02},{0x08,0x14,0x54,0x54,0x3C},
{0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
{0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},
{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},{0x7C,0x14,0x14,0x14,0x08},
{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},
{0x04,0x3F,0x44,0x40,0x20},{0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},
{0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
{0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},
{0x00,0x41,0x36,0x08,0x00},{0x10,0x08,0x08,0x10,0x08},{0x00,0x00,0x00,0x00,0x00}
};

/* write pixel into logical buffer */
static inline void buf_putpix(uint16_t *buf, int x, int y, uint16_t color) {
    if (x < 0 || x >= LOG_W || y < 0 || y >= LOG_H) return;
    buf[y * LOG_W + x] = color;
}

/* draw char into logical buffer */
static void draw_char(uint16_t *buf, int cx, int cy, char ch, uint16_t fg, uint16_t bg) {
    if (ch < 32 || ch > 127) ch = '?';
    const uint8_t *g = font5x7[(int)ch - 32];
    for (int col = 0; col < 5; col++) {
        uint8_t bits = g[col];
        for (int row = 0; row < 7; row++) {
            int px = cx + col;
            int py = cy + row;
            if (bits & (1 << row)) buf_putpix(buf, px, py, fg);
            else buf_putpix(buf, px, py, bg);
        }
    }
    /* spacing column */
    for (int row = 0; row < 7; row++) buf_putpix(buf, cx + 5, cy + row, bg);
    /* bottom spacing row */
    for (int col = 0; col < CHAR_W; col++) buf_putpix(buf, cx + col, cy + 7, bg);
}

/* render text with wrapping into logical buffer */
static void render_text(uint16_t *buf, const char *text, uint16_t fg, uint16_t bg) {
    size_t total = (size_t)LOG_W * (size_t)LOG_H;
    for (size_t i = 0; i < total; i++) buf[i] = bg;

    int cols = LOG_W / CHAR_W;
    int rows = LOG_H / CHAR_H;
    int col = 0, row = 0;
    const char *p = text;
    while (*p && row < rows) {
        char c = *p++;
        if (c == '\r') continue;
        if (c == '\n') { col = 0; row++; continue; }
        if (col >= cols) { col = 0; row++; if (row >= rows) break; }
        int x = col * CHAR_W;
        int y = row * CHAR_H;
        draw_char(buf, x, y, c, fg, bg);
        col++;
    }
}

/* Build physical buffer (big-endian RGB565 bytes) from logical buffer with 90° CW rotation.
   For physical pixel (xP,yP):
     src_x = LOG_W - 1 - yP
     src_y = xP
*/
static void build_physical_from_log(uint8_t *outbuf, uint16_t *logbuf, int swap) {
    const int W = ST_PHY_W;  /* 128 */
    const int H = ST_PHY_H;  /* 160 */
    for (int yP = 0; yP < H; yP++) {
        for (int xP = 0; xP < W; xP++) {
            int src_x = LOG_W - 1 - yP; /* 159..0 */
            int src_y = xP;             /* 0..127 */
            uint16_t pix = logbuf[src_y * LOG_W + src_x];
            size_t idx = (yP * W + xP) * 2;
            uint8_t hi = (pix >> 8) & 0xFF;
            uint8_t lo = pix & 0xFF;
            if (swap) { outbuf[idx] = lo; outbuf[idx+1] = hi; }
            else      { outbuf[idx] = hi; outbuf[idx+1] = lo; }
        }
    }
}

/* send physical buffer to display in chunks (DC must be set inside) */
static int send_physical_buffer(uint8_t *physbuf, size_t len) {
    st_set_full_window();
    const size_t CHUNK = 4096;
    size_t sent = 0;
    while (sent < len) {
        size_t take = (len - sent) < CHUNK ? (len - sent) : CHUNK;
        spi_data_buf(physbuf + sent, take);
        sent += take;
    }
    return 0;
}

/* read all from FILE* */
static char *read_all(FILE *f) {
    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) return NULL;
    while (1) {
        size_t r = fread(buf + len, 1, cap - len, f);
        len += r;
        if (feof(f)) break;
        if (len == cap) {
            cap *= 2;
            char *n = realloc(buf, cap);
            if (!n) { free(buf); return NULL; }
            buf = n;
        }
    }
    if (len == cap) {
        char *n = realloc(buf, len + 1);
        if (!n) { free(buf); return NULL; }
        buf = n;
    }
    buf[len] = '\0';
    return buf;
}

int main(int argc, char **argv) {
    const char *infile = NULL;
    int loop_ms = 0;
    uint8_t use_madctl = (uint8_t)default_madctl;
    int swap_bytes = default_swap;

    /* very small CLI */
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-l") && i+1 < argc) { loop_ms = atoi(argv[++i]); }
        else if (!strcmp(argv[i], "--swap")) { swap_bytes = 1; }
        else if (!strcmp(argv[i], "-m") && i+1 < argc) {
            unsigned int mv = 0; sscanf(argv[++i], "0x%X", &mv);
            use_madctl = (uint8_t)mv;
        }
        else if (!strcmp(argv[i], "-v")) { /* ignored for now */ }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            return 1;
        } else infile = argv[i];
    }

    char *text = NULL;
    if (infile) {
        FILE *f = fopen(infile, "rb");
        if (!f) { fprintf(stderr, "Open %s: %s\n", infile, strerror(errno)); return 1; }
        text = read_all(f);
        fclose(f);
    } else {
        text = read_all(stdin);
    }
    if (!text) text = strdup(" ");

    /* init bcm2835 & SPI */
    if (!bcm2835_init()) { fprintf(stderr, "bcm2835_init failed (run as root?)\n"); free(text); return 1; }
    if (!bcm2835_spi_begin()) { fprintf(stderr, "bcm2835_spi_begin failed\n"); bcm2835_close(); free(text); return 1; }

    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64); /* ~4 MHz */
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);

    bcm2835_gpio_fsel(PIN_DC, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_RST, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(PIN_BL, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_write(PIN_BL, HIGH);

    /* initialize display once with chosen MADCTL */
    st_init(use_madctl);

    /* allocate logical and physical buffers */
    uint16_t *logbuf = calloc((size_t)LOG_W * LOG_H, sizeof(uint16_t));
    if (!logbuf) { fprintf(stderr, "OOM logbuf\n"); bcm2835_spi_end(); bcm2835_close(); free(text); return 1; }
    size_t phys_len = (size_t)ST_PHY_W * ST_PHY_H * 2;
    uint8_t *physbuf = malloc(phys_len);
    if (!physbuf) { fprintf(stderr, "OOM physbuf\n"); free(logbuf); bcm2835_spi_end(); bcm2835_close(); free(text); return 1; }

    uint16_t fg = rgb565(255,255,255);
    uint16_t bg = rgb565(0,0,0);

    /* render once (or loop if requested) */
    do {
        render_text(logbuf, text, fg, bg);
        build_physical_from_log(physbuf, logbuf, swap_bytes);
        send_physical_buffer(physbuf, phys_len);
        if (loop_ms > 0) msleep(loop_ms);
    } while (loop_ms > 0);

    /* cleanup */
    free(physbuf);
    free(logbuf);
    free(text);
    bcm2835_spi_end();
    bcm2835_close();
    printf("Done. Display should show text (rotated 90° CW). MADCTL=0x%02X swap=%d\n", use_madctl, swap_bytes);
    return 0;
}