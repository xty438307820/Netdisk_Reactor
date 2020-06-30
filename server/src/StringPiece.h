#ifndef __STRINGPIECE_H__
#define __STRINGPIECE_H__

#include <string.h>
#include <string>

// For passing C-style string argument to a function.
class StringArg
{
 public:
  StringArg(const char* str)
    : str_(str)
  { }

  StringArg(const std::string& str)
    : str_(str.c_str())
  { }

  const char* c_str() const { return str_; }

 private:
  const char* str_;
};

class StringPiece {
 private:
  const char*   ptr_;
  int           length_;

 public:
  StringPiece()
    : ptr_(NULL), length_(0) { }
  StringPiece(const char* str)
    : ptr_(str), length_(static_cast<int>(strlen(ptr_))) { }
  StringPiece(const unsigned char* str)
    : ptr_(reinterpret_cast<const char*>(str)),
      length_(static_cast<int>(strlen(ptr_))) { }
  StringPiece(const std::string& str)
    : ptr_(str.data()), length_(static_cast<int>(str.size())) { }
  StringPiece(const char* offset, int len)
    : ptr_(offset), length_(len) { }

  const char* data() const { return ptr_; }
  int size() const { return length_; }
  bool empty() const { return length_ == 0; }
  const char* begin() const { return ptr_; }
  const char* end() const { return ptr_ + length_; }

  void clear() { ptr_ = NULL; length_ = 0; }
  void set(const char* buffer, int len) { ptr_ = buffer; length_ = len; }
  void set(const char* str) {
    ptr_ = str;
    length_ = static_cast<int>(strlen(str));
  }
  void set(const void* buffer, int len) {
    ptr_ = reinterpret_cast<const char*>(buffer);
    length_ = len;
  }

  char operator[](int i) const { return ptr_[i]; }

  void remove_prefix(int n) {
    ptr_ += n;
    length_ -= n;
  }

  void remove_suffix(int n) {
    length_ -= n;
  }

  bool operator==(const StringPiece& x) const {
    return ((length_ == x.length_) &&
            (memcmp(ptr_, x.ptr_, length_) == 0));
  }
  bool operator!=(const StringPiece& x) const {
    return !(*this == x);
  }

  int compare(const StringPiece& x) const {
    int r = memcmp(ptr_, x.ptr_, length_ < x.length_ ? length_ : x.length_);
    if (r == 0) {
      if (length_ < x.length_) r = -1;
      else if (length_ > x.length_) r = +1;
    }
    return r;
  }

  std::string as_string() const {
    return std::string(data(), size());
  }

  void CopyToString(std::string* target) const {
    target->assign(ptr_, length_);
  }

  bool starts_with(const StringPiece& x) const {
    return ((length_ >= x.length_) && (memcmp(ptr_, x.ptr_, x.length_) == 0));
  }
};

#endif
