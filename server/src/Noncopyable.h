#ifndef __NONECOPYABLE_H__
#define __NONECOPYABLE_H__

class Noncopyable
  {
   protected:
      Noncopyable() = default;
      ~Noncopyable() = default;
      Noncopyable(const Noncopyable&) = delete;
      Noncopyable& operator=(const Noncopyable&) = delete;
};

#endif