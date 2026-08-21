# LMonitor

## Linux 分布式服务器监控系统


LMonitor 是一个基于 **C++17** 开发的 Linux 分布式服务器监控平台。

系统采用 **Agent + Server** 架构，实现对 Linux 主机运行状态的实时采集、网络传输、数据处理、历史存储以及 Web 可视化展示。


项目主要面向 Linux 服务器监控场景，实现了一套轻量级、高性能、可扩展的监控系统。



---

# 1. 项目介绍


LMonitor 由两个核心部分组成：


## Agent 监控采集端

运行在被监控 Linux 主机上。


主要负责：

- CPU 信息采集
- 内存信息采集
- 磁盘信息采集
- 网络流量采集
- Load Average采集
- 进程资源采集
- 主机状态采集


采集完成后，通过 TCP 长连接发送至 Server。


---


## Server 监控服务端


负责：

- TCP连接管理
- 数据解析
- 指标存储
- 历史数据管理
- 主机状态维护
- 告警检测
- HTTP接口提供
- Web Dashboard展示



---

# 2. 系统架构


                Linux Host


          +----------------+
          | LMonitor Agent |
          +-------+--------+
                  |
                  |
             TCP Protocol
                  |
                  v


    +--------------------------------+
    |       Reactor TCP Server       |
    |                                |
    | epoll + EventLoop + Channel    |
    +---------------+----------------+

                    |
      +-------------+-------------+
      |                           |

      v                           v
+----------------+          +----------------+
| Worker Thread  |          | Alert Manager  |
| Pool           |          |                |
+-------+--------+          +----------------+
        |
        |
        v
+----------------+
| Metrics Store  |
| History Store  |
+-------+--------+
        |
        |
        v
+----------------+
| HTTP Dashboard |
+----------------+



---

# 3. 核心功能


## 3.1 Linux系统监控


支持采集：

| 指标 | 支持 |
|-|-|
| CPU使用率 | ✅ |
| 内存使用率 | ✅ |
| 磁盘使用率 | ✅ |
| 网络流量 | ✅ |
| Load Average | ✅ |
| 进程信息 | ✅ |
| 系统运行时间 | ✅ |



---


# 3.2 Reactor高性能网络模型


Server端采用 Reactor 架构。


核心技术：

- Linux epoll
- 非阻塞 Socket
- EventLoop事件循环
- Channel事件管理
- Acceptor连接管理



网络处理流程：

Client
  |
TCP Connection
  |
Acceptor
  |
EventLoop
  |
TcpConnection
  |
Worker Thread Pool
  |
Metrics Processing




优势：

- IO线程不阻塞
- 支持大量连接
- 网络处理与业务处理分离



---


# 3.3 多线程任务处理


系统采用：
Reactor线程
负责：
- socket事件
- 数据接收
    |

    v
Worker线程池
负责：
- 数据解析
- 指标处理
- 数据存储



避免：

- 网络线程阻塞
- 大量计算影响连接处理



---


# 3.4 数据存储


实现：

## 实时数据存储

保存当前主机状态：

- CPU
- Memory
- Load
- Disk
- Network


## 历史数据存储

保存：

- 历史采样数据
- CPU变化趋势
- Memory变化趋势



## 主机状态管理


支持：
ONLINE
STALE
OFFLINE




---


# 3.5 告警系统


实现基于阈值的实时告警。


支持：

- CPU异常检测
- 告警状态管理
- 告警恢复


示例：

CPU > 80%
    |

    v

 FIRING

CPU恢复
    |

    v

RECOVERED



---


# 3.6 Web Dashboard


提供浏览器可视化界面。


支持：

- 主机列表
- CPU实时状态
- Memory实时状态
- 历史趋势曲线
- 进程查看
- 告警展示



运行效果：

> 后续添加 Dashboard 截图



---

# 4. 技术栈


## 开发语言

- C++17


## 操作系统

- Linux Ubuntu


## 网络通信

- TCP Socket
- epoll


## 网络模型

- Reactor Pattern


## 并发

- std::thread
- Thread Pool


## 构建

- CMake


## Web

- HTTP Server
- HTML
- JavaScript



---

# 5. 项目目录

LMonitor
├── include
│
├── src
│
├── web
│   └── index.html
│
├── docs
│   └── images
│
├── CMakeLists.txt
│
└── README.md




模块说明：
agent
系统指标采集

collector
CPU/Memory/Disk/Network采集

reactor
EventLoop
Channel
Acceptor

network
TCP通信

server
服务端核心逻辑

store
数据存储

alert
告警系统

http
Dashboard接口



---

# 6. 编译


环境要求：

- Linux
- GCC
- CMake


执行：


```bash
mkdir build

cd build

cmake ..

cmake --build .

---
# 7. 运行
启动 Server：
./lmonitor_server
启动 Agent：
./lmonitor_agent
访问：
http://server-ip:8080

---
# 8. HTTP API
获取主机列表
GET /api/hosts
获取历史数据
GET /api/hosts/{hostname}/history
获取告警
GET /api/alerts

---

#9.项目技术亮点

1. 基于epoll的Reactor网络模型
实现高性能事件驱动服务器。
2. Agent-Server分布式架构
实现监控采集与数据处理解耦。
3. 多线程异步处理
采用线程池提高任务处理能力。
4. 模块化设计
包含：
- 网络模块
- 存储模块
- 告警模块
- HTTP模块
5. 可扩展架构
未来可扩展：
- WebSocket实时推送
- 数据库存储
- 多Agent管理
- 用户认证
- 配置中心

---

#10. 后续优化方向
计划：
- 增加配置文件管理
- 增加MySQL/Redis存储
- 增加WebSocket通信
- 支持多服务器集群监控
- 增加权限认证


作者
Ambition47
Linux / C++ 后端系统开发项目
