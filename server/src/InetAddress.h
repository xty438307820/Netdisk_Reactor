#ifndef __INETADDRESS_H__
#define __INETADDRESS_H__

#include "StringPiece.h"

#include <netinet/in.h>

class InetAddress
{
 public:
  explicit InetAddress(uint16_t port = 0, bool loopbackOnly = false);

  InetAddress(StringArg ip, uint16_t port);

  explicit InetAddress(const struct sockaddr_in& addr)
    : addr_(addr)
  { }

  sa_family_t family() const { return addr_.sin_family; }
  std::string toIp() const;
  std::string toIpPort() const;
  uint16_t toPort() const;

  const struct sockaddr* getSockAddr() const { return (struct sockaddr*)&addr_; }
  void setSockAddrInet(const struct sockaddr_in& addr) { addr_ = addr; }

  uint32_t ipNetEndian() const;
  uint16_t portNetEndian() const { return addr_.sin_port; }

  // resolve hostname to IP address, not changing port or sin_family
  // return true on success.
  // thread safe
  static bool resolve(StringArg hostname, InetAddress* result);
  // static std::vector<InetAddress> resolveAll(const char* hostname, uint16_t port = 0);

 private:
    struct sockaddr_in addr_;
};

#endif
