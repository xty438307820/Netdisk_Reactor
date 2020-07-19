#ifndef __TCPCONNECTION_H__
#define __TCPCONNECTION_H__

#include "Noncopyable.h"
#include "StringPiece.h"
#include "Buffer.h"
#include "InetAddress.h"
#include "Timestamp.h"

#include <memory>
#include <functional>

struct tcp_info;

class Channel;
class EventLoop;
class Socket;

class TcpConnection : Noncopyable,
                      public std::enable_shared_from_this<TcpConnection>
{
 public:
 typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;

 typedef std::function<void (const TcpConnectionPtr&)> ConnectionCallback;
 typedef std::function<void (const TcpConnectionPtr&)> WriteCompleteCallback;
 typedef std::function<void (const TcpConnectionPtr&, size_t)> HighWaterMarkCallback;
 typedef std::function<void (const TcpConnectionPtr&)> CloseCallback;
 typedef std::function<void (const TcpConnectionPtr&,
                              const string&,
                              Timestamp)> MessageCallback;

  // 构造TcpConnection,由TcpServer使用,用户不能使用
  TcpConnection(EventLoop* loop,
                const string& name,
                int sockfd,
                const InetAddress& localAddr,
                const InetAddress& peerAddr);
  ~TcpConnection();

  EventLoop* getLoop() const { return loop_; }
  const string& name() const { return name_; }
  const InetAddress& localAddress() const { return localAddr_; }
  const InetAddress& peerAddress() const { return peerAddr_; }
  bool connected() const { return state_ == kConnected; }
  bool disconnected() const { return state_ == kDisconnected; }
  // return true if success.
  bool getTcpInfo(struct tcp_info*) const;
  string getTcpInfoString() const;

  // void send(string&& message); // C++11
  void send(const void* message, int len);
  void send(const StringPiece& message);
  // void send(Buffer&& message); // C++11
  //void send(Buffer* message);  // this one will swap data
  void shutdown(); // NOT thread safe, no simultaneous calling
  // void shutdownAndForceCloseAfter(double seconds); // NOT thread safe, no simultaneous calling
  void forceClose();
  void setTcpNoDelay(bool on);
  // reading or not
  void startRead();
  void stopRead();
  bool isReading() const { return reading_; }; // NOT thread safe, may race with start/stopReadInLoop

  void setConnectionCallback(const ConnectionCallback& cb)
  { connectionCallback_ = cb; }

  void setMessageCallback(const MessageCallback& cb)
  { messageCallback_ = cb; }

  void setWriteCompleteCallback(const WriteCompleteCallback& cb)
  { writeCompleteCallback_ = cb; }

  void setHighWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
  { highWaterMarkCallback_ = cb; highWaterMark_ = highWaterMark; }

  /// Advanced interface
  Buffer* inputBuffer()
  { return &inputBuffer_; }

  Buffer* outputBuffer()
  { return &outputBuffer_; }

  /// Internal use only.
  void setCloseCallback(const CloseCallback& cb)
  { closeCallback_ = cb; }

  // called when TcpServer accepts a new connection
  void connectEstablished();   // should be called only once
  // called when TcpServer has removed me from its map
  void connectDestroyed();  // should be called only once

  //客户端状态,根据状态对接收的消息执行不同行为
  enum StateC {  
    StateC_Init,  //刚连接的状态,接受键盘输入进行相应操作:0->登录,1->注册
    StateC_Registering,  //正在注册
    StateC_Logining_Step1,  //登录步骤一
    StateC_Logining_Step2,  //登录步骤二
    StateC_Login_Success,  //登录成功
    StateC_Print,  //该状态直接将接收的消息打印
    StateC_Mkdir,  //该状态等待mkdir命令的返回结果
    StateC_Remove,  //该状态等待remove命令的返回结果
    StateC_Cd,  //该状态等待cd命令的返回结果
    StateC_Begin_Puts,  //该状态等待服务端返回是否有重复文件
    StateC_Puts,  //客户端puts文件
    StateC_Begin_Gets,  //该状态等待服务端返回是否有该文件
    StateC_Gets  //客户端gets文件
  };
  StateC getStateC() { return statec_;}
  void setStateC(StateC s) { statec_ = s; }

  string username;  //保存当前用户名
  string filename;  //暂存puts或gets的文件名
  std::unique_ptr<Socket> so_file;  //暂存文件
  long file_size;  //暂存文件大小,puts和gets用
  long cur_size;  //目前已经gets数据

 private:
  enum StateE { kDisconnected, kConnecting, kConnected, kDisconnecting };
  void handleRead(Timestamp receiveTime);
  void handleWrite();
  void handleClose();
  void handleError();
  // void sendInLoop(string&& message);
  void sendInLoop(const StringPiece& message);
  void sendInLoop(const void* message, size_t len);
  void shutdownInLoop();
  // void shutdownAndForceCloseInLoop(double seconds);
  void forceCloseInLoop();
  void setState(StateE s) { state_ = s; }
  const char* stateToString() const;
  void startReadInLoop();
  void stopReadInLoop();

  //处理键盘输入,分发给处理用户事件
  void handleKeyboardInput();
  
  //处理用户事件
  void handleRegister();
  void handleLogin();

  EventLoop* loop_;
  const string name_;
  StateE state_;
  mutable StateC statec_;
  bool reading_;

  std::unique_ptr<Socket> socket_;
  std::unique_ptr<Channel> channel_;//用于TCP连接的channel
  std::unique_ptr<Channel> keyboard_channel_;//用于键盘输入的channel
  const InetAddress localAddr_;
  const InetAddress peerAddr_;
  ConnectionCallback connectionCallback_;
  MessageCallback messageCallback_;
  WriteCompleteCallback writeCompleteCallback_;
  HighWaterMarkCallback highWaterMarkCallback_;
  CloseCallback closeCallback_;
  size_t highWaterMark_;
  Buffer inputBuffer_;
  Buffer outputBuffer_;
};

typedef std::shared_ptr<TcpConnection> TcpConnectionPtr;
void defaultConnectionCallback(const TcpConnectionPtr& conn);
void defaultMessageCallback(const TcpConnectionPtr&,const string& buf,Timestamp);

#endif
