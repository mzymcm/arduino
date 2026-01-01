#!/usr/bin/env python3
"""
minimal_client.py - 最简化客户端
"""

import socket
import time
import sys

def main():
    print("最简化串口桥接客户端")
    print("=" * 50)
    
    try:
        # 创建socket
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5)
        
        # 连接
        print(f"连接到 localhost:8888...")
        sock.connect(('localhost', 8888))
        print("连接成功！")
        
        # 接收欢迎信息
        print("\n接收欢迎信息...")
        welcome = b""
        sock.settimeout(2)
        
        while True:
            try:
                data = sock.recv(1024)
                if data:
                    welcome += data
                    sys.stdout.write(data.decode('utf-8', errors='replace'))
                    
                    # 检查是否收到提示符
                    if b'>' in data:
                        break
                else:
                    break
            except socket.timeout:
                if welcome:
                    break
                else:
                    print("等待超时")
                    break
        
        print("\n" + "="*50)
        
        # 交互循环
        while True:
            try:
                # 读取用户输入
                command = input("bridge> ").strip()
                
                if not command:
                    continue
                
                if command.lower() in ['quit', 'exit', 'q']:
                    print("退出")
                    break
                
                # 发送命令
                sock.sendall((command + '\n').encode())
                print("命令已发送，等待响应...")
                
                # 接收响应
                response = b""
                sock.settimeout(2)
                
                while True:
                    try:
                        data = sock.recv(1024)
                        if data:
                            response += data
                            decoded = data.decode('utf-8', errors='replace')
                            sys.stdout.write(decoded)
                            
                            # 检查是否收到提示符
                            if b'>' in data:
                                break
                        else:
                            break
                    except socket.timeout:
                        if response:
                            break
                        else:
                            print("响应超时")
                            break
                
            except KeyboardInterrupt:
                print("\n\n输入 'quit' 退出")
            except Exception as e:
                print(f"\n错误: {e}")
                break
        
        # 关闭连接
        sock.close()
        print("\n连接已关闭")
        
    except ConnectionRefusedError:
        print("连接被拒绝，请确保服务器正在运行")
    except socket.timeout:
        print("连接超时")
    except Exception as e:
        print(f"错误: {e}")

if __name__ == "__main__":
    main()
