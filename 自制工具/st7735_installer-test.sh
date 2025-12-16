#!/bin/bash

# ST7735显示屏用户空间Framebuffer驱动安装脚本

set -euo pipefail

# 配置参数
INSTALL_DIR="/root/go_run/st7735"
DRIVER_NAME="st7735_fb"
SERVICE_NAME="st7735-fb.service"
CONSOLE_SCRIPT="console-to-st7735.sh"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # 无颜色

# 日志函数
info() {
    echo -e "${GREEN}[INFO] $1${NC}"
}

warning() {
    echo -e "${YELLOW}[WARNING] $1${NC}"
}

error() {
    echo -e "${RED}[ERROR] $1${NC}"
    exit 1
}

# 检查是否以root用户运行
if [ "$(id -u)" -ne 0 ]; then
    error "此脚本需要以root用户运行，请使用sudo或切换到root用户"
fi

# 1. 安装依赖包
info "安装必要依赖..."
apt-get update -y >/dev/null
apt-get install -y libbcm2835-dev build-essential wiringpi pigpio fbset fbterm con2fbmap >/dev/null || {
    error "依赖包安装失败"
}

# 2. 创建安装目录
info "创建安装目录..."
mkdir -p "$INSTALL_DIR" || error "无法创建安装目录 $INSTALL_DIR"
cd "$INSTALL_DIR" || error "无法进入安装目录 $INSTALL_DIR"

# 3. 创建驱动源代码
info "创建驱动源代码..."
cat > "$DRIVER_NAME.c" << 'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <bcm2835.h>

// 屏幕尺寸
#define WIDTH 128
#define HEIGHT 128
#define BPP 16 // RGB565

// 颜色定义 (RGB565)
#define BLACK 0x0000
#define WHITE 0xFFFF
#define RED 0xF800
#define GREEN 0x07E0
#define BLUE 0x001F

// 引脚定义
#define DC_PIN RPI_V2_GPIO_P1_22 // GPIO25
#define RST_PIN RPI_V2_GPIO_P1_13 // GPIO27
#define BL_PIN RPI_V2_GPIO_P1_18 // GPIO24
#define CS_PIN RPI_V2_GPIO_P1_24 // GPIO8

// 全局变量
static int fb_fd = -1;
static struct fb_var_screeninfo vinfo;
static struct fb_fix_screeninfo finfo;
static char *fb_buffer = NULL;
static long screensize = 0;

// ST7735命令
#define ST7735_NOP 0x00
#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT 0x11
#define ST7735_DISPON 0x29
#define ST7735_CASET 0x2A
#define ST7735_RASET 0x2B
#define ST7735_RAMWR 0x2C
#define ST7735_COLMOD 0x3A
#define ST7735_MADCTL 0x36

// 写命令到ST7735
void write_command(uint8_t cmd) {
    bcm2835_gpio_write(DC_PIN, LOW);
    bcm2835_spi_transfer(cmd);
}

// 写数据到ST7735
void write_data(uint8_t data) {
    bcm2835_gpio_write(DC_PIN, HIGH);
    bcm2835_spi_transfer(data);
}

// 写16位数据到ST7735
void write_data16(uint16_t data) {
    bcm2835_gpio_write(DC_PIN, HIGH);
    bcm2835_spi_transfer(data >> 8);
    bcm2835_spi_transfer(data & 0xFF);
}

// 设置显示窗口
void set_address_window(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    write_command(ST7735_CASET);
    write_data(0x00);
    write_data(x0 + 2); // 列起始偏移
    write_data(0x00);
    write_data(x1 + 2); // 列结束
    write_command(ST7735_RASET);
    write_data(0x00);
    write_data(y0 + 3); // 行起始偏移
    write_data(0x00);
    write_data(y1 + 3); // 行结束
    write_command(ST7735_RAMWR);
}

// 初始化ST7735
void st7735_init(void) {
    printf("Initializing ST7735...\n");
    
    // 初始化GPIO
    bcm2835_gpio_fsel(DC_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(RST_PIN, BCM2835_GPIO_FSEL_OUTP);
    bcm2835_gpio_fsel(BL_PIN, BCM2835_GPIO_FSEL_OUTP);
    
    // 初始化SPI
    bcm2835_spi_begin();
    bcm2835_spi_setBitOrder(BCM2835_SPI_BIT_ORDER_MSBFIRST);
    bcm2835_spi_setDataMode(BCM2835_SPI_MODE0);
    bcm2835_spi_setClockDivider(BCM2835_SPI_CLOCK_DIVIDER_64);
    bcm2835_spi_chipSelect(BCM2835_SPI_CS0);
    bcm2835_spi_setChipSelectPolarity(BCM2835_SPI_CS0, LOW);
    
    // 硬件复位
    bcm2835_gpio_write(RST_PIN, HIGH);
    usleep(100000);
    bcm2835_gpio_write(RST_PIN, LOW);
    usleep(100000);
    bcm2835_gpio_write(RST_PIN, HIGH);
    usleep(100000);
    
    // 背光开启
    bcm2835_gpio_write(BL_PIN, HIGH);
    
    // 软件复位
    write_command(ST7735_SWRESET);
    usleep(150000);
    
    // 退出睡眠模式
    write_command(ST7735_SLPOUT);
    usleep(150000);
    
    // 颜色模式：RGB565
    write_command(ST7735_COLMOD);
    write_data(0x05);
    
    // 内存访问控制
    write_command(ST7735_MADCTL);
    write_data(0xC0); // 旋转设置
    
    // 显示开启
    write_command(ST7735_DISPON);
    usleep(100000);
    
    printf("ST7735 initialized.\n");
}

// 将framebuffer内容刷新到屏幕
void flush_framebuffer() {
    set_address_window(0, 0, WIDTH-1, HEIGHT-1);
    uint16_t *fb_ptr = (uint16_t*)fb_buffer;
    
    for (int y = 0; y < HEIGHT; y++) {
        for (int x = 0; x < WIDTH; x++) {
            uint16_t color = fb_ptr[y * WIDTH + x];
            write_data16(color);
        }
    }
}

// 创建虚拟framebuffer
int create_framebuffer() {
    // 创建一个虚拟framebuffer设备
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd < 0) {
        // 尝试创建新的framebuffer
        system("mknod /dev/fb1 c 29 1");
        fb_fd = open("/dev/fb1", O_RDWR | O_CREAT, 0666);
        if (fb_fd < 0) {
            perror("Failed to open framebuffer");
            return -1;
        }
    }
    
    // 获取framebuffer信息
    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
        perror("Error reading fixed information");
        return -2;
    }
    
    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
        perror("Error reading variable information");
        return -3;
    }
    
    // 设置framebuffer参数
    vinfo.xres = WIDTH;
    vinfo.yres = HEIGHT;
    vinfo.xres_virtual = WIDTH;
    vinfo.yres_virtual = HEIGHT;
    vinfo.bits_per_pixel = BPP;
    vinfo.red.offset = 11;
    vinfo.red.length = 5;
    vinfo.green.offset = 5;
    vinfo.green.length = 6;
    vinfo.blue.offset = 0;
    vinfo.blue.length = 5;
    
    if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo)) {
        perror("Error setting variable information");
        return -4;
    }
    
    // 重新获取更新后的信息
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
    
    // 计算缓冲区大小
    screensize = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
    
    // 映射framebuffer到内存
    fb_buffer = (char*)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if ((long)fb_buffer == -1) {
        perror("Failed to mmap framebuffer");
        return -5;
    }
    
    printf("Framebuffer created: %dx%d, %d bpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel);
    return 0;
}

// 清理函数
void cleanup() {
    if (fb_buffer) munmap(fb_buffer, screensize);
    if (fb_fd >= 0) close(fb_fd);
    bcm2835_gpio_write(BL_PIN, LOW);
    bcm2835_spi_end();
    bcm2835_close();
}

// 主函数
int main(int argc, char *argv[]) {
    printf("ST7735 User-space Framebuffer Driver\n");
    
    // 初始化bcm2835
    if (!bcm2835_init()) {
        printf("Failed to initialize bcm2835\n");
        return 1;
    }
    
    // 初始化ST7735
    st7735_init();
    
    // 创建framebuffer
    if (create_framebuffer() < 0) {
        printf("Failed to create framebuffer\n");
        cleanup();
        return 1;
    }
    
    // 清屏
    memset(fb_buffer, 0, screensize);
    flush_framebuffer();
    
    printf("Driver ready. Press Ctrl+C to exit.\n");
    
    // 主循环：定期刷新framebuffer到屏幕
    while (1) {
        flush_framebuffer();
        usleep(16666); // 约60Hz刷新率
    }
    
    cleanup();
    return 0;
}
EOF

# 4. 编译驱动程序
info "编译驱动程序..."
gcc -o "$DRIVER_NAME" "$DRIVER_NAME.c" -lbcm2835 -lm || {
    error "驱动程序编译失败"
}

# 5. 创建systemd服务文件
info "创建systemd服务文件..."
cat > "/etc/systemd/system/$SERVICE_NAME" << 'EOF'
[Unit]
Description=ST7735 User-space Framebuffer Driver
After=multi-user.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/root/go_run/st7735/st7735_fb
WorkingDirectory=/root/go_run/st7735
Restart=always
RestartSec=5
User=root
Environment=DISPLAY=:0

[Install]
WantedBy=multi-user.target
EOF

# 6. 创建控制台重定向脚本
info "创建控制台重定向脚本..."
cat > "/usr/local/bin/$CONSOLE_SCRIPT" << 'EOF'
#!/bin/bash

# 等待驱动程序启动
sleep 5

# 如果/dev/fb1不存在，创建一个
if [ ! -e "/dev/fb1" ]; then
    mknod /dev/fb1 c 29 1
    chmod 666 /dev/fb1
fi

# 设置控制台字体（小字体适合128x128分辨率）
if [ -f "/usr/share/consolefonts/Uni2-Terminus12x6.psf.gz" ]; then
    setfont /usr/share/consolefonts/Uni2-Terminus12x6.psf.gz
elif [ -f "/usr/share/consolefonts/ter-112n.psf.gz" ]; then
    setfont /usr/share/consolefonts/ter-112n.psf.gz
fi

# 重定向控制台输出
echo "Redirecting console to ST7735..."

# 尝试映射控制台到fb1
con2fbmap 1 1 2>/dev/null || true

# 设置环境变量
export FRAMEBUFFER=/dev/fb1

echo "Console redirected to ST7735"
EOF

# 设置脚本权限
chmod +x "/usr/local/bin/$CONSOLE_SCRIPT"

# 7. 配置系统启动
info "配置系统启动项..."

# 启用SPI接口
if ! grep -q "dtparam=spi=on" /boot/config.txt; then
    echo "dtparam=spi=on" >> /boot/config.txt
    info "已启用SPI接口"
else
    info "SPI接口已启用"
fi

# 添加控制台重定向到rc.local
if ! grep -q "$CONSOLE_SCRIPT" /etc/rc.local; then
    # 在exit 0之前添加脚本调用
    sed -i '/exit 0/i \/usr/local/bin/'"$CONSOLE_SCRIPT" /etc/rc.local
    chmod +x /etc/rc.local
    info "已添加控制台重定向到rc.local"
else
    info "控制台重定向已配置"
fi

# 启用并启动systemd服务
systemctl daemon-reload
systemctl enable "$SERVICE_NAME"
systemctl start "$SERVICE_NAME" || {
    warning "服务启动失败，将在重启后尝试"
}

info "安装完成！请重启树莓派使所有配置生效。"
echo -e "${YELLOW}重启命令: sudo reboot${NC}"