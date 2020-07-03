#include "InetAddress.h"

#include "SocketsOps.h"

#include <netdb.h>
#include <netinet/in.h>

using std::string;

//     /* Structure describing an Internet socket address.  */
//     struct sockaddr_in {
//         sa_family_t    sin_family; /* address family: AF_INET */
//         uint16_t       sin_port;   /* port in network byte order */
//         struct in_addr sin_addr;   /* internet address */
//     };

//     /* Internet address. */
//     typedef uint32_t in_addr_t;
//     struct in_addr {
//         in_addr_t       s_addr;     /* address in network byte order */
//     };

//     struct sockaddr {
// 　    　unsigned short sa_family; /* address family, AF_xxx */
// 　    　char sa_data[14]; /* 14 bytes of protocol address */
// 　 　};

InetAddress::InetAddress(uint16_t port, bool loopbackOnly)
{
    memset(&addr_,0 , sizeof addr_);
    addr_.sin_family = AF_INET;
    in_addr_t ip = loopbackOnly ? INADDR_LOOPBACK : INADDR_ANY;
    addr_.sin_addr.s_addr = htobe32(ip);
    addr_.sin_port = htobe16(port);
}

InetAddress::InetAddress(StringArg ip, uint16_t port)
{
    memset(&addr_,0 , sizeof addr_);
    addr_.sin_family = AF_INET;
    addr_.sin_port = htobe16(port);
    inet_pton(AF_INET, ip.c_str(), &addr_.sin_addr);
}

string InetAddress::toIpPort() const
{
  char buf[64] = "";
  struct sockaddr* addr = (struct sockaddr*)&addr_;
  if (addr->sa_family == AF_INET)
  {
    const struct sockaddr_in* addr4 = (const struct sockaddr_in*)addr;
    ::inet_ntop(AF_INET, &addr4->sin_addr, buf, static_cast<socklen_t>(sizeof buf));
  }
  size_t end = strlen(buf);
  const struct sockaddr_in* addr4 = (const struct sockaddr_in*)addr;
  uint16_t port = be16toh(addr4->sin_port);
  snprintf(buf+end, sizeof(buf)-end, ":%u", port);
  return buf;
}

string InetAddress::toIp() const
{
  char buf[64] = "";
  struct sockaddr* addr = (struct sockaddr*)&addr_;
  if (addr->sa_family == AF_INET)
  {
    const struct sockaddr_in* addr4 = (const struct sockaddr_in*)addr;
    ::inet_ntop(AF_INET, &addr4->sin_addr, buf, static_cast<socklen_t>(sizeof buf));
  }
  return buf;
}

uint32_t InetAddress::ipNetEndian() const
{
  return addr_.sin_addr.s_addr;
}

uint16_t InetAddress::toPort() const
{
  return be16toh(portNetEndian());
}

static __thread char t_resolveBuffer[64 * 1024];

bool InetAddress::resolve(StringArg hostname, InetAddress* out)
{
  struct hostent hent;
  struct hostent* he = NULL;
  int herrno = 0;
  memset(&hent,0 , sizeof(hent));

  int ret = gethostbyname_r(hostname.c_str(), &hent, t_resolveBuffer, sizeof t_resolveBuffer, &he, &herrno);
  if (ret == 0 && he != NULL)
  {
    out->addr_.sin_addr = *reinterpret_cast<struct in_addr*>(he->h_addr);
    return true;
  }
  else
  {
    if (ret)
    {
      #ifdef DEBUG
      printf("InetAddress::resolve\n");
      #endif
    }
    return false;
  }
}