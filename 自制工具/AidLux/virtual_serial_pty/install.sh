#!/bin/bash

echo "安装虚拟串口转发器..."
echo ""

# 检查是否以root运行
if [ "$EUID" -ne 0 ]; then 
    echo "请使用sudo运行此脚本"
    echo "  sudo $0"
    exit 1
fi

# 编译
echo "1. 编译程序..."
gcc -o virtual_serial virtual_serial_pty.c -lpthread

if [ $? -ne 0 ]; then
    echo "编译失败!"
    exit 1
fi

# 复制到系统目录
echo "2. 安装到系统目录..."
cp virtual_serial /usr/local/bin/
chmod +x /usr/local/bin/virtual_serial

# 创建服务文件
echo "3. 创建系统服务..."
cat > /etc/systemd/system/virtual-serial.service << EOF
[Unit]
Description=Virtual Serial Port Forwarder
After=network.target

[Service]
Type=simple
User=$SUDO_USER
ExecStart=/usr/local/bin/virtual_serial -p /tmp/vcom0 -s 192.168.1.100:8080
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF

# 创建配置文件
echo "4. 创建配置文件目录..."
mkdir -p /etc/virtual-serial/

cat > /etc/virtual-serial/default.conf << EOF
# 虚拟串口转发器配置文件
# 虚拟串口路径
VIRTUAL_PORT=/tmp/vcom0

# 服务器地址
SERVER_IP=192.168.1.100
SERVER_PORT=8080

# 串口参数
BAUDRATE=115200
DATABITS=8
STOPBITS=1
PARITY=N
FLOW_CONTROL=false

# 其他设置
DEBUG=false
RETRY_COUNT=5
RECONNECT_DELAY=5
EOF

echo ""
echo "安装完成!"
echo ""
echo "使用方法:"
echo "  1. 手动运行: virtual_serial --help"
echo "  2. 系统服务: sudo systemctl start virtual-serial"
echo "  3. 开机启动: sudo systemctl enable virtual-serial"
echo ""
echo "配置文件: /etc/virtual-serial/default.conf"
echo ""
