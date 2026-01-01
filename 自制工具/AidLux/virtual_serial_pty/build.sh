#!/bin/bash

echo "========================================="
echo "编译虚拟串口转发器 (无警告版本)"
echo "========================================="
echo ""

# 设置文件名
SOURCE_FILE="virtual_serial_pty.c"
OUTPUT_FILE="virtual_serial"

# 清理旧文件
if [ -f "$OUTPUT_FILE" ]; then
    echo "备份旧文件: $OUTPUT_FILE -> ${OUTPUT_FILE}.old"
    mv "$OUTPUT_FILE" "${OUTPUT_FILE}.old"
fi

# 编译（关闭某些警告）
echo "编译源代码..."
gcc -Wall -Wextra -Wno-stringop-truncation -Wno-implicit-function-declaration \
    -O2 -o "$OUTPUT_FILE" "$SOURCE_FILE" -lpthread

if [ $? -eq 0 ]; then
    echo ""
    echo "✓ 编译成功!"
    echo "  生成的可执行文件: $OUTPUT_FILE"
    
    # 设置执行权限
    chmod +x "$OUTPUT_FILE"
    
    # 测试基本功能
    echo ""
    echo "基本功能测试:"
    echo "1. 检查可执行文件..."
    if [ -x "$OUTPUT_FILE" ]; then
        echo "   ✓ 可执行文件权限正常"
    else
        echo "   ✗ 可执行文件权限异常"
    fi
    
    echo "2. 检查依赖..."
    ldd "$OUTPUT_FILE" > /dev/null 2>&1
    if [ $? -eq 0 ]; then
        echo "   ✓ 动态库依赖正常"
    else
        echo "   ✗ 动态库依赖异常"
    fi
    
    echo ""
    echo "快速使用:"
    echo "  ./$OUTPUT_FILE -h                  # 查看帮助"
    echo "  ./$OUTPUT_FILE -p /tmp/vcom0 \\"
    echo "    -s 127.0.0.1:8080 -v            # 启动虚拟串口"
    echo ""
    echo "测试步骤:"
    echo "  1. 启动TCP服务器: nc -l 8080"
    echo "  2. 启动虚拟串口: ./$OUTPUT_FILE -p /tmp/vcom0 -s 127.0.0.1:8080 -v"
    echo "  3. 发送数据: echo 'test' > /tmp/vcom0"
    echo "  4. 在TCP服务器终端应该看到 'test'"
else
    echo ""
    echo "✗ 编译失败!"
    exit 1
fi

echo ""
echo "编译完成!"
