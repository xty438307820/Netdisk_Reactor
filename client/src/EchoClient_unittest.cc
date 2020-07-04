#include "TcpClient.h"

#include "EventLoop.h"
#include "InetAddress.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

class EchoClient;
std::vector<std::unique_ptr<EchoClient>> clients;
int current = 0;

class EchoClient : Noncopyable
{
 public:
  EchoClient(EventLoop* loop, const InetAddress& listenAddr, const string& id)
    : loop_(loop),
      client_(loop, listenAddr, "EchoClient"+id)
  {
    client_.setConnectionCallback(
        std::bind(&EchoClient::onConnection, this, _1));
    client_.setMessageCallback(
        std::bind(&EchoClient::onMessage, this, _1, _2, _3));
    //client_.enableRetry();
  }

  void connect()
  {
    client_.connect();
  }
  // void stop();

 private:
  void onConnection(const TcpConnectionPtr& conn)
  {
    #ifdef DEBUG
    printf("%s -> %s is %s \n",conn->localAddress().toIpPort().c_str(),conn->peerAddress().toIpPort().c_str(),(conn->connected() ? "UP" : "DOWN"));
    #endif

    if (conn->connected())
    {
      ++current;
      if (static_cast<size_t>(current) < clients.size())
      {
        clients[current]->connect();
      }
      #ifdef DEBUG
      printf("*** connected\n");
      #endif
    }
  }

  void onMessage(const TcpConnectionPtr& conn, const string& msg, Timestamp time)
  {
    #ifdef DEBUG
    printf("%s recv %lu bytes at %s\n",conn->name().c_str(),msg.size(),time.toString().c_str());
    printf("%s\n",msg.c_str());
    #endif
    if(conn->getStateC() == conn->StateC_Registering){
      if(*(int*)msg.c_str() == 0){
        printf("Register ok.........\n");
      }
      else{
        printf("Username exist, register fail, back to home page\n");
      }
      printf("Enter 0 to register, 1 to login:\n");
      conn->setStateC(conn->StateC_Init);
    }
    else if(conn->getStateC() == conn->StateC_Logining_Step1){
      char *pwd;
      char *secret;
      string salt(msg);
      pwd = getpass("Enter password:");
      if(salt.size() == 0){
        printf("Your username or password error, back to home page\n");
        printf("Enter 0 to register, 1 to login:\n");
        conn->setStateC(TcpConnection::StateC_Init);
      }
      else{
        secret = crypt(pwd,salt.c_str());
        conn->setStateC(TcpConnection::StateC_Logining_Step2);
        conn->send(string(secret+12));
      }
    }
    else if(conn->getStateC() == conn->StateC_Logining_Step2){
      if(*(int*)msg.c_str() == 0){
        printf("Login success.........\n");
        conn->setStateC(TcpConnection::StateC_Login_Success);
      }
      else{
        printf("Your username or password error, back to home page\n");
        printf("Enter 0 to register, 1 to login:\n");
        conn->setStateC(TcpConnection::StateC_Init);
      }
    }
    else if(conn->getStateC() == conn->StateC_Print){
      printf("%s\n",msg.c_str());
      conn->setStateC(conn->StateC_Login_Success);
    }

  }

  EventLoop* loop_;
  TcpClient client_;
};

int main(int argc, char* argv[])
{
  #ifdef DEBUG
  printf("pid = %d, tid = %d\n",getpid(),CurrentThread::tid());
  #endif
  signal(SIGPIPE,SIG_IGN);//忽略SIGPIPE信号
  if (argc > 1)
  {
    EventLoop loop;
    InetAddress serverAddr(argv[1], 2000);

    int n = 1;
    if (argc > 2)
    {
      n = atoi(argv[2]);
    }

    clients.reserve(n);
    for (int i = 0; i < n; ++i)
    {
      char buf[32];
      snprintf(buf, sizeof buf, "%d", i+1);
      clients.emplace_back(new EchoClient(&loop, serverAddr, buf));
    }
    printf("--------------------------------\n");
    printf("|Welcome to netdisk            |\n");
    printf("--------------------------------\n");
    printf("Enter 0 to register, 1 to login:\n");
    clients[current]->connect();
    loop.loop();
  }
  else
  {
    printf("Usage: %s host_ip [current#]\n", argv[0]);
  }
}

