#include <unistd.h>
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>

#include <sys/param.h>
#include <sys/mount.h>
// linux
// #include <sys/statfs.h>

namespace oscalls {

  template <typename RetType, class... ArgTypes>
  static inline RetType call(const char* name, RetType (*func)(ArgTypes...), const RetType& error_value, const ArgTypes&... args) {
    RetType ret = func(args...);

    if (ret != error_value) {
      return ret;
    }
    
    std::ostringstream oss;
    oss << errno << " " << name << " os call error";
    throw std::runtime_error(oss.str());
  }

  void close(int fd) {
    call("close", ::close, -1, fd);
  }
}

// open is declared: int open(const char *pathname, int flags, ...);
namespace oscalls::wrappers {
  static inline int open(const char* pathname, int flags) {
    return ::open(pathname, flags);
  }
  static inline int open(const char* pathname, int flags, mode_t mode) {
    return ::open(pathname, flags, mode);
  }
}

namespace oscalls {

  int open(const char* pathname, int flags) {
    return call("open", wrappers::open, -1, pathname, flags);
  }

  int open(const char* pathname, int flags, mode_t mode) {
    return call("open", wrappers::open, -1, pathname, flags, mode);
  }

  size_t read(int fd, void* buf, size_t count) {
    return call("read", ::read, static_cast<ssize_t>(-1), fd, buf, count);
  }

  size_t pread(int fd, void* buf, size_t count, off_t offset) {
    return call("pread", ::pread, static_cast<ssize_t>(-1), fd, buf, count, offset);
  }

  size_t pwrite(int fd, const void* buf, size_t count, off_t offset) {
    return call("pwrite", ::pwrite, static_cast<ssize_t>(-1), fd, buf, count, offset);
  }

  size_t writev(int fd, const iovec* iov, int iovcnt) {
    return call("writev", ::writev, static_cast<ssize_t>(-1), fd, iov, iovcnt);
  }

  void ftruncate(int fd, off_t length) {
    call("ftruncate", ::ftruncate, -1, fd, length);
  }

  void fstat(int fd, struct stat* buf) {
    call("fstat", ::fstat, -1, fd, buf);
  }

  void* mmap(void* addr, size_t length, int prot, int flags, int fd, off_t offset) {
    return call("mmap", ::mmap, MAP_FAILED, addr, length, prot, flags, fd, offset);
  }

  void munmap(void* addr, size_t length) {
    call("munmap", ::munmap, -1, addr, length);
  }

  void mlock(const void* addr, size_t len) {
    call("mlock", ::mlock, -1, addr, len);
  }

  void flock(int fd, int operation) {
    call("flock", ::flock, -1, fd, operation);
  }

  void access(const char* pathname, int mode) {
    call("access", ::access, -1, pathname, mode);
  }

  bool access_no_exception(const char* pathname, int mode) {
    return ::access(pathname, mode) == 0;
  }

  void statfs(const char* path, struct statfs* buf) {
    call("statfs", ::statfs, -1, path, buf);
  }
}
