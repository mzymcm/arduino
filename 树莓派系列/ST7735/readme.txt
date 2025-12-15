#========= 下载并安装BCM2835库
wget http://www.airspayce.com/mikem/bcm2835/bcm2835-1.71.tar.gz
tar xvfz bcm2835-1.71.tar.gz
cd bcm2835-1.71
./configure
make
sudo make install

#========= 创建文件 st7735.c
#========= 创建测试文件 test_st7735.c
#========= 创建头文件 st7735.h
#========= 创建Makefile


我们将使用BCM编号的GPIO引脚，并通过SPI接口与ST7735通信。由于树莓派A1的SPI接口可能有多个，我们使用SPI0（默认）。
引脚连接：
SCLK -> GPIO11 (BCM)
MOSI -> GPIO10 (BCM)
DC -> GPIO25 (BCM) (数据/命令选择)
CS -> GPIO8 (BCM) (片选，低电平有效)
RST -> GPIO27 (BCM) (复位，低电平复位)
BL -> GPIO24 (BCM) (背光，高电平点亮)


# 编译
make

# 运行 (需要sudo权限访问GPIO)
sudo ./test_st7735