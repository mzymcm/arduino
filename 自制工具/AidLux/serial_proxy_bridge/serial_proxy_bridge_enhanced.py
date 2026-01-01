#!/usr/bin/env python3
"""
serial_proxy_bridge_enhanced.py - 增强版串口文件代理桥接服务
版本: 2.1
功能: 提供TCP到串口的桥接服务，支持无串口模式、多客户端、配置管理
"""

import os
import sys
import time
import json
import socket
import struct
import logging
import threading
import subprocess
import tempfile
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import Optional, List, Dict, Any, Union
from datetime import datetime
import select
import queue

# ==================== 配置管理 ====================
@dataclass
class BridgeConfig:
    """桥接服务配置"""
    # 网络设置
    host: str = "0.0.0.0"  # 监听所有接口
    port: int = 8888
    max_clients: int = 10
    
    # 串口设置
    enable_serial: bool = True  # 是否启用串口功能
    serial_port: str = "/dev/ttyUSB0"
    baudrate: int = 115200
    bytesize: int = 8
    parity: str = 'N'
    stopbits: int = 1
    timeout: float = 1.0
    
    # 模拟串口设置
    simulation_mode: bool = False  # 是否使用模拟模式
    simulate_responses: bool = True  # 是否模拟响应
    simulation_data: List[str] = None  # 模拟数据列表
    
    # 安全设置
    require_auth: bool = False
    password: str = "default_pass"
    allowed_ips: List[str] = None  # None表示允许所有
    
    # 日志设置
    log_level: str = "INFO"
    log_file: str = "serial_bridge.log"
    log_to_console: bool = True
    
    # 性能设置
    buffer_size: int = 4096
    heartbeat_interval: int = 30  # 心跳间隔(秒)
    command_timeout: int = 5  # 命令超时时间(秒)
    
    # 工作模式
    mode: str = "auto"  # auto, serial, simulation, file, command
    
    # 文件/命令模式设置
    input_file: str = ""  # 输入文件路径
    output_file: str = ""  # 输出文件路径
    command_template: str = ""  # 命令模板
    
    def __post_init__(self):
        if self.allowed_ips is None:
            self.allowed_ips = []
        if self.simulation_data is None:
            self.simulation_data = ["OK", "ERROR", "READY", "DONE"]

# ==================== 日志系统 ====================
class BridgeLogger:
    """自定义日志处理器"""
    
    @staticmethod
    def setup_logger(config: BridgeConfig) -> logging.Logger:
        """配置日志系统"""
        logger = logging.getLogger("SerialBridge")
        logger.setLevel(getattr(logging, config.log_level.upper()))
        
        # 清除现有的处理器
        logger.handlers.clear()
        
        # 控制台输出格式
        console_format = logging.Formatter(
            '%(asctime)s - %(name)s - %(levelname)s - %(message)s',
            datefmt='%Y-%m-%d %H:%M:%S'
        )
        
        # 文件输出格式
        file_format = logging.Formatter(
            '%(asctime)s | %(levelname)-8s | %(message)s | [%(filename)s:%(lineno)d]',
            datefmt='%Y-%m-%d %H:%M:%S'
        )
        
        # 控制台处理器
        if config.log_to_console:
            console_handler = logging.StreamHandler()
            console_handler.setFormatter(console_format)
            console_handler.setLevel(logging.INFO)
            logger.addHandler(console_handler)
        
        # 文件处理器
        try:
            file_handler = logging.FileHandler(config.log_file, encoding='utf-8')
            file_handler.setFormatter(file_format)
            file_handler.setLevel(logging.DEBUG)
            logger.addHandler(file_handler)
        except Exception as e:
            print(f"无法创建日志文件: {e}")
        
        # 保存创建时间
        logger.created = time.time()
        
        return logger

# ==================== 串口管理器基类 ====================
class BasePortManager:
    """端口管理器基类"""
    
    def __init__(self, config: BridgeConfig, logger: logging.Logger):
        self.config = config
        self.logger = logger
        self.is_open = False
        self.mode = "base"
        
    def open_port(self) -> bool:
        """打开端口"""
        raise NotImplementedError("子类必须实现此方法")
    
    def close_port(self) -> bool:
        """关闭端口"""
        raise NotImplementedError("子类必须实现此方法")
    
    def write_data(self, data: bytes) -> int:
        """写入数据"""
        raise NotImplementedError("子类必须实现此方法")
    
    def read_data(self, size: int = 1024) -> bytes:
        """读取数据"""
        raise NotImplementedError("子类必须实现此方法")
    
    def get_status(self) -> Dict[str, Any]:
        """获取状态"""
        return {
            'mode': self.mode,
            'is_open': self.is_open,
            'config': {
                'serial_port': self.config.serial_port,
                'baudrate': self.config.baudrate,
            }
        }
    
    def detect_ports(self) -> List[Dict[str, Any]]:
        """检测可用端口"""
        return []

# ==================== 串口管理器 ====================
class SerialPortManager(BasePortManager):
    """物理串口管理器"""
    
    def __init__(self, config: BridgeConfig, logger: logging.Logger):
        super().__init__(config, logger)
        self.mode = "serial"
        self.serial_port = None
        
        # 尝试导入串口库
        try:
            import serial
            import serial.tools.list_ports
            self.serial = serial
            self.serial_available = True
            self.logger.info("串口库可用")
        except ImportError:
            self.serial = None
            self.serial_available = False
            self.logger.warning("串口库不可用，将使用模拟模式")
    
    def detect_ports(self) -> List[Dict[str, Any]]:
        """检测可用串口设备"""
        ports = []
        
        if self.serial_available:
            try:
                available_ports = self.serial.tools.list_ports.comports()
                for port in available_ports:
                    ports.append({
                        'device': port.device,
                        'name': port.name,
                        'description': port.description,
                        'hwid': port.hwid,
                        'vid': port.vid if port.vid else None,
                        'pid': port.pid if port.pid else None,
                        'type': 'serial'
                    })
                self.logger.info(f"检测到 {len(ports)} 个串口设备")
            except Exception as e:
                self.logger.error(f"检测串口失败: {e}")
        
        # 添加常见设备路径
        common_paths = [
            '/dev/ttyUSB0', '/dev/ttyUSB1', '/dev/ttyUSB2',
            '/dev/ttyACM0', '/dev/ttyACM1',
            'COM1', 'COM2', 'COM3', 'COM4',
        ]
        
        for path in common_paths:
            if not any(p['device'] == path for p in ports):
                ports.append({
                    'device': path,
                    'name': os.path.basename(path),
                    'description': '可能存在的设备',
                    'hwid': 'unknown',
                    'type': 'serial'
                })
        
        return ports
    
    def open_port(self) -> bool:
        """打开串口设备"""
        if self.is_open:
            self.close_port()
        
        if not self.serial_available:
            self.logger.warning("串口库不可用，无法打开真实串口")
            return False
        
        try:
            self.serial_port = self.serial.Serial(
                port=self.config.serial_port,
                baudrate=self.config.baudrate,
                bytesize=self.config.bytesize,
                parity=self.config.parity,
                stopbits=self.config.stopbits,
                timeout=self.config.timeout
            )
            
            self.is_open = self.serial_port.is_open
            if self.is_open:
                self.logger.info(f"串口已打开: {self.config.serial_port} @ {self.config.baudrate}bps")
            return self.is_open
            
        except Exception as e:
            self.logger.error(f"打开串口失败: {e}")
            return False
    
    def close_port(self) -> bool:
        """关闭串口设备"""
        if self.serial_port and hasattr(self.serial_port, 'is_open') and self.serial_port.is_open:
            try:
                self.serial_port.close()
                self.logger.info("串口已关闭")
                return True
            except Exception as e:
                self.logger.error(f"关闭串口失败: {e}")
                return False
        
        self.is_open = False
        self.serial_port = None
        return True
    
    def write_data(self, data: bytes) -> int:
        """向串口写入数据"""
        if not self.is_open or not self.serial_port:
            self.logger.error("串口未打开")
            return 0
        
        try:
            bytes_written = self.serial_port.write(data)
            self.serial_port.flush()
            self.logger.debug(f"发送数据: {data[:50]}... ({bytes_written} 字节)")
            return bytes_written
        except Exception as e:
            self.logger.error(f"写入串口失败: {e}")
            return 0
    
    def read_data(self, size: int = 1024) -> bytes:
        """从串口读取数据"""
        if not self.is_open or not self.serial_port:
            return b""
        
        try:
            data = self.serial_port.read(size)
            if data:
                self.logger.debug(f"接收数据: {data[:50]}... ({len(data)} 字节)")
            return data
        except Exception as e:
            self.logger.error(f"读取串口失败: {e}")
            return b""
    
    def get_status(self) -> Dict[str, Any]:
        """获取串口状态"""
        status = super().get_status()
        
        if self.is_open and self.serial_port:
            try:
                status.update({
                    'in_waiting': self.serial_port.in_waiting,
                    'out_waiting': self.serial_port.out_waiting,
                    'port_info': str(self.serial_port),
                })
            except:
                pass
        
        return status

# ==================== 模拟串口管理器 ====================
class SimulationPortManager(BasePortManager):
    """模拟串口管理器"""
    
    def __init__(self, config: BridgeConfig, logger: logging.Logger):
        super().__init__(config, logger)
        self.mode = "simulation"
        self.response_queue = queue.Queue()
        self.input_buffer = b""
        self.output_buffer = b""
        self.response_index = 0
        self._setup_simulation()
    
    def _setup_simulation(self):
        """设置模拟参数"""
        self.simulation_responses = self.config.simulation_data.copy()
        self.logger.info(f"模拟模式已启用，响应数据: {self.simulation_responses}")
    
    def open_port(self) -> bool:
        """打开模拟串口"""
        self.is_open = True
        self.logger.info("模拟串口已打开")
        return True
    
    def close_port(self) -> bool:
        """关闭模拟串口"""
        self.is_open = False
        self.logger.info("模拟串口已关闭")
        return True
    
    def write_data(self, data: bytes) -> int:
        """向模拟串口写入数据"""
        if not self.is_open:
            self.logger.error("模拟串口未打开")
            return 0
        
        # 记录输入数据
        self.input_buffer += data
        
        # 生成模拟响应
        if self.config.simulate_responses and self.simulation_responses:
            response = self.simulation_responses[self.response_index % len(self.simulation_responses)]
            self.response_index += 1
            self.output_buffer = response.encode()
            self.logger.debug(f"模拟发送数据: {data[:50]}... (生成响应: {response})")
        else:
            self.logger.debug(f"模拟发送数据: {data[:50]}...")
        
        return len(data)
    
    def read_data(self, size: int = 1024) -> bytes:
        """从模拟串口读取数据"""
        if not self.is_open:
            return b""
        
        # 返回缓存的响应数据
        if self.output_buffer:
            data = self.output_buffer[:size]
            self.output_buffer = self.output_buffer[size:]
            self.logger.debug(f"模拟接收数据: {data[:50]}... ({len(data)} 字节)")
            return data
        
        # 无数据时返回空或模拟数据
        if self.config.simulate_responses:
            return b"SIMULATION: No data available\r\n"
        
        return b""
    
    def get_status(self) -> Dict[str, Any]:
        """获取模拟串口状态"""
        status = super().get_status()
        status.update({
            'simulation_mode': True,
            'response_count': self.response_index,
            'input_buffer_size': len(self.input_buffer),
            'output_buffer_size': len(self.output_buffer),
            'available_responses': self.simulation_responses,
        })
        return status

# ==================== 文件模式管理器 ====================
class FilePortManager(BasePortManager):
    """文件模式管理器（通过文件读写）"""
    
    def __init__(self, config: BridgeConfig, logger: logging.Logger):
        super().__init__(config, logger)
        self.mode = "file"
        self.input_file = config.input_file
        self.output_file = config.output_file
        self.file_lock = threading.Lock()
        
        # 确保文件存在
        if self.input_file:
            Path(self.input_file).touch(exist_ok=True)
        if self.output_file:
            Path(self.output_file).touch(exist_ok=True)
    
    def open_port(self) -> bool:
        """打开文件端口"""
        self.is_open = True
        self.logger.info(f"文件模式已启用 - 输入: {self.input_file}, 输出: {self.output_file}")
        return True
    
    def close_port(self) -> bool:
        """关闭文件端口"""
        self.is_open = False
        self.logger.info("文件模式已关闭")
        return True
    
    def write_data(self, data: bytes) -> int:
        """向文件写入数据"""
        if not self.is_open:
            self.logger.error("文件模式未打开")
            return 0
        
        if not self.input_file:
            self.logger.error("未配置输入文件")
            return 0
        
        try:
            with self.file_lock:
                with open(self.input_file, 'ab') as f:
                    f.write(data)
                    f.write(b'\n')  # 添加换行符分隔
                self.logger.debug(f"写入文件 {self.input_file}: {data[:50]}...")
                return len(data)
        except Exception as e:
            self.logger.error(f"写入文件失败: {e}")
            return 0
    
    def read_data(self, size: int = 1024) -> bytes:
        """从文件读取数据"""
        if not self.is_open:
            return b""
        
        if not self.output_file or not os.path.exists(self.output_file):
            return b""
        
        try:
            with self.file_lock:
                with open(self.output_file, 'rb') as f:
                    data = f.read(size)
                # 清空文件（模拟读取）
                with open(self.output_file, 'wb') as f:
                    pass
                
                if data:
                    self.logger.debug(f"从文件 {self.output_file} 读取: {data[:50]}...")
                return data
        except Exception as e:
            self.logger.error(f"读取文件失败: {e}")
            return b""
    
    def get_status(self) -> Dict[str, Any]:
        """获取文件模式状态"""
        status = super().get_status()
        status.update({
            'input_file': self.input_file,
            'output_file': self.output_file,
            'input_file_exists': os.path.exists(self.input_file) if self.input_file else False,
            'output_file_exists': os.path.exists(self.output_file) if self.output_file else False,
        })
        return status

# ==================== 命令模式管理器 ====================
class CommandPortManager(BasePortManager):
    """命令模式管理器（通过系统命令）"""
    
    def __init__(self, config: BridgeConfig, logger: logging.Logger):
        super().__init__(config, logger)
        self.mode = "command"
        self.command_template = config.command_template or "echo 'Received: {}'"
        self.process = None
        self.command_lock = threading.Lock()
    
    def open_port(self) -> bool:
        """打开命令端口"""
        self.is_open = True
        self.logger.info(f"命令模式已启用 - 模板: {self.command_template}")
        return True
    
    def close_port(self) -> bool:
        """关闭命令端口"""
        if self.process:
            try:
                self.process.terminate()
                self.process.wait(timeout=2)
            except:
                pass
            self.process = None
        
        self.is_open = False
        self.logger.info("命令模式已关闭")
        return True
    
    def write_data(self, data: bytes) -> int:
        """通过命令处理数据"""
        if not self.is_open:
            self.logger.error("命令模式未打开")
            return 0
        
        try:
            # 解码数据为字符串
            data_str = data.decode('utf-8', errors='replace').strip()
            
            # 使用模板执行命令
            command = self.command_template.format(data_str)
            
            with self.command_lock:
                self.process = subprocess.Popen(
                    command,
                    shell=True,
                    stdout=subprocess.PIPE,
                    stderr=subprocess.PIPE,
                    text=True
                )
                
                stdout, stderr = self.process.communicate(timeout=self.config.command_timeout)
                
                if stdout:
                    self.logger.debug(f"命令输出: {stdout[:100]}")
                if stderr:
                    self.logger.warning(f"命令错误: {stderr[:100]}")
                
                self.logger.debug(f"执行命令: {command[:50]}...")
                
            return len(data)
            
        except subprocess.TimeoutExpired:
            self.logger.error("命令执行超时")
            if self.process:
                self.process.terminate()
            return 0
        except Exception as e:
            self.logger.error(f"执行命令失败: {e}")
            return 0
    
    def read_data(self, size: int = 1024) -> bytes:
        """命令模式通常不主动读取，只返回状态"""
        if not self.is_open:
            return b""
        
        # 返回状态信息
        status_msg = f"Command mode active - Template: {self.command_template}"
        return status_msg.encode()
    
    def get_status(self) -> Dict[str, Any]:
        """获取命令模式状态"""
        status = super().get_status()
        status.update({
            'command_template': self.command_template,
            'process_running': self.process is not None,
        })
        return status

# ==================== 端口管理器工厂 ====================
class PortManagerFactory:
    """端口管理器工厂"""
    
    @staticmethod
    def create_manager(config: BridgeConfig, logger: logging.Logger) -> BasePortManager:
        """根据配置创建端口管理器"""
        
        # 确定最终模式
        if config.mode == "auto":
            # 自动检测：尝试串口，失败则用模拟
            try:
                import serial
                actual_mode = "serial"
            except ImportError:
                actual_mode = "simulation"
        else:
            actual_mode = config.mode
        
        # 根据模式创建管理器
        if actual_mode == "serial":
            if config.enable_serial:
                return SerialPortManager(config, logger)
            else:
                logger.warning("串口功能被禁用，使用模拟模式")
                return SimulationPortManager(config, logger)
        
        elif actual_mode == "simulation":
            return SimulationPortManager(config, logger)
        
        elif actual_mode == "file":
            if config.input_file or config.output_file:
                return FilePortManager(config, logger)
            else:
                logger.warning("文件模式未配置输入/输出文件，使用模拟模式")
                return SimulationPortManager(config, logger)
        
        elif actual_mode == "command":
            if config.command_template:
                return CommandPortManager(config, logger)
            else:
                logger.warning("命令模式未配置命令模板，使用模拟模式")
                return SimulationPortManager(config, logger)
        
        else:
            logger.error(f"未知模式: {actual_mode}，使用模拟模式")
            return SimulationPortManager(config, logger)

# ==================== 客户端会话 ====================
class ClientSession:
    """客户端会话管理"""
    
    def __init__(self, socket: socket.socket, address: tuple, session_id: str):
        self.socket = socket
        self.address = address
        self.session_id = session_id
        self.connected_time = time.time()
        self.last_activity = time.time()
        self.authenticated = False
        self.buffer = b""
        self.username = None
        
    def update_activity(self):
        """更新最后活动时间"""
        self.last_activity = time.time()
    
    def is_timeout(self, timeout_seconds: int = 300) -> bool:
        """检查是否超时"""
        return (time.time() - self.last_activity) > timeout_seconds
    
    def send(self, data: bytes) -> bool:
        """发送数据到客户端"""
        try:
            self.socket.sendall(data)
            self.update_activity()
            return True
        except Exception:
            return False
    
    def recv(self, buffer_size: int = 4096) -> Optional[bytes]:
        """从客户端接收数据"""
        try:
            data = self.socket.recv(buffer_size)
            if data:
                self.update_activity()
            return data
        except Exception:
            return None
    
    def close(self):
        """关闭客户端连接"""
        try:
            self.socket.shutdown(socket.SHUT_RDWR)
        except:
            pass
        finally:
            self.socket.close()

# ==================== 命令处理器 ====================
class CommandHandler:
    """命令处理器"""
    
    def __init__(self, port_manager: BasePortManager, logger: logging.Logger):
        self.port_manager = port_manager
        self.logger = logger
        self.command_history = []
        self.max_history = 100
    
    def process_command(self, command: str, args: List[str] = None) -> Dict[str, Any]:
        """处理命令"""
        if args is None:
            args = []
        
        # 记录命令历史
        full_command = f"{command} {' '.join(args)}".strip()
        self.command_history.append(full_command)
        if len(self.command_history) > self.max_history:
            self.command_history.pop(0)
        
        command = command.lower()
        
        # 命令路由
        if command == "help":
            return self._cmd_help()
        elif command == "test":
            return self._cmd_test()
        elif command == "open":
            return self._cmd_open()
        elif command == "close":
            return self._cmd_close()
        elif command == "status":
            return self._cmd_status()
        elif command == "send":
            return self._cmd_send(args)
        elif command == "read":
            return self._cmd_read(args)
        elif command == "list":
            return self._cmd_list_ports()
        elif command == "config":
            return self._cmd_config(args)
        elif command == "echo":
            return self._cmd_echo(args)
        elif command == "time":
            return self._cmd_time()
        elif command == "mode":
            return self._cmd_mode(args)
        elif command == "history":
            return self._cmd_history(args)
        elif command == "clear":
            return self._cmd_clear()
        elif command == "ping":
            return self._cmd_ping(args)
        elif command == "quit" or command == "exit":
            return {"success": True, "message": "再见", "quit": True}
        else:
            return {
                "success": False,
                "message": f"未知命令: {command}。输入 'help' 查看帮助",
            }
    
    def _cmd_help(self) -> Dict[str, Any]:
        """帮助命令"""
        help_text = """
串口代理桥接服务 - 命令帮助

基本命令:
  help               - 显示此帮助信息
  test               - 测试连接
  open               - 打开端口
  close              - 关闭端口
  status             - 查看状态
  send <data>        - 发送数据 (支持十六进制: send hex:4142)
  read [timeout]     - 从端口读取数据 (可选超时时间)
  list               - 列出可用端口
  config <key=value> - 配置参数
  echo <message>     - 回声测试
  time               - 显示服务器时间
  mode [mode]        - 查看或切换模式 (serial, simulation, file, command)
  history [n]        - 查看命令历史
  clear              - 清屏
  ping [host]        - 网络测试
  quit/exit          - 退出连接

端口模式:
  1. Serial - 物理串口模式
  2. Simulation - 模拟模式
  3. File - 文件读写模式
  4. Command - 系统命令模式

示例:
  send Hello         # 发送字符串
  send hex:414243    # 发送十六进制数据
  read 2             # 读取数据，等待2秒
  mode simulation    # 切换到模拟模式
  config baudrate=9600
"""
        return {"success": True, "message": help_text}
    
    def _cmd_test(self) -> Dict[str, Any]:
        """测试命令"""
        return {"success": True, "message": "服务器运行正常"}
    
    def _cmd_open(self) -> Dict[str, Any]:
        """打开端口"""
        if self.port_manager.open_port():
            return {"success": True, "message": "端口已打开"}
        else:
            return {"success": False, "message": "打开端口失败"}
    
    def _cmd_close(self) -> Dict[str, Any]:
        """关闭端口"""
        if self.port_manager.close_port():
            return {"success": True, "message": "端口已关闭"}
        else:
            return {"success": False, "message": "关闭端口失败"}
    
    def _cmd_status(self) -> Dict[str, Any]:
        """状态命令"""
        status = self.port_manager.get_status()
        status_text = "\n".join([f"  {k}: {v}" for k, v in status.items()])
        return {"success": True, "message": f"当前状态:\n{status_text}"}
    
    def _cmd_send(self, args: List[str]) -> Dict[str, Any]:
        """发送数据"""
        if not args:
            return {"success": False, "message": "需要指定要发送的数据"}
        
        data_str = " ".join(args)
        
        # 处理十六进制数据
        if data_str.lower().startswith("hex:"):
            try:
                hex_data = data_str[4:].strip()
                # 移除可能的分隔符
                hex_data = hex_data.replace(" ", "").replace(":", "")
                data = bytes.fromhex(hex_data)
            except ValueError as e:
                return {"success": False, "message": f"十六进制数据格式错误: {e}"}
        else:
            data = data_str.encode()
        
        # 发送数据
        bytes_written = self.port_manager.write_data(data)
        
        if bytes_written > 0:
            return {"success": True, "message": f"成功发送 {bytes_written} 字节"}
        else:
            return {"success": False, "message": "发送失败"}
    
    def _cmd_read(self, args: List[str]) -> Dict[str, Any]:
        """读取数据"""
        timeout = 1.0  # 默认超时时间
        if args and args[0].isdigit():
            timeout = float(args[0])
        
        # 读取数据
        data = self.port_manager.read_data(1024)
        
        # 如果没有数据且需要等待，可以稍微等待一下
        if not data and timeout > 0:
            time.sleep(min(timeout, 0.1))
            data = self.port_manager.read_data(1024)
        
        if data:
            # 尝试解码为字符串
            try:
                text = data.decode('utf-8', errors='replace')
                hex_repr = data.hex()
                message = f"接收到 {len(data)} 字节:\n"
                message += f"  文本: {text}\n"
                message += f"  十六进制: {hex_repr}"
            except:
                hex_repr = data.hex()
                message = f"接收到 {len(data)} 字节 (十六进制): {hex_repr}"
            return {"success": True, "message": message}
        else:
            return {"success": True, "message": "没有数据"}
    
    def _cmd_list_ports(self) -> Dict[str, Any]:
        """列出端口"""
        ports = self.port_manager.detect_ports()
        if not ports:
            return {"success": True, "message": "未检测到端口设备"}
        
        result = "可用端口设备:\n"
        for i, port in enumerate(ports, 1):
            result += f"  {i}. {port['device']}"
            if port.get('description'):
                result += f" - {port['description']}"
            result += f" ({port.get('type', 'unknown')})\n"
        
        return {"success": True, "message": result}
    
    def _cmd_config(self, args: List[str]) -> Dict[str, Any]:
        """配置命令"""
        if not args:
            return self._cmd_status()
        
        config_str = " ".join(args)
        if "=" not in config_str:
            return {"success": False, "message": "格式错误，使用: config key=value"}
        
        key, value = config_str.split("=", 1)
        key = key.strip().lower()
        value = value.strip()
        
        return {
            "success": True, 
            "message": f"配置已更新: {key}={value} (部分配置需要重启生效)"
        }
    
    def _cmd_echo(self, args: List[str]) -> Dict[str, Any]:
        """回声命令"""
        message = " ".join(args) if args else "回声测试"
        return {"success": True, "message": f"回声: {message}"}
    
    def _cmd_time(self) -> Dict[str, Any]:
        """时间命令"""
        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        uptime = time.time() - self.logger.created
        hours, remainder = divmod(uptime, 3600)
        minutes, seconds = divmod(remainder, 60)
        
        return {
            "success": True,
            "message": f"服务器时间: {current_time}\n运行时间: {int(hours)}时{int(minutes)}分{int(seconds)}秒"
        }
    
    def _cmd_mode(self, args: List[str]) -> Dict[str, Any]:
        """模式命令"""
        if not args:
            current_mode = self.port_manager.mode
            return {
                "success": True,
                "message": f"当前模式: {current_mode}\n可用模式: serial, simulation, file, command"
            }
        
        new_mode = args[0].lower()
        if new_mode in ["serial", "simulation", "file", "command"]:
            return {
                "success": True,
                "message": f"模式已切换到: {new_mode} (需要重启服务生效)"
            }
        else:
            return {
                "success": False,
                "message": f"无效模式: {new_mode}。可用模式: serial, simulation, file, command"
            }
    
    def _cmd_history(self, args: List[str]) -> Dict[str, Any]:
        """历史命令"""
        count = 10
        if args and args[0].isdigit():
            count = min(int(args[0]), len(self.command_history))
        
        if not self.command_history:
            return {"success": True, "message": "命令历史为空"}
        
        history_text = "最近命令历史:\n"
        start_idx = max(0, len(self.command_history) - count)
        for i, cmd in enumerate(self.command_history[start_idx:], start=start_idx+1):
            history_text += f"  {i}. {cmd}\n"
        
        return {"success": True, "message": history_text}
    
    def _cmd_clear(self) -> Dict[str, Any]:
        """清屏命令"""
        # 发送清屏序列
        return {"success": True, "message": "\033[2J\033[H"}
    
    def _cmd_ping(self, args: List[str]) -> Dict[str, Any]:
        """ping命令"""
        target = args[0] if args else "127.0.0.1"
        
        # 简单的模拟ping
        if target in ["127.0.0.1", "localhost", "local"]:
            return {"success": True, "message": f"PING {target}: 响应时间 <1ms"}
        else:
            # 尝试真实ping
            try:
                import platform
                param = "-n 1" if platform.system().lower() == "windows" else "-c 1"
                command = f"ping {param} {target}"
                result = subprocess.run(command, shell=True, capture_output=True, text=True, timeout=2)
                
                if result.returncode == 0:
                    return {"success": True, "message": f"PING {target}: 成功"}
                else:
                    return {"success": False, "message": f"PING {target}: 失败"}
            except:
                return {"success": False, "message": f"无法PING {target}"}

# ==================== 主桥接服务 ====================
class EnhancedSerialBridge:
    """增强版串口桥接服务"""
    
    def __init__(self, config: BridgeConfig = None):
        self.config = config or BridgeConfig()
        
        # 初始化日志
        self.logger = BridgeLogger.setup_logger(self.config)
        
        # 初始化端口管理器
        self.port_manager = PortManagerFactory.create_manager(self.config, self.logger)
        
        # 初始化命令处理器
        self.command_handler = CommandHandler(self.port_manager, self.logger)
        
        # 服务状态
        self.running = False
        self.server_socket = None
        self.clients: Dict[str, ClientSession] = {}
        self.client_lock = threading.Lock()
        
        # 工作线程
        self.worker_thread = None
        self.monitor_thread = None
        
        self.logger.info("串口桥接服务初始化完成")
        self.logger.info(f"运行模式: {self.port_manager.mode}")
        
        # 显示配置信息
        self.logger.info(f"监听地址: {self.config.host}:{self.config.port}")
        if self.port_manager.mode == "serial":
            self.logger.info(f"串口设备: {self.config.serial_port} @ {self.config.baudrate}bps")
    
    def start(self) -> bool:
        """启动服务"""
        try:
            # 创建服务器socket
            self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            
            # 设置非阻塞模式
            self.server_socket.setblocking(0)
            
            # 绑定地址
            self.server_socket.bind((self.config.host, self.config.port))
            self.server_socket.listen(self.config.max_clients)
            
            self.running = True
            
            # 启动监控线程
            self.monitor_thread = threading.Thread(target=self._monitor_clients, daemon=True)
            self.monitor_thread.start()
            
            # 启动工作线程
            self.worker_thread = threading.Thread(target=self._accept_clients, daemon=True)
            self.worker_thread.start()
            
            self.logger.info(f"✅ 串口代理桥接服务已启动 (版本 2.1)")
            self.logger.info(f"   监听地址: {self.config.host}:{self.config.port}")
            self.logger.info(f"   运行模式: {self.port_manager.mode}")
            self.logger.info(f"   最大客户端数: {self.config.max_clients}")
            self.logger.info(f"   按 Ctrl+C 停止服务")
            self.logger.info("-" * 50)
            
            return True
            
        except Exception as e:
            self.logger.error(f"启动服务失败: {e}")
            return False
    
    def stop(self):
        """停止服务"""
        self.running = False
        
        # 关闭所有客户端连接
        with self.client_lock:
            for session_id, client in list(self.clients.items()):
                try:
                    client.close()
                    self.logger.info(f"关闭客户端: {client.address}")
                except:
                    pass
            self.clients.clear()
        
        # 关闭服务器socket
        if self.server_socket:
            try:
                self.server_socket.close()
            except:
                pass
        
        # 关闭端口
        self.port_manager.close_port()
        
        self.logger.info("服务已停止")
    
    def _accept_clients(self):
        """接受客户端连接"""
        while self.running:
            try:
                # 使用select检查是否有新连接
                ready, _, _ = select.select([self.server_socket], [], [], 0.5)
                if ready:
                    client_socket, address = self.server_socket.accept()
                    
                    # 生成会话ID
                    session_id = f"{address[0]}:{address[1]}-{int(time.time())}"
                    
                    # 创建客户端会话
                    client = ClientSession(client_socket, address, session_id)
                    
                    # 检查IP白名单
                    if self.config.allowed_ips and address[0] not in self.config.allowed_ips:
                        self.logger.warning(f"拒绝连接: {address} (不在白名单中)")
                        client.send("连接被拒绝: IP地址未授权\n".encode())
                        client.close()
                        continue
                    
                    # 添加到客户端列表
                    with self.client_lock:
                        self.clients[session_id] = client
                    
                    # 启动客户端处理线程
                    client_thread = threading.Thread(
                        target=self._handle_client,
                        args=(client,),
                        daemon=True
                    )
                    client_thread.start()
                    
                    self.logger.info(f"客户端连接: {address} (会话: {session_id})")
                    
            except socket.timeout:
                continue
            except OSError as e:
                if self.running:
                    self.logger.error(f"接受连接错误: {e}")
                break
            except Exception as e:
                if self.running:
                    self.logger.error(f"接受连接异常: {e}")
    
    def _handle_client(self, client: ClientSession):
        """处理客户端连接"""
        try:
            # 发送欢迎信息
            welcome = f"""
欢迎使用串口代理桥接服务 (v2.1)
会话ID: {client.session_id}
运行模式: {self.port_manager.mode}
连接时间: {datetime.fromtimestamp(client.connected_time).strftime('%Y-%m-%d %H:%M:%S')}
输入 'help' 查看可用命令
输入 'quit' 退出

> """
            client.send(welcome.encode())
            
            # 如果需要认证
            if self.config.require_auth and not client.authenticated:
                client.send("请输入密码: ".encode())
                password = client.recv().decode().strip() if client.recv() else ""
                if password != self.config.password:
                    client.send("密码错误，连接被拒绝\n".encode())
                    return
                client.authenticated = True
                client.send("认证成功！\n\n> ".encode())
            
            # 处理客户端命令
            while self.running and client.socket.fileno() != -1:
                try:
                    # 接收命令
                    data = client.recv(self.config.buffer_size)
                    if not data:
                        break
                    
                    # 添加到缓冲区
                    client.buffer += data
                    
                    # 处理完整的命令 (以换行符分隔)
                    if b'\n' in client.buffer:
                        lines = client.buffer.split(b'\n')
                        for line in lines[:-1]:  # 处理所有完整行
                            command = line.decode().strip()
                            if command:
                                self._process_client_command(client, command)
                        
                        # 保留最后不完整的行
                        client.buffer = lines[-1]
                    
                except socket.timeout:
                    continue
                except ConnectionResetError:
                    break
                except Exception as e:
                    self.logger.error(f"处理客户端数据错误: {e}")
                    break
                
        except Exception as e:
            self.logger.error(f"客户端处理异常: {e}")
        finally:
            # 清理客户端
            with self.client_lock:
                if client.session_id in self.clients:
                    del self.clients[client.session_id]
            
            client.close()
            self.logger.info(f"客户端断开: {client.address}")
    
    def _process_client_command(self, client: ClientSession, command: str):
        """处理客户端命令"""
        # 解析命令和参数
        parts = command.split()
        cmd = parts[0] if parts else ""
        args = parts[1:] if len(parts) > 1 else []
        
        # 处理命令
        result = self.command_handler.process_command(cmd, args)
        
        # 发送响应
        response = f"{result['message']}\n"
        if not result.get('quit', False):
            response += "\n> "
        
        client.send(response.encode())
        
        # 如果是退出命令，关闭连接
        if result.get('quit', False):
            with self.client_lock:
                if client.session_id in self.clients:
                    del self.clients[client.session_id]
            client.close()
    
    def _monitor_clients(self):
        """监控客户端连接"""
        while self.running:
            time.sleep(30)  # 每30秒检查一次
            
            with self.client_lock:
                current_time = time.time()
                timeout_clients = []
                
                # 检查超时客户端
                for session_id, client in self.clients.items():
                    if client.is_timeout(300):  # 5分钟超时
                        timeout_clients.append(session_id)
                
                # 断开超时客户端
                for session_id in timeout_clients:
                    client = self.clients[session_id]
                    self.logger.info(f"客户端超时断开: {client.address}")
                    client.close()
                    del self.clients[session_id]
                
                # 发送心跳给所有客户端
                for client in self.clients.values():
                    try:
                        # 发送空提示符保持连接
                        if time.time() - client.last_activity > 60:
                            client.send(b"\n> ")
                    except:
                        pass
    
    def get_status(self) -> Dict[str, Any]:
        """获取服务状态"""
        with self.client_lock:
            client_count = len(self.clients)
            clients_info = [
                {
                    'session_id': c.session_id,
                    'address': c.address,
                    'connected_time': c.connected_time,
                    'last_activity': c.last_activity,
                    'authenticated': c.authenticated,
                }
                for c in self.clients.values()
            ]
        
        return {
            'running': self.running,
            'uptime': time.time() - self.logger.created,
            'client_count': client_count,
            'clients': clients_info,
            'port_status': self.port_manager.get_status(),
            'config': asdict(self.config),
        }

# ==================== 配置文件管理 ====================
class ConfigManager:
    """配置文件管理器"""
    
    @staticmethod
    def load_config(filepath: str = "config.json") -> BridgeConfig:
        """从文件加载配置"""
        default_config = BridgeConfig()
        
        if os.path.exists(filepath):
            try:
                with open(filepath, 'r', encoding='utf-8') as f:
                    if filepath.endswith('.json'):
                        data = json.load(f)
                    elif filepath.endswith('.yaml') or filepath.endswith('.yml'):
                        import yaml
                        data = yaml.safe_load(f)
                    else:
                        return default_config
                
                # 更新配置
                for key, value in data.items():
                    if hasattr(default_config, key):
                        setattr(default_config, key, value)
                
                print(f"配置文件加载成功: {filepath}")
                
            except Exception as e:
                print(f"加载配置文件失败: {e}")
        
        return default_config
    
    @staticmethod
    def save_config(config: BridgeConfig, filepath: str = "config.json"):
        """保存配置到文件"""
        try:
            data = asdict(config)
            
            with open(filepath, 'w', encoding='utf-8') as f:
                if filepath.endswith('.json'):
                    json.dump(data, f, indent=2, ensure_ascii=False)
                elif filepath.endswith('.yaml') or filepath.endswith('.yml'):
                    import yaml
                    yaml.dump(data, f, default_flow_style=False, allow_unicode=True)
            
            print(f"配置文件已保存: {filepath}")
            
        except Exception as e:
            print(f"保存配置文件失败: {e}")
    
    @staticmethod
    def create_sample_config(filepath: str = "config_sample.json"):
        """创建示例配置文件"""
        config = BridgeConfig()
        config.enable_serial = False
        config.simulation_mode = True
        config.mode = "simulation"
        config.simulation_data = ["OK", "READY", "ERROR:01", "DATA_RECEIVED"]
        
        ConfigManager.save_config(config, filepath)
        print(f"示例配置文件已创建: {filepath}")

class AutoOpenBridge(EnhancedSerialBridge):
    """自动打开端口的桥接服务"""
    
    def __init__(self, config: BridgeConfig = None):
        super().__init__(config)
        
        # 启动时自动打开端口
        self.logger.info("正在自动打开端口...")
        if self.port_manager.open_port():
            self.logger.info("✅ 端口已成功打开")
            status = self.port_manager.get_status()
            self.logger.info(f"端口状态: {status}")
        else:
            self.logger.warning("⚠️ 自动打开端口失败，可能需要手动打开")
            self.logger.info("客户端连接后可以发送 'open' 命令手动打开")

# ==================== 主函数 ====================
def main():
    """主函数"""
    print("\n" + "="*60)
    print("    串口代理桥接服务 - 增强版 v2.1")
    print("="*60)
    
    import argparse
    
    # 命令行参数
    parser = argparse.ArgumentParser(description='串口代理桥接服务')
    parser.add_argument('--config', '-c', default='config.json', help='配置文件路径')
    parser.add_argument('--port', '-p', type=int, help='监听端口')
    parser.add_argument('--host', default='0.0.0.0', help='监听地址')
    parser.add_argument('--mode', choices=['auto', 'serial', 'simulation', 'file', 'command'], 
                       help='运行模式')
    parser.add_argument('--no-serial', action='store_true', help='禁用串口模式')
    parser.add_argument('--simulation', action='store_true', help='启用模拟模式')
    parser.add_argument('--create-sample', action='store_true', help='创建示例配置文件')
    parser.add_argument('--list-ports', action='store_true', help='列出可用端口')
    
    args = parser.parse_args()
    
    # 创建示例配置
    if args.create_sample:
        ConfigManager.create_sample_config()
        return
    
    # 加载配置
    config = ConfigManager.load_config(args.config)
    
    # 覆盖命令行参数
    if args.port:
        config.port = args.port
    if args.host:
        config.host = args.host
    if args.mode:
        config.mode = args.mode
    if args.no_serial:
        config.enable_serial = False
    if args.simulation:
        config.mode = "simulation"
        config.simulation_mode = True
    
    # 列出端口
    if args.list_ports:
        print("\n检测可用端口...")
        try:
            import serial.tools.list_ports
            ports = list(serial.tools.list_ports.comports())
            if ports:
                print("可用串口设备:")
                for i, port in enumerate(ports, 1):
                    print(f"  {i}. {port.device} - {port.description}")
            else:
                print("未检测到串口设备")
        except ImportError:
            print("串口库未安装，无法检测设备")
        return
    
    # 显示配置信息
    print(f"\n配置信息:")
    print(f"  监听地址: {config.host}:{config.port}")
    print(f"  运行模式: {config.mode}")
    print(f"  串口功能: {'启用' if config.enable_serial else '禁用'}")
    
    if config.mode == "serial" and config.enable_serial:
        print(f"  串口设备: {config.serial_port}")
        print(f"  波特率: {config.baudrate}")
    elif config.mode == "simulation":
        print(f"  模拟模式: 启用")
        print(f"  模拟响应: {config.simulation_data}")
    elif config.mode == "file":
        print(f"  文件模式: 启用")
        print(f"  输入文件: {config.input_file}")
        print(f"  输出文件: {config.output_file}")
    elif config.mode == "command":
        print(f"  命令模式: 启用")
        print(f"  命令模板: {config.command_template}")
    
    print(f"  日志级别: {config.log_level}")
    print(f"  认证要求: {'是' if config.require_auth else '否'}")
    
    print("\n" + "-"*60)
    
    # 创建并启动服务
    bridge = AutoOpenBridge(config)
    
    try:
        if bridge.start():
            # 保持主线程运行
            while bridge.running:
                try:
                    time.sleep(1)
                except KeyboardInterrupt:
                    print("\n\n收到停止信号，正在关闭服务...")
                    break
        
    except Exception as e:
        print(f"\n服务运行异常: {e}")
    
    finally:
        bridge.stop()
        print("服务已停止")
        print("="*60 + "\n")

if __name__ == "__main__":
    main()
