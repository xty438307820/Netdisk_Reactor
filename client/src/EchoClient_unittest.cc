#include "TcpClient.h"

#include "EventLoop.h"
#include "InetAddress.h"
#include "Socket.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/stat.h>

using std::placeholders::_1;
using std::placeholders::_2;
using std::placeholders::_3;

class EchoClient;
std::vector<std::unique_ptr<EchoClient>> clients;
int current = 0;

//打印绿色字体
void printGreen(const char*);

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
        printf("\033[1;32mLogin success.........\033[0m\n");
        conn->setStateC(TcpConnection::StateC_Login_Success);
        printGreen(conn->username.c_str());
      }
      else{
        printf("Your username or password error, back to home page\n");
        printf("Enter 0 to register, 1 to login:\n");
        conn->setStateC(TcpConnection::StateC_Init);
      }
    }
    else if(conn->getStateC() == conn->StateC_Print){
      if(msg.size() > 0) printf("%s\n",msg.c_str());
      conn->setStateC(conn->StateC_Login_Success);
      printGreen(conn->username.c_str());
    }
    else if(conn->getStateC() == conn->StateC_Mkdir){
      if(*(int*)msg.c_str() != 0) printf("mkdir: cannot create directory: File exists\n");
      conn->setStateC(conn->StateC_Login_Success);
      printGreen(conn->username.c_str());
    }
    else if(conn->getStateC() == conn->StateC_Remove){
      if(*(int*)msg.c_str() != 0) printf("remove: cannot remove: No such file or directory\n");
      conn->setStateC(conn->StateC_Login_Success);
      printGreen(conn->username.c_str());
    }
    else if(conn->getStateC() == conn->StateC_Cd){
      if(*(int*)msg.c_str() != 0) printf("cd: cannot cd: No such directory\n");
      conn->setStateC(conn->StateC_Login_Success);
      printGreen(conn->username.c_str());
    }
    else if(conn->getStateC() == conn->StateC_Begin_Puts){
      if(*(int*)msg.c_str() != 0){
        printf("puts: cannot puts: File exists\n");
        conn->setStateC(conn->StateC_Login_Success);
        printGreen(conn->username.c_str());
      }
      else{
        struct stat statbuf;
        stat((char*)conn->filename.c_str(),&statbuf);
        conn->file_size = statbuf.st_size;
        conn->setStateC(conn->StateC_Puts);
        conn->send(string((char*)&statbuf.st_size,sizeof(long)));
      }
    }
    else if(conn->getStateC() == conn->StateC_Puts){
      if(*(int*)msg.c_str() == 0){
        printf("begin puts......\n");
        int fd = open(conn->filename.c_str(),O_RDONLY);
        char buf[8192] = {0};
        int len;

        long curLen = 0;

        while( (len = read(fd, buf, sizeof(buf)-1) ) != 0){
          buf[len] = 0;
          conn->send(string(buf));
          curLen += len;
          printf("%5.2f%%\r",(double)curLen/conn->file_size );
          fflush(stdout);
        }
        printf("100.00%%\n");
        close(fd);

        conn->setStateC(conn->StateC_Login_Success);
        printGreen(conn->username.c_str());
      }
    }
    else if(conn->getStateC() == conn->StateC_Begin_Gets ){
      if(*(long*)msg.c_str() < 0){
        printf("gets: cannot gets: No such file\n");
        conn->setStateC(conn->StateC_Login_Success);
        printGreen(conn->username.c_str());
      }
      else{
        printf("begin gets......\n");
        conn->file_size = *(long*)msg.c_str();
        conn->cur_size = 0;
        conn->setStateC(conn->StateC_Gets);
        conn->so_file.reset( new Socket( open(conn->filename.c_str(),O_CREAT|O_WRONLY, 0644) ) );
      }
    }
    else if(conn->getStateC() == conn->StateC_Gets){
      conn->cur_size += msg.size();
      write(conn->so_file->fd(), msg.c_str(), msg.size());
      printf("%5.2f%%\r",(double)conn->cur_size/conn->file_size );
      fflush(stdout);
      if(conn->cur_size == conn->file_size){
        conn->so_file.reset();
        conn->setStateC(conn->StateC_Login_Success);

        int ret = 0;
        conn->send(string((char*)&ret,sizeof(int)));
        
        printf("100.00%%\n");
        printGreen(conn->username.c_str());
      }
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

