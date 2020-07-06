#include "TcpConnection.h"

#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"
#include "SocketsOps.h"

#include <errno.h>

using std::placeholders::_1;

void defaultConnectionCallback(const TcpConnectionPtr& conn)
{
  #ifdef DEBUG
  printf("%s -> %s is %s\n",conn->localAddress().toIpPort().c_str(),conn->peerAddress().toIpPort().c_str(),(conn->connected() ? "UP" : "DOWN"));
  #endif
  // do not call conn->forceClose(), because some users want to register message callback only.
}

void defaultMessageCallback(const TcpConnectionPtr&,
                                        const string& msg,
                                        Timestamp)
{
}

TcpConnection::TcpConnection(EventLoop* loop,
                             const string& nameArg,
                             int sockfd,
                             const InetAddress& localAddr,
                             const InetAddress& peerAddr)
  : loop_(loop),
    name_(nameArg),
    state_(kConnecting),
    statec_(StateC_Init),
    reading_(true),
    socket_(new Socket(sockfd)),
    channel_(new Channel(loop, sockfd)),
    keyboard_channel_(new Channel(loop, STDIN_FILENO)),
    localAddr_(localAddr),
    peerAddr_(peerAddr),
    highWaterMark_(64*1024*1024)
{
  channel_->setReadCallback(
      std::bind(&TcpConnection::handleRead, this, _1));
  channel_->setWriteCallback(
      std::bind(&TcpConnection::handleWrite, this));
  channel_->setCloseCallback(
      std::bind(&TcpConnection::handleClose, this));
  channel_->setErrorCallback(
      std::bind(&TcpConnection::handleError, this));
  keyboard_channel_->setReadCallback(std::bind(&TcpConnection::handleKeyboardInput, this));
  #ifdef DEBUG
  printf("TcpConnection::ctor[%s] at %p fd=%d\n",name_.c_str(),this,sockfd);
  #endif
  socket_->setKeepAlive(true);
}

TcpConnection::~TcpConnection()
{
  #ifdef DEBUG
  printf("TcpConnection::dtor[%s] at %p fd=%d state=%s\n",name_.c_str(),this,channel_->fd(),stateToString());
  #endif
  assert(state_ == kDisconnected);
}

bool TcpConnection::getTcpInfo(struct tcp_info* tcpi) const
{
  return socket_->getTcpInfo(tcpi);
}

string TcpConnection::getTcpInfoString() const
{
  char buf[1024];
  buf[0] = '\0';
  socket_->getTcpInfoString(buf, sizeof buf);
  return buf;
}

void TcpConnection::send(const void* data, int len)
{
  send(StringPiece(static_cast<const char*>(data), len));
}

void TcpConnection::send(const StringPiece& message)
{
  Buffer buf;
  buf.append(message);
  uint32_t be32 = htobe32(message.size());
  buf.prepend(&be32,sizeof(be32));
  if (state_ == kConnected)
  {
    if (loop_->isInLoopThread())
    {
      sendInLoop(buf.peek(),buf.readableBytes());
      buf.retrieveAll();
    }
    else
    {
      void (TcpConnection::*fp)(const StringPiece& message) = &TcpConnection::sendInLoop;
      loop_->runInLoop(
          std::bind(fp,
                    this,     // FIXME
                    buf.retrieveAllAsString()));
                    //std::forward<string>(message)));
    }
  }
}

void TcpConnection::sendInLoop(const StringPiece& message)
{
  sendInLoop(message.data(), message.size());
}

void TcpConnection::sendInLoop(const void* data, size_t len)
{
  loop_->assertInLoopThread();
  ssize_t nwrote = 0;
  size_t remaining = len;
  bool faultError = false;
  if (state_ == kDisconnected)
  {
    #ifdef DEBUG
    printf("disconnected, give up writing\n");
    #endif
    return;
  }
  // if no thing in output queue, try writing directly
  if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
  {
    nwrote = sockets::write(channel_->fd(), data, len);
    if (nwrote >= 0)
    {
      remaining = len - nwrote;
      if (remaining == 0 && writeCompleteCallback_)
      {
        loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
      }
    }
    else // nwrote < 0
    {
      nwrote = 0;
      if (errno != EWOULDBLOCK)
      {
        #ifdef DEBUG
        printf("TcpConnection::sendInLoop\n");
        #endif
        if (errno == EPIPE || errno == ECONNRESET) // FIXME: any others?
        {
          faultError = true;
        }
      }
    }
  }

  assert(remaining <= len);
  if (!faultError && remaining > 0)
  {
    size_t oldLen = outputBuffer_.readableBytes();
    if (oldLen + remaining >= highWaterMark_
        && oldLen < highWaterMark_
        && highWaterMarkCallback_)
    {
      loop_->queueInLoop(std::bind(highWaterMarkCallback_, shared_from_this(), oldLen + remaining));
    }
    outputBuffer_.append(static_cast<const char*>(data)+nwrote, remaining);
    if (!channel_->isWriting())
    {
      channel_->enableWriting();
    }
  }
}

void TcpConnection::shutdown()
{
  // FIXME: use compare and swap
  if (state_ == kConnected)
  {
    setState(kDisconnecting);
    // FIXME: shared_from_this()?
    loop_->runInLoop(std::bind(&TcpConnection::shutdownInLoop, this));
  }
}

void TcpConnection::shutdownInLoop()
{
  loop_->assertInLoopThread();
  if (!channel_->isWriting())
  {
    // we are not writing
    socket_->shutdownWrite();
  }
}

void TcpConnection::forceClose()
{
  // FIXME: use compare and swap
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    setState(kDisconnecting);
    loop_->queueInLoop(std::bind(&TcpConnection::forceCloseInLoop, shared_from_this()));
  }
}

void TcpConnection::forceCloseInLoop()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected || state_ == kDisconnecting)
  {
    // as if we received 0 byte in handleRead();
    handleClose();
  }
}

const char* TcpConnection::stateToString() const
{
  switch (state_)
  {
    case kDisconnected:
      return "kDisconnected";
    case kConnecting:
      return "kConnecting";
    case kConnected:
      return "kConnected";
    case kDisconnecting:
      return "kDisconnecting";
    default:
      return "unknown state";
  }
}

void TcpConnection::setTcpNoDelay(bool on)
{
  socket_->setTcpNoDelay(on);
}

void TcpConnection::startRead()
{
  loop_->runInLoop(std::bind(&TcpConnection::startReadInLoop, this));
}

void TcpConnection::startReadInLoop()
{
  loop_->assertInLoopThread();
  if (!reading_ || !channel_->isReading())
  {
    channel_->enableReading();
    reading_ = true;
  }
}

void TcpConnection::stopRead()
{
  loop_->runInLoop(std::bind(&TcpConnection::stopReadInLoop, this));
}

void TcpConnection::stopReadInLoop()
{
  loop_->assertInLoopThread();
  if (reading_ || channel_->isReading())
  {
    channel_->disableReading();
    reading_ = false;
  }
}

void TcpConnection::connectEstablished()
{
  loop_->assertInLoopThread();
  assert(state_ == kConnecting);
  setState(kConnected);
  channel_->tie(shared_from_this());
  channel_->enableReading();

  keyboard_channel_->enableReading();

  connectionCallback_(shared_from_this());
}

void TcpConnection::connectDestroyed()
{
  loop_->assertInLoopThread();
  if (state_ == kConnected)
  {
    setState(kDisconnected);
    channel_->disableAll();

    connectionCallback_(shared_from_this());
  }
  channel_->remove();
}

void TcpConnection::handleRead(Timestamp receiveTime)
{
  loop_->assertInLoopThread();
  int savedErrno = 0;
  ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
  if (n > 0)
  {
    while(inputBuffer_.readableBytes() >= 4){
      const void* data = inputBuffer_.peek();
      uint32_t  be32 = *static_cast<const uint32_t*>(data);
      uint32_t  len = be32toh(be32);
      if(inputBuffer_.readableBytes() >= 4 + len){
        inputBuffer_.retrieve(4);
        std::string msg(inputBuffer_.peek(),len);
        messageCallback_(shared_from_this(),msg ,receiveTime);
        inputBuffer_.retrieve(len);
      }
      else break;
    }
  }
  else if (n == 0)
  {
    handleClose();
  }
  else
  {
    errno = savedErrno;
    #ifdef DEBUG
    printf("TcpConnection::handleRead\n");
    #endif
    handleError();
  }
}

void TcpConnection::handleWrite()
{
  loop_->assertInLoopThread();
  if (channel_->isWriting())
  {
    ssize_t n = sockets::write(channel_->fd(),
                               outputBuffer_.peek(),
                               outputBuffer_.readableBytes());
    if (n > 0)
    {
      outputBuffer_.retrieve(n);
      if (outputBuffer_.readableBytes() == 0)
      {
        channel_->disableWriting();
        if (writeCompleteCallback_)
        {
          loop_->queueInLoop(std::bind(writeCompleteCallback_, shared_from_this()));
        }
        if (state_ == kDisconnecting)
        {
          shutdownInLoop();
        }
      }
    }
    else
    {
      #ifdef DEBUG
      printf("TcpConnection::handleWrite\n");
      #endif
      // if (state_ == kDisconnecting)
      // {
      //   shutdownInLoop();
      // }
    }
  }
  else
  {
    #ifdef DEBUG
    printf("Connection fd = %d is down, no more writing\n",channel_->fd());
    #endif
  }
}

void TcpConnection::handleClose()
{
  loop_->assertInLoopThread();
  #ifdef DEBUG
  printf("fd = %d state = %s\n",channel_->fd(),stateToString());
  #endif
  assert(state_ == kConnected || state_ == kDisconnecting);
  // we don't close fd, leave it to dtor, so we can find leaks easily.
  setState(kDisconnected);
  channel_->disableAll();

  TcpConnectionPtr guardThis(shared_from_this());
  connectionCallback_(guardThis);
  // must be the last line
  closeCallback_(guardThis);
}

void TcpConnection::handleError()
{
  int err = sockets::getSocketError(channel_->fd());
  #ifdef DEBUG
  printf("TcpConnection::handleError [%s] - SO_ERROR = %d\n",name_.c_str(),err);
  #endif
}

//以下为根据情况处理用户键盘输入
void encryptStr(char* p);

void TcpConnection::handleRegister(){
    char username[128]={0};
    char salt[32]={0};
    char* pwd;  //明文密码
    char* secret;  //密文密码

    printf("Registering.........\n");
    printf("Enter username:");
    //读取键盘输入账号
    fgets(username,sizeof(username),stdin);
    username[strlen(username)-1] = 0;
    //读取键盘输入密码
    pwd = getpass("Enter password:");
    encryptStr(salt);
    secret = crypt(pwd,salt);
    //将注册信息打包
    //sprintf(buf,"%lu%lu%lu%s%s%s",strlen(username),strlen(salt),strlen(secret)-12,username,salt,secret+12);
    int uname_len = strlen(username);
    int salt_len = strlen(salt);
    int secret_len = strlen(secret) - 12;

    string package_ = string((char*)&uname_len,4) + string((char*)&salt_len,4) + \
    string((char*)&secret_len,4) + username + salt + (secret + 12);
    
    send(package_);
}

void TcpConnection::handleLogin(){
  char username[128]={0};
  
  printf("Logining.........\n");
  printf("Enter username:");
  //读取键盘输入账号
  fgets(username,sizeof(username),stdin);
  username[strlen(username)-1] = 0;
  send(string(username));
}

void TcpConnection::handleKeyboardInput(){
  char buf[128]={0};
  if(statec_ == StateC_Init){  //刚启动程序状态
    fgets(buf,sizeof(buf),stdin);
    buf[strlen(buf)-1] = 0;  //去掉换行符
    if(strcmp(buf,"0") == 0){  //注册
      send(string(buf));
      statec_ = StateC_Registering;
      handleRegister();
    }
    else if(strcmp(buf,"1") == 0){
      send(string(buf));
      statec_ = StateC_Logining_Step1;
      handleLogin();
    }
    else printf("Enter 0 to register, 1 to login:\n");
  }
  else if(statec_ == StateC_Login_Success){
    fgets(buf,sizeof(buf),stdin);
    buf[strlen(buf)-1] = 0;  //去掉换行符
    string sbuf = string(buf);

    //这两个命令不带参数
    if(sbuf == "pwd" || sbuf == "ls") statec_ = StateC_Print;
    //以下命令带一个参数
    else{
      int start = 0;
      int len = sbuf.size();
      string cmd;
      string parm;
      while(start < sbuf.size() && sbuf[start] == ' ') start++;
      while(start < sbuf.size() && sbuf[start] != ' ') cmd.push_back(sbuf[start++]);
      while(start < sbuf.size() && sbuf[start] == ' ') start++;
      while(start < sbuf.size() && sbuf[start] != ' ') parm.push_back(sbuf[start++]);
      
      if(cmd == "mkdir"){
        if(parm == ""){
          printf("mkdir: missing operand\n");
          return;
        }
        else statec_ = StateC_Mkdir;
      }

    }
    
    send(sbuf);
  }

}