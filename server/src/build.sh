g++ EchoServer_unittest.cc TcpServer.cc TcpConnection.cc EventLoop.cc Channel.cc EPollPoller.cc \
SocketsOps.cc Timestamp.cc CurrentThread.cc Socket.cc InetAddress.cc Thread.cc EventLoopThread.cc \
EventLoopThreadPool.cc Acceptor.cc CountDownLatch.cc Buffer.cc MysqlDbHelper.cc dirOp.cc -lpthread -lmysqlclient