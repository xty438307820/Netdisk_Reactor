#include "SocketsOps.h"

#include <endian.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>  // snprintf
#include <sys/socket.h>
#include <sys/uio.h>  // readv
#include <unistd.h>
#include <string.h>

int sockets::createNonblockingOrDie(sa_family_t family)
{
  int sockfd = ::socket(family, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, IPPROTO_TCP);
  if (sockfd < 0)
  {
    printf("sockets::createNonblockingOrDie\n");
  }
  return sockfd;
}

void sockets::bindOrDie(int sockfd, const struct sockaddr* addr)
{
  int ret = ::bind(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in)));
  if (ret < 0)
  {
    printf("sockets::bindOrDie\n");
  }
}

void sockets::listenOrDie(int sockfd)
{
  int ret = ::listen(sockfd, SOMAXCONN);
  if (ret < 0)
  {
    printf("sockets::listenOrDie\n");
  }
}

int sockets::accept(int sockfd, struct sockaddr_in* addr)
{
  socklen_t addrlen = static_cast<socklen_t>(sizeof *addr);
  int connfd = ::accept4(sockfd, (struct sockaddr*)addr,
                         &addrlen, SOCK_NONBLOCK | SOCK_CLOEXEC);
  if (connfd < 0)
  {
    int savedErrno = errno;
    printf("Socket::accept\n");
    printf("unknown error of ::accept\n");
  }
  return connfd;
}

int sockets::connect(int sockfd, const struct sockaddr* addr)
{
  return ::connect(sockfd, addr, static_cast<socklen_t>(sizeof(struct sockaddr_in)));
}

ssize_t sockets::read(int sockfd, void *buf, size_t count)
{
  return ::read(sockfd, buf, count);
}

ssize_t sockets::readv(int sockfd, const struct iovec *iov, int iovcnt)
{
  return ::readv(sockfd, iov, iovcnt);
}

ssize_t sockets::write(int sockfd, const void *buf, size_t count)
{
  return ::write(sockfd, buf, count);
}

void sockets::close(int sockfd)
{
  if (::close(sockfd) < 0)
  {
    printf("sockets::close\n");
  }
}

void sockets::shutdownWrite(int sockfd)
{
  if (::shutdown(sockfd, SHUT_WR) < 0)
  {
    printf("sockets::shutdownWrite\n");
  }
}

struct sockaddr_in sockets::getLocalAddr(int sockfd)
{
  struct sockaddr_in localaddr;
  memset(&localaddr,0 , sizeof localaddr);
  socklen_t addrlen = static_cast<socklen_t>(sizeof localaddr);
  if (::getsockname(sockfd, (struct sockaddr*)&localaddr, &addrlen) < 0)
  {
    printf("sockets::getLocalAddr\n");
  }
  return localaddr;
}

int sockets::getSocketError(int sockfd)
{
  int optval;
  socklen_t optlen = static_cast<socklen_t>(sizeof optval);

  if (::getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &optval, &optlen) < 0)
  {
    return errno;
  }
  else
  {
    return optval;
  }
}
