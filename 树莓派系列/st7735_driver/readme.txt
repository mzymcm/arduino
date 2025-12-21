st7735_driver/
├── Makefile
├── st7735.h
├── st7735.c
└── main.c

# 1. 创建项目目录并进入
mkdir st7735_driver
cd st7735_driver

# 2. 创建上述4个文件：
#    st7735.h, st7735.c, main.c, Makefile

# 3. 编译程序
make

# 4. 运行程序（需要root权限）
sudo ./st7735_demo

# 5. 清理编译文件
make clean