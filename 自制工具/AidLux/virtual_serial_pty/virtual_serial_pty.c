#define _XOPEN_SOURCE   700
#define _GNU_SOURCE     1

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <errno.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/time.h>
#include <stdbool.h>
#include <getopt.h>
#include <netdb.h>

#define BUFFER_SIZE 4096
#define DEFAULT_VIRTUAL_PORT "/tmp/vcom0"
#define DEFAULT_SERVER_IP "127.0.0.1"
#define DEFAULT_SERVER_PORT 8080
#define MAX_RETRY_COUNT 5

// 配置结构
typedef struct {
    char virtual_port[256];
    char slave_name[256];
    char server_ip[64];
    int server_port;
    int baudrate;
    int data_bits;
    int stop_bits;
    char parity;
    bool flow_control;
    bool debug;
    int retry_count;
    int reconnect_delay;
} Config;

// 运行时结构
typedef struct {
    int pty_master;
    int pty_slave;
    int socket_fd;
    pthread_t thread_pty2net;
    pthread_t thread_net2pty;
    pthread_mutex_t mutex;
    bool running;
    bool connected;
    Config config;
} VirtualSerial;

static VirtualSerial *g_vserial = NULL;

void signal_handler(int sig) {
    printf("\n收到信号 %d，正在退出...\n", sig);
    if (g_vserial != NULL) {
        g_vserial->running = false;
    }
}

// 安全字符串复制函数
void safe_strcpy(char *dest, const char *src, size_t dest_size) {
    if (dest_size > 0) {
        size_t len = strlen(src);
        if (len >= dest_size) {
            len = dest_size - 1;
        }
        memcpy(dest, src, len);
        dest[len] = '\0';
    }
}

// 设置终端为原始模式
void set_raw_mode(struct termios *tios) {
    // 禁用规范模式
    tios->c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    
    // 禁用输出处理
    tios->c_oflag &= ~OPOST;
    
    // 设置输入模式
    tios->c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    
    // 设置控制模式
    tios->c_cflag &= ~(CSIZE | PARENB);
    tios->c_cflag |= CS8;
    
    // 设置控制字符
    tios->c_cc[VMIN] = 0;
    tios->c_cc[VTIME] = 10;
}

// 使用 posix_openpt 创建虚拟串口
int create_virtual_serial(VirtualSerial *vserial) {
    char slave_name[256];
    
    // 使用 posix_openpt 创建伪终端主设备
    vserial->pty_master = posix_openpt(O_RDWR | O_NOCTTY);
    if (vserial->pty_master == -1) {
        perror("posix_openpt失败");
        return -1;
    }
    
    // 设置主设备权限
    if (grantpt(vserial->pty_master) == -1) {
        perror("grantpt失败");
        close(vserial->pty_master);
        return -1;
    }
    
    // 解锁主设备
    if (unlockpt(vserial->pty_master) == -1) {
        perror("unlockpt失败");
        close(vserial->pty_master);
        return -1;
    }
    
    // 获取从设备名称
    if (ptsname_r(vserial->pty_master, slave_name, sizeof(slave_name)) != 0) {
        perror("ptsname_r失败");
        close(vserial->pty_master);
        return -1;
    }
    
    // 打开从设备
    vserial->pty_slave = open(slave_name, O_RDWR | O_NOCTTY);
    if (vserial->pty_slave == -1) {
        perror("打开从设备失败");
        close(vserial->pty_master);
        return -1;
    }
    
    // 安全复制字符串
    safe_strcpy(vserial->config.slave_name, slave_name, sizeof(vserial->config.slave_name));
    
    // 设置终端属性
    struct termios tios;
    if (tcgetattr(vserial->pty_master, &tios) == -1) {
        perror("tcgetattr失败");
        close(vserial->pty_master);
        close(vserial->pty_slave);
        return -1;
    }
    
    // 设置为原始模式
    set_raw_mode(&tios);
    
    // 设置波特率
    speed_t speed;
    switch (vserial->config.baudrate) {
        case 9600:   speed = B9600; break;
        case 19200:  speed = B19200; break;
        case 38400:  speed = B38400; break;
        case 57600:  speed = B57600; break;
        case 115200: speed = B115200; break;
        case 230400: speed = B230400; break;
        case 460800: speed = B460800; break;
        case 921600: speed = B921600; break;
        default:     speed = B115200; break;
    }
    cfsetispeed(&tios, speed);
    cfsetospeed(&tios, speed);
    
    // 设置数据位
    tios.c_cflag &= ~CSIZE;
    switch (vserial->config.data_bits) {
        case 5: tios.c_cflag |= CS5; break;
        case 6: tios.c_cflag |= CS6; break;
        case 7: tios.c_cflag |= CS7; break;
        case 8: tios.c_cflag |= CS8; break;
        default: tios.c_cflag |= CS8; break;
    }
    
    // 设置停止位
    if (vserial->config.stop_bits == 2) {
        tios.c_cflag |= CSTOPB;
    } else {
        tios.c_cflag &= ~CSTOPB;
    }
    
    // 设置校验位
    switch (vserial->config.parity) {
        case 'O': case 'o':  // 奇校验
            tios.c_cflag |= PARENB;
            tios.c_cflag |= PARODD;
            tios.c_iflag |= INPCK;
            break;
        case 'E': case 'e':  // 偶校验
            tios.c_cflag |= PARENB;
            tios.c_cflag &= ~PARODD;
            tios.c_iflag |= INPCK;
            break;
        case 'N': case 'n':  // 无校验
        default:
            tios.c_cflag &= ~PARENB;
            tios.c_iflag &= ~INPCK;
            break;
    }
    
    // 设置流控
    if (vserial->config.flow_control) {
        tios.c_cflag |= CRTSCTS;
    } else {
        tios.c_cflag &= ~CRTSCTS;
        tios.c_iflag &= ~(IXON | IXOFF | IXANY);
    }
    
    // 应用设置
    if (tcsetattr(vserial->pty_master, TCSANOW, &tios) == -1) {
        perror("tcsetattr失败");
        close(vserial->pty_master);
        close(vserial->pty_slave);
        return -1;
    }
    
    // 刷新缓冲区
    tcflush(vserial->pty_master, TCIOFLUSH);
    
    // 创建符号链接
    unlink(vserial->config.virtual_port);
    if (symlink(slave_name, vserial->config.virtual_port) == -1) {
        perror("创建符号链接失败");
        close(vserial->pty_master);
        close(vserial->pty_slave);
        return -1;
    }
    
    // 设置权限，让其他用户也可以访问
    chmod(slave_name, 0666);
    
    printf("✓ 创建虚拟串口成功\n");
    printf("  虚拟设备: %s\n", vserial->config.virtual_port);
    printf("  真实设备: %s\n", slave_name);
    printf("  波特率: %d\n", vserial->config.baudrate);
    
    return 0;
}

// 连接到服务器
int connect_to_server(VirtualSerial *vserial) {
    struct sockaddr_in server_addr;
    struct addrinfo hints, *res;
    char port_str[16];
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    snprintf(port_str, sizeof(port_str), "%d", vserial->config.server_port);
    
    int status = getaddrinfo(vserial->config.server_ip, port_str, &hints, &res);
    if (status != 0) {
        fprintf(stderr, "无法解析主机 %s: %s\n", 
                vserial->config.server_ip, 
                gai_strerror(status));
        return -1;
    }
    
    vserial->socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (vserial->socket_fd < 0) {
        perror("创建socket失败");
        freeaddrinfo(res);
        return -1;
    }
    
    // 设置socket选项
    int opt = 1;
    setsockopt(vserial->socket_fd, SOL_SOCKET, SO_KEEPALIVE, &opt, sizeof(opt));
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr = ((struct sockaddr_in*)res->ai_addr)->sin_addr;
    server_addr.sin_port = htons(vserial->config.server_port);
    
    freeaddrinfo(res);
    
    if (connect(vserial->socket_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("连接服务器失败");
        close(vserial->socket_fd);
        return -1;
    }
    
    printf("✓ 已连接到服务器: %s:%d\n", 
           vserial->config.server_ip, 
           vserial->config.server_port);
    
    vserial->connected = true;
    return 0;
}

// PTY到网络线程
void* pty_to_network_thread(void *arg) {
    VirtualSerial *vserial = (VirtualSerial *)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read, bytes_sent;
    fd_set readfds;
    struct timeval timeout;
    
    printf("→ PTY到网络转发线程启动\n");
    
    while (vserial->running) {
        if (!vserial->connected) {
            usleep(100000);
            continue;
        }
        
        FD_ZERO(&readfds);
        FD_SET(vserial->pty_master, &readfds);
        
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;  // 100ms超时
        
        int ret = select(vserial->pty_master + 1, &readfds, NULL, NULL, &timeout);
        
        if (ret > 0 && FD_ISSET(vserial->pty_master, &readfds)) {
            bytes_read = read(vserial->pty_master, buffer, sizeof(buffer));
            if (bytes_read > 0) {
                pthread_mutex_lock(&vserial->mutex);
                
                if (vserial->config.debug) {
                    printf("← 从应用程序收到 %ld 字节: ", bytes_read);
                    for (int i = 0; i < bytes_read && i < 16; i++) {
                        printf("%02X ", (unsigned char)buffer[i]);
                    }
                    printf("\n");
                }
                
                bytes_sent = send(vserial->socket_fd, buffer, bytes_read, 0);
                if (bytes_sent < 0) {
                    if (errno == EPIPE || errno == ECONNRESET) {
                        printf("! 网络连接断开\n");
                        vserial->connected = false;
                        close(vserial->socket_fd);
                    } else {
                        perror("发送到网络失败");
                    }
                } else if (vserial->config.debug) {
                    printf("→ 已转发 %ld 字节到网络\n", bytes_sent);
                }
                
                pthread_mutex_unlock(&vserial->mutex);
            } else if (bytes_read < 0 && errno != EAGAIN) {
                perror("读取PTY失败");
                break;
            }
        } else if (ret < 0) {
            perror("select失败");
            break;
        }
    }
    
    printf("← PTY到网络转发线程结束\n");
    return NULL;
}

// 网络到PTY线程
void* network_to_pty_thread(void *arg) {
    VirtualSerial *vserial = (VirtualSerial *)arg;
    char buffer[BUFFER_SIZE];
    ssize_t bytes_received, bytes_written;
    fd_set readfds;
    struct timeval timeout;
    
    printf("← 网络到PTY转发线程启动\n");
    
    while (vserial->running) {
        if (!vserial->connected) {
            // 尝试重连
            pthread_mutex_lock(&vserial->mutex);
            int retry = 0;
            while (retry < vserial->config.retry_count && !vserial->connected) {
                if (connect_to_server(vserial) == 0) {
                    printf("✓ 重连成功\n");
                    break;
                }
                retry++;
                sleep(vserial->config.reconnect_delay);
            }
            pthread_mutex_unlock(&vserial->mutex);
            
            if (!vserial->connected) {
                sleep(1);
                continue;
            }
        }
        
        FD_ZERO(&readfds);
        FD_SET(vserial->socket_fd, &readfds);
        
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        int ret = select(vserial->socket_fd + 1, &readfds, NULL, NULL, &timeout);
        
        if (ret > 0 && FD_ISSET(vserial->socket_fd, &readfds)) {
            bytes_received = recv(vserial->socket_fd, buffer, sizeof(buffer), 0);
            if (bytes_received > 0) {
                pthread_mutex_lock(&vserial->mutex);
                
                if (vserial->config.debug) {
                    printf("← 从网络收到 %ld 字节: ", bytes_received);
                    for (int i = 0; i < bytes_received && i < 16; i++) {
                        printf("%02X ", (unsigned char)buffer[i]);
                    }
                    printf("\n");
                }
                
                bytes_written = write(vserial->pty_master, buffer, bytes_received);
                if (bytes_written < 0) {
                    perror("写入PTY失败");
                } else if (vserial->config.debug) {
                    printf("→ 已转发 %ld 字节到应用程序\n", bytes_written);
                }
                
                pthread_mutex_unlock(&vserial->mutex);
            } else if (bytes_received == 0) {
                printf("! 服务器关闭连接\n");
                vserial->connected = false;
                close(vserial->socket_fd);
            } else if (bytes_received < 0 && errno != EAGAIN) {
                perror("从网络接收失败");
                vserial->connected = false;
                close(vserial->socket_fd);
            }
        } else if (ret < 0) {
            perror("select失败");
            break;
        }
    }
    
    printf("→ 网络到PTY转发线程结束\n");
    return NULL;
}

// 初始化配置
void init_config(Config *config, int argc, char *argv[]) {
    // 设置默认值
    strcpy(config->virtual_port, DEFAULT_VIRTUAL_PORT);
    strcpy(config->server_ip, DEFAULT_SERVER_IP);
    config->server_port = DEFAULT_SERVER_PORT;
    config->baudrate = 115200;
    config->data_bits = 8;
    config->stop_bits = 1;
    config->parity = 'N';
    config->flow_control = false;
    config->debug = false;
    config->retry_count = MAX_RETRY_COUNT;
    config->reconnect_delay = 5;
    
    // 解析命令行参数
    int opt;
    while ((opt = getopt(argc, argv, "p:s:b:d:t:y:fvh")) != -1) {
        switch (opt) {
            case 'p':
                safe_strcpy(config->virtual_port, optarg, sizeof(config->virtual_port));
                break;
            case 's': {
                char *colon = strchr(optarg, ':');
                if (colon) {
                    *colon = '\0';
                    safe_strcpy(config->server_ip, optarg, sizeof(config->server_ip));
                    config->server_port = atoi(colon + 1);
                } else {
                    safe_strcpy(config->server_ip, optarg, sizeof(config->server_ip));
                }
                break;
            }
            case 'b':
                config->baudrate = atoi(optarg);
                break;
            case 'd':
                config->data_bits = atoi(optarg);
                break;
            case 't':
                config->stop_bits = atoi(optarg);
                break;
            case 'y':
                config->parity = optarg[0];
                break;
            case 'f':
                config->flow_control = true;
                break;
            case 'v':
                config->debug = true;
                break;
            case 'h':
            default:
                printf("虚拟串口转发器 - 版本 1.0\n");
                printf("用法: %s [选项]\n", argv[0]);
                printf("\n选项:\n");
                printf("  -p PATH         虚拟串口路径 (默认: %s)\n", DEFAULT_VIRTUAL_PORT);
                printf("  -s IP:PORT      服务器地址 (默认: %s:%d)\n", 
                       DEFAULT_SERVER_IP, DEFAULT_SERVER_PORT);
                printf("  -b RATE         波特率 (默认: 115200)\n");
                printf("  -d BITS         数据位 (5-8, 默认: 8)\n");
                printf("  -t BITS         停止位 (1-2, 默认: 1)\n");
                printf("  -y TYPE         校验位 (N,O,E, 默认: N)\n");
                printf("  -f              启用流控\n");
                printf("  -v              调试模式\n");
                printf("  -h              显示此帮助信息\n");
                printf("\n示例:\n");
                printf("  %s -p /tmp/vcom0 -s 192.168.1.100:8080\n", argv[0]);
                printf("  %s -p /tmp/myserial -s localhost:9000 -b 9600 -v\n", argv[0]);
                exit(0);
        }
    }
}

// 清理资源
void cleanup(VirtualSerial *vserial) {
    if (!vserial) return;
    
    printf("正在清理资源...\n");
    
    vserial->running = false;
    
    // 等待线程结束
    if (vserial->thread_pty2net) {
        pthread_join(vserial->thread_pty2net, NULL);
    }
    if (vserial->thread_net2pty) {
        pthread_join(vserial->thread_net2pty, NULL);
    }
    
    // 关闭文件描述符
    if (vserial->pty_master >= 0) {
        close(vserial->pty_master);
    }
    if (vserial->pty_slave >= 0) {
        close(vserial->pty_slave);
    }
    if (vserial->socket_fd >= 0) {
        close(vserial->socket_fd);
    }
    
    // 删除符号链接
    unlink(vserial->config.virtual_port);
    
    // 销毁互斥锁
    pthread_mutex_destroy(&vserial->mutex);
    
    printf("资源清理完成\n");
}

int main(int argc, char *argv[]) {
    VirtualSerial vserial = {0};
    g_vserial = &vserial;
    
    // 初始化配置
    init_config(&vserial.config, argc, argv);
    
    // 初始化互斥锁
    if (pthread_mutex_init(&vserial.mutex, NULL) != 0) {
        perror("初始化互斥锁失败");
        return 1;
    }
    
    // 设置信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // 忽略SIGPIPE信号
    
    printf("===========================================\n");
    printf("虚拟串口转发器启动\n");
    printf("===========================================\n");
    
    // 创建虚拟串口
    if (create_virtual_serial(&vserial) < 0) {
        fprintf(stderr, "创建虚拟串口失败\n");
        return 1;
    }
    
    // 连接到服务器
    if (connect_to_server(&vserial) < 0) {
        fprintf(stderr, "连接服务器失败\n");
        cleanup(&vserial);
        return 1;
    }
    
    // 设置运行标志
    vserial.running = true;
    
    // 创建转发线程
    if (pthread_create(&vserial.thread_pty2net, NULL, pty_to_network_thread, &vserial) != 0) {
        perror("创建PTY到网络线程失败");
        cleanup(&vserial);
        return 1;
    }
    
    if (pthread_create(&vserial.thread_net2pty, NULL, network_to_pty_thread, &vserial) != 0) {
        perror("创建网络到PTY线程失败");
        cleanup(&vserial);
        return 1;
    }
    
    printf("\n===========================================\n");
    printf("虚拟串口已准备就绪！\n");
    printf("\n应用程序可以使用以下命令访问虚拟串口：\n");
    printf("  串口设备: %s\n", vserial.config.virtual_port);
    printf("  服务器: %s:%d\n", vserial.config.server_ip, vserial.config.server_port);
    printf("  波特率: %d\n", vserial.config.baudrate);
    printf("\n测试命令示例：\n");
    printf("  echo 'Hello World' > %s     # 发送数据到服务器\n", vserial.config.virtual_port);
    printf("  cat %s &                    # 从服务器接收数据\n", vserial.config.virtual_port);
    printf("  stty -F %s                  # 查看串口设置\n", vserial.config.virtual_port);
    printf("===========================================\n\n");
    
    printf("转发服务运行中...\n");
    printf("按Ctrl+C停止服务\n");
    printf("-------------------------------------------\n");
    
    // 主循环
    while (vserial.running) {
        sleep(1);
    }
    
    // 清理资源
    cleanup(&vserial);
    
    printf("服务已停止\n");
    return 0;
}
