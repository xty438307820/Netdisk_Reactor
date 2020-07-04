#include "TcpServer.h"

#include "Thread.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "config.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h> 

int numThreads = 3;

using namespace std::placeholders;

int sqlTableChange(char*);  //用户注册插入mysql

class EchoServer
{
 public:
  EchoServer(EventLoop* loop, const InetAddress& listenAddr)
    : loop_(loop),
      server_(loop, listenAddr, "EchoServer")
  {
    server_.setConnectionCallback(
        std::bind(&EchoServer::onConnection, this, _1));
    server_.setMessageCallback(
        std::bind(&EchoServer::onMessage, this, _1, _2, _3));
    server_.setThreadNum(numThreads);
  }

  void start()
  {
    server_.start();
  }
  // void stop();

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    printf("%s -> %s is %s\n",conn->peerAddress().toIpPort().c_str(),conn->localAddress().toIpPort().c_str(),(conn->connected() ? "UP" : "DOWN"));
    printf("%s\n",conn->getTcpInfoString().c_str());
  }

  void onMessage(const TcpConnectionPtr& conn, const string& msg, Timestamp time)
  {
    //printf("%s recv %lu bytes at %s\n",conn->name().c_str(),msg.size(),time.toString().c_str());
    if(conn->getStateC() == TcpConnection::StateC_Init){
      //printf("%s\n",msg.c_str());
      handleRegister(conn,msg);
    }
    
  }

  void handleRegister(const TcpConnectionPtr& conn,const string& msg){
    size_t uname_len = *(int*)(msg.c_str());
    size_t salt_len = *(int*)(msg.c_str()+4);
    size_t secret_len = *(int*)(msg.c_str()+8);

    string username = msg.substr(12, uname_len);
    string salt = msg.substr(12+uname_len, salt_len);
    string secret = msg.substr(12+uname_len+salt_len, secret_len);
    
    char sql[256]={0};
    sprintf(sql,"insert into UserInfo(UserName,Salt,PassWord) values('%s','%s','%s')",username.c_str(),salt.c_str(),secret.c_str());
    int ret = sqlTableChange(sql);
    if(0 == ret){
      string path = WORK_DIR + username;
      mkdir(path.c_str(),0755);
    }
    conn->send(string((char*)&ret,4));
  }

  EventLoop* loop_;
  TcpServer server_;
};

int main(int argc, char* argv[])
{
  printf("pid = %d, tid = %d\n",getpid(),CurrentThread::tid());
  printf("sizeof TcpConnection = %lu\n",sizeof(TcpConnection));
  signal(SIGPIPE,SIG_IGN);//忽略SIGPIPE信号
  if (argc > 1)
  {
    numThreads = atoi(argv[1]);
  }
  EventLoop loop;
  InetAddress listenAddr(2000, false);
  EchoServer server(&loop, listenAddr);

  server.start();

  loop.loop();
}

