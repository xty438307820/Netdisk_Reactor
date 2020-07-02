# 遇到的问题

- (多线程信号处理问题)服务端往断开的客户端write时触发SIGPIPE信号导致服务端进程退出------solve:忽略SIGPIPE信号