# LMonitor

## Linux服务器监控系统（C++17）


## 一、项目简介

LMonitor 是一个基于 C++17 开发的轻量级 Linux 服务器监控系统。

项目采用 Agent + Server 分布式架构设计：

- Agent 负责采集服务器运行状态信息；
- Server 负责接收数据、处理数据、保存历史状态，并提供 HTTP 查询接口；
- Web 页面通过 HTTP API 获取监控数据，实现服务器状态可视化。

项目主要用于学习和实践 Linux 系统编程、网络通信、高并发服务器设计以及工程化部署。


---

## 二、系统架构

                浏览器
                   |
                   |
                 HTTP
                   |
          +--------+--------+
          |    HTTP Server  |
          +--------+--------+
                   |
          +--------+--------+
          |      Server     |
          |                 |
          | Reactor模型     |
          | ThreadPool      |
          | Metrics管理     |
          | Alert管理       |
          +--------+--------+
                   |
                   |
                 TCP
                   |
          +--------+--------+
          |      Agent      |
          +--------+--------+
                   |
    +--------------+--------------+
    |                             |
  CPU采集                    内存采集

  磁盘采集                   网络采集

  进程采集                   系统信息


---

# 三、主要功能


## 1. Linux系统资源采集

Agent运行在目标服务器上，周期性采集系统指标。


支持采集：

- CPU使用率
- 内存使用率
- 磁盘使用率
- 网络流量
- Load Average
- 系统运行时间
- 进程信息


采集数据示例：

```json
{
    "hostname":"server-ubuntu2404",
    "cpu":2.13,
    "memory":21.55,
    "load1":2.16,
    "uptime":2511295
}


2. TCP通信模块
Agent与Server之间采用TCP Socket进行通信。
实现：
- TCP长连接
- 数据传输
- 自定义数据协议
- 客户端自动连接管理
通信流程：
Agent

  |
  |
 TCP Socket

  |
  |

Server
3. Reactor网络模型
Server采用 Reactor 事件驱动模型。
核心设计：
- epoll I/O多路复用
- 非阻塞网络通信
- 事件循环处理
- 高效连接管理
结构：
                Reactor

                   |

          +--------+--------+

          |                 |

       Accept            Read/Write

          |

       Worker ThreadPool


4. 多线程任务处理
Server内部采用线程池模型。
功能：
- 主线程负责网络事件处理；
- Worker线程负责业务任务执行；
- 降低阻塞，提高并发处理能力。
线程模型：
                 Main Thread

                      |

                  Reactor

                      |

          +-----------+-----------+

          |           |           |

       Worker1    Worker2     Worker3


5. HTTP监控接口
Server提供HTTP API接口。
支持查询：
获取服务器列表
接口：GET /api/hosts
返回：
[
 {
  "hostname":"server-ubuntu2404",
  "cpu":2.13,
  "memory":21.55,
  "load1":2.16
 }
]

获取历史监控数据
接口：
GET /api/hosts/{hostname}/history
返回：
[
 {
  "timestamp":1787542346176,
  "cpu":3.29,
  "memory":21.53
 }
]

获取告警信息
接口：GET /api/alerts
返回：
[
 {
  "hostname":"server-ubuntu2404",
  "metric":"cpu",
  "state":"FIRING",
  "current_value":91.73,
  "threshold":80,
  "message":"CPU usage exceeded warning threshold"
 }
]

四、告警管理系统
系统支持资源异常检测。
目前支持：
CPU告警
默认：
CPU > 80%
触发：
CPU usage exceeded warning threshold
内存告警
默认：
Memory > 90%
告警状态：
- NORMAL
- FIRING
五、日志系统
实现独立Logger模块。
支持：
- 多线程安全日志
- 时间戳记录
- 线程ID记录
- 日志文件输出
- 日志自动轮转


日志示例：
2026-08-24 11:17:26

[INFO]

[thread=140419938883392]

Metrics stored:

host=server-ubuntu2404

cpu=2.13%

memory=21.55%


六、配置管理
系统支持配置文件加载。
配置文件：config/lmonitor.conf
示例：

[server]

tcp_port=9000

http_port=8081



[worker]

threads=4

queue_size=1024



[history]

max_size=120



[alert]

cpu_threshold=80

memory_threshold=90



[agent]

server_ip=127.0.0.1

server_port=9000

interval=1.0


七、系统部署
项目支持Linux生产环境部署。
部署目录：
/opt/lmonitor


├── bin

│   ├── lmonitor_server

│   └── lmonitor_agent


├── config

│   └── lmonitor.conf


└── logs

    ├── lmonitor_server.log

    └── lmonitor_agent.log


八、systemd服务管理
支持Linux服务化运行。
Server服务：
lmonitor-server.service
Agent服务：
lmonitor-agent.service
支持：
- 开机自动启动
- 异常自动重启
- 后台运行
查看状态：
systemctl status lmonitor-server

systemctl status lmonitor-agent
启动：
systemctl start lmonitor-server

systemctl start lmonitor-agent


九、项目目录结构
LMonitor

├── include

│   ├── agent

│   ├── server

│   ├── network

│   ├── http

│   ├── alert

│   ├── collector

│   └── model


├── src

│   ├── agent

│   ├── server

│   ├── network

│   ├── http

│   ├── alert

│   ├── collector

│   └── log


├── config

│   └── lmonitor.conf


├── web

│   └── index.html


├── docs


├── build


└── README.md


十、技术栈
类型	技术
编程语言	C++17
操作系统	Linux Ubuntu
构建工具	CMake
网络通信	TCP Socket
IO模型	epoll
网络架构	Reactor
并发模型	ThreadPool
数据接口	HTTP Server
部署方式	systemd
版本管理	Git


十一、编译方式
创建build目录：
mkdir build

cd build
生成：
cmake ..
编译：
cmake --build .


十二、运行方式
启动Server：
./lmonitor_server
启动Agent：
./lmonitor_agent


十三、项目亮点总结
Linux系统编程
- 基于Linux接口实现系统资源采集；
- 使用/proc文件系统获取系统状态。
网络编程
- 实现TCP通信；
- 基于epoll实现事件驱动服务器。
高并发设计
- Reactor网络模型；
- ThreadPool任务处理模型。
工程化能力
- CMake工程管理；
- Git版本管理；
- systemd服务部署；
- 配置文件驱动。
后端服务开发
- HTTP接口设计；
- 数据存储管理；
- 告警模块设计。


十四、运行环境
测试环境：
Ubuntu 24.04 LTS
编译环境：
g++ 11+

CMake 3.22+

C++17



十五、作者:HaoQi Miao

方向：Linux C++ 后端开发
