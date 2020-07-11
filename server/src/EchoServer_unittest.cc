#include "TcpServer.h"

#include "Thread.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "config.h"
#include "Socket.h"

#include <utility>

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h> 
#include <fcntl.h>

int numThreads = 3;

using namespace std::placeholders;

//工具函数
int sqlTableChange(char*);  //用户注册插入mysql
int sqlSingleSelect(char* sql,char* buf);  //sql单值查询,查询结果填入buf
std::string myls(const char*);
int myMkdir(const char*);
int myRemove(const char*);
int testOpenDir(const char*);
int fileExist(const char*);

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
      if(msg == "0") conn->setStateC(TcpConnection::StateC_Registering);
      else if(msg == "1") conn->setStateC(TcpConnection::StateC_Logining_Step1);
    }
    else if(conn->getStateC() == TcpConnection::StateC_Registering){
      conn->setStateC(TcpConnection::StateC_Init);
      handleRegister(conn,msg);
    }
    else if(conn->getStateC() == TcpConnection::StateC_Logining_Step1){
      char sql[256] = {0};
      char salt[32] = {0};
      conn->username_ = msg;
      sprintf(sql,"select Salt from UserInfo where UserName='%s'",msg.c_str());
      sqlSingleSelect(sql,salt);
      if(strlen(salt) == 0) conn->setStateC(TcpConnection::StateC_Init);
      else conn->setStateC(TcpConnection::StateC_Logining_Step2);
      conn->send(string(salt));
    }
    else if(conn->getStateC() == TcpConnection::StateC_Logining_Step2){
      char sql[256] = {0};
      char buf[128] = {0};
      string secret(msg);
      sprintf(sql,"select PassWord from UserInfo where UserName='%s'",conn->username_.c_str());
      sqlSingleSelect(sql,buf);
      if(secret == string(buf)){
        int ret = 0;
        conn->setStateC(TcpConnection::StateC_Login_Success);
        conn->absolutePath_ = WORK_DIR + conn->username_;
        conn->relativePath_ = "/";
        conn->send(string((char*)&ret,sizeof(int)));
      }
      else{
        int ret = -1;
        conn->setStateC(TcpConnection::StateC_Init);
        conn->send(string((char*)&ret,sizeof(int)));
      }
    }
    else if(conn->getStateC() == TcpConnection::StateC_Login_Success){
      //前两个命令为无参命令
      if(msg == "pwd"){
        conn->send(conn->relativePath_);
      }
      else if(msg == "ls"){
        conn->send( myls((conn->absolutePath_+conn->relativePath_).c_str()) );
      }
      //后面的命令为带一个参数的命令
      else{
        int start = 0;
        string cmd;
        string parm;
        while(start < msg.size() && msg[start] == ' ') start++;
        while(start < msg.size() && msg[start] != ' ') cmd.push_back(msg[start++]);
        while(start < msg.size() && msg[start] == ' ') start++;
        while(start < msg.size() && msg[start] != ' ') parm.push_back(msg[start++]);

        if(cmd == "mkdir"){
          int ret = myMkdir((conn->absolutePath_ + conn->relativePath_ + parm).c_str());
          conn->send(string((char*)&ret,sizeof(int)));
        }
        else if(cmd == "remove"){
          int ret = myRemove((conn->absolutePath_ + conn->relativePath_ + parm).c_str());
          conn->send(string((char*)&ret,sizeof(int)));
        }
        else if(cmd == "cd"){
          int ret;
          if(parm == ""){
            conn->relativePath_ = "/";
            ret = 0;
          }
          else if(parm == ".") ret = 0;
          else if(parm == ".."){
            if(conn->relativePath_ != "/"){
              conn->relativePath_.pop_back();
              while(conn->relativePath_.back() != '/') conn->relativePath_.pop_back();
            }
            ret = 0;
          }
          else{
            ret = testOpenDir((conn->absolutePath_ + conn->relativePath_ + parm).c_str());
            if(ret == 0){
              conn->relativePath_ += parm;
              conn->relativePath_ += "/";
            }
          }
          conn->send(string((char*)&ret,sizeof(int)));
        }
        else if(cmd == "puts"){
          int ret = fileExist((conn->absolutePath_ + conn->relativePath_ + parm).c_str()) ? 0 : -1;
          if(ret == 0){
            conn->setStateC(conn->StateC_Begin_Puts);
            conn->so_file.reset(new Socket(open( (conn->absolutePath_ + conn->relativePath_ + parm).c_str(), O_CREAT|O_WRONLY, 0644)));
          }
          conn->send(string((char*)&ret,sizeof(int)));
        }

      }
    }
    else if(conn->getStateC() == conn->StateC_Begin_Puts){
      conn->file_size = *(long*)msg.c_str();
      conn->setStateC(conn->StateC_Puts);
      int ret = 0;
      conn->send(string((char*)&ret,sizeof(int)));
    }
    else if(conn->getStateC() == conn->StateC_Puts){
      conn->file_size -= msg.size();
      write(conn->so_file->fd(), msg.c_str(), msg.size());
      if(conn->file_size == 0){
        conn->so_file.reset();
        conn->setStateC(conn->StateC_Login_Success);
      }
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

