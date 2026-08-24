# LMonitor系统架构


## 1. 总体架构


LMonitor采用Agent-Server架构。

            Web Browser
                 |
                 |
              HTTP API
                 |
         +-------+-------+
         |  HTTP Server  |
         +-------+-------+
                 |
         +-------+-------+
         |    Server     |
         |
         | Reactor
         | ThreadPool
         | MetricsStore
         | AlertManager
         |
         +-------+-------+
                 |
                TCP
                 |
         +-------+-------+
         |     Agent     |
         +-------+-------+
                 |
    +------------+------------+
    |                         |
  CPU采集                 Memory采集

  Disk采集                Network采集

  Process采集             System信息



---

## 2. Agent模块


Agent运行在被监控服务器。


主要职责：

- 周期采集系统指标；
- 构造监控数据；
- TCP发送到Server。


核心模块：

MonitorAgent
 |
 +-- CpuCollector
 |
 +-- MemoryCollector
 |
 +-- DiskCollector
 |
 +-- NetworkCollector
 |
 +-- ProcessCollector



---

## 3. Server模块


Server负责：

- TCP连接管理；
- 数据解析；
- 指标存储；
- 告警检测；
- HTTP接口。


核心模块：
TcpServer
 |
 +-- Reactor
 |
 +-- ThreadPool
 |
 +-- MetricsStore
 |
 +-- AlertManager
 |
 +-- HttpServer



---

## 4. 网络模型


Server采用Reactor模型：

             Main Thread

                  |

               epoll

                  |

    +-------------+-------------+

    |             |             |

 Client1      Client2       Client3


                  |

            Worker ThreadPool



---

## 5. 数据流程

Linux Kernel
  |
Collectors
|
SystemMetrics
  |
TCP Frame
  |
Server
  |
MetricsStore
  |
HTTP API
  |
Dashboard
