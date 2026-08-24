# LMonitor部署说明


## 1. 编译


```bash
mkdir build

cd build

cmake ..

cmake --build .

2. 安装目录
/opt/lmonitor

├── bin

├── config

├── logs

└── web

3. systemd配置
Server:
lmonitor-server.service
Agent:
lmonitor-agent.service
4. 启动
systemctl start lmonitor-server

systemctl start lmonitor-agent
5. 查看状态
systemctl status lmonitor-server

systemctl status lmonitor-agent

---


