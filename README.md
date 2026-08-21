# LMonitor

Linux 分布式服务器监控系统


## 1. 项目简介

LMonitor 是一个基于 C++17 开发的 Linux 分布式服务器监控系统。

该系统用于实时采集 Linux 主机运行状态，并通过 TCP 网络通信将监控数据发送至服务器端。

服务器端负责数据接收、异步处理、状态管理、历史数据存储，并通过 HTTP Dashboard 提供可视化监控界面。


系统主要由以下模块组成：

- Agent 监控采集模块
- Reactor 网络服务器模块
- Metrics 数据存储模块
- HTTP Dashboard 可视化模块
- Alert 告警管理模块


---

# 2. 系统架构


```
                 +----------------+
                 |   Linux主机    |
                 |                |
                 | LMonitor Agent |
                 +-------+--------+
                         |
                         |
                     TCP通信
                         |
                         v

              +---------------------+
              | Reactor TCP Server  |
              |                     |
              | epoll EventLoop     |
              +----------+----------+
                         |
              +----------+----------+
              |                     |
              v                     v

       +-------------+       +-------------+
       | Worker Pool |       | AlertManager|
       +-------------+       +-------------+

              |
              v

       +----------------+
       | Metrics Store  |
       | History Store  |
       +----------------+

              |
              v

       +----------------+
       | HTTP Dashboard |
       +----------------+

```


系统工作流程：

1. Agent 周期性采集 Linux 系统指标
2. 通过 TCP 协议发送监控数据
3. Reactor Server 接收网络数据
4. Worker线程池异步处理数据
5. 存储实时指标和历史数据
6. Alert模块进行异常检测
7. Web Dashboard展示监控状态


---

# 3. 主要功能


## 3.1 Linux系统指标采集


支持采集：

- CPU 使用率
- 内存使用率
- 磁盘使用情况
- 网络流量
- Load Average
- 进程资源信息
- 系统运行时间


---

## 3.2 高性能网络服务器


服务器端采用 Reactor 网络模型。


主要技术：

- Linux epoll
- 非阻塞 TCP
- EventLoop事件循环
- Reactor设计模式
- Worker线程池


处理流程：

```
I/O线程

    |

    v

Reactor EventLoop

    |

    v

Worker线程池

    |

    v

指标解析与存储

```


---

## 3.3 指标存储


实现：

- 主机实时状态存储
- 最新指标缓存
- 历史监控数据保存
- 主机在线状态管理


支持：

- ONLINE
- STALE
- OFFLINE


三种状态管理。


---

## 3.4 告警管理系统


实现基于阈值的监控告警。


支持：

- CPU异常检测
- 告警状态管理
- FIRING状态
- 恢复检测


示例：

```
CPU使用率 > 80%

        |

        v

    触发告警

        |

        v

     FIRING状态

        |

        v

      恢复正常

```


---

## 3.5 Web Dashboard


提供基于 HTTP 的可视化监控界面。


支持：

- 主机列表展示
- 实时CPU/内存状态
- 历史趋势曲线
- 进程信息查看
- 当前告警展示


---

# 4. 技术栈


## 编程语言

- C++17


## 操作系统

- Linux


## 网络通信

- TCP Socket
- epoll
- Reactor模式


## 构建工具

- CMake


## 并发模型

- std::thread
- Thread Pool


## Web

- HTTP Server
- HTML
- JavaScript


---

# 5. 项目目录结构


```
LMonitor

├── include
│
├── src
│
├── web
│   └── index.html
│
├── CMakeLists.txt
│
└── README.md

```


主要模块：


```
agent

    Linux系统指标采集


reactor

    EventLoop
    Channel
    Acceptor


network

    TCP网络通信


server

    监控服务器


store

    数据存储


alert

    告警检测


http

    Web接口服务

```


---

# 6. 编译方法


环境要求：

- Linux系统
- C++17编译器
- CMake


执行：


```bash
mkdir build

cd build

cmake ..

cmake --build .
```


---

# 7. 运行方法


启动服务器：


```bash
./lmonitor_server
```


启动Agent：


```bash
./lmonitor_agent
```


访问Dashboard：


```text
http://server-ip:8080
```


---

# 8. HTTP接口


## 获取主机列表


请求：

```
GET /api/hosts
```


## 获取告警信息


请求：

```
GET /api/alerts
```


返回示例：


```json
[
 {
  "hostname":"server-ubuntu2404",
  "metric":"cpu",
  "state":"FIRING",
  "current_value":99.6,
  "threshold":80
 }
]
```


---

# 9. 项目特点


## 高性能网络模型

采用 Reactor + epoll 模型，实现高并发 TCP 数据接收。


## 异步任务处理

通过 Worker线程池降低网络线程阻塞。


## 模块化设计

系统采用模块化结构：

- 数据采集
- 网络通信
- 数据处理
- 存储管理
- Web展示
- 告警管理


## 可扩展性

支持后续扩展：

- WebSocket实时推送
- 数据库存储
- 多服务器集群监控
- 用户认证系统
- 配置文件管理


---

# 10. 后续优化方向


计划增加：

- 配置文件支持
- MySQL/Redis持久化
- WebSocket实时通信
- 多Agent管理
- 用户权限认证


---

# 作者

Ambition47

Linux / C++系统开发项目

