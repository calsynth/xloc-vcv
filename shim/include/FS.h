// Host-directory-backed Teensy FS API (used by SD.h and LittleFS.h shims).
#pragma once

#include <Arduino.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <vector>

// mingw's mkdir() takes a single argument
#ifdef _WIN32
#include <direct.h>
static inline int xemu_mkdir(const char *p) { return ::_mkdir(p); }
#else
static inline int xemu_mkdir(const char *p) { return ::mkdir(p, 0755); }
#endif

#define FILE_READ 0
#define FILE_WRITE 1
#define FILE_WRITE_BEGIN 2

class File {
public:
  File() {}

  // regular file
  File(const std::string &path, int mode) {
    const char *m = (mode == FILE_READ) ? "rb" : (mode == FILE_WRITE ? "ab+" : "wb+");
    FILE *f = fopen(path.c_str(), m);
    if (f) {
      impl_ = std::make_shared<Impl>();
      impl_->f = f;
      impl_->path = path;
    }
  }

  // directory
  static File open_dir(const std::string &path) {
    DIR *d = opendir(path.c_str());
    File file;
    if (d) {
      file.impl_ = std::make_shared<Impl>();
      file.impl_->dir = d;
      file.impl_->path = path;
    }
    return file;
  }

  operator bool() const { return (bool)impl_ && (impl_->f || impl_->dir); }
  bool isDirectory() const { return impl_ && impl_->dir; }

  const char *name() {
    if (!impl_) return "";
    size_t slash = impl_->path.find_last_of('/');
    impl_->basename =
        (slash == std::string::npos) ? impl_->path : impl_->path.substr(slash + 1);
    return impl_->basename.c_str();
  }

  int read() {
    if (!impl_ || !impl_->f) return -1;
    return fgetc(impl_->f);
  }
  size_t read(void *buf, size_t n) {
    if (!impl_ || !impl_->f) return 0;
    return fread(buf, 1, n, impl_->f);
  }
  int peek() {
    if (!impl_ || !impl_->f) return -1;
    int c = fgetc(impl_->f);
    if (c >= 0) ungetc(c, impl_->f);
    return c;
  }
  size_t write(uint8_t b) {
    if (!impl_ || !impl_->f) return 0;
    return fputc(b, impl_->f) == EOF ? 0 : 1;
  }
  size_t write(const void *buf, size_t n) {
    if (!impl_ || !impl_->f) return 0;
    return fwrite(buf, 1, n, impl_->f);
  }
  size_t write(const char *s) { return write(s, strlen(s)); }
  size_t print(const char *s) { return write(s); }
  size_t println(const char *s) { size_t n = write(s); return n + write("\n", 1); }
  size_t printf(const char *fmt, ...) {
    if (!impl_ || !impl_->f) return 0;
    char buf[512];
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, args);
    va_end(args);
    if (n > 0) return write(buf, (size_t)n);
    return 0;
  }
  // Reads until terminator (consumed, not included) or EOF.
  String readStringUntil(char terminator) {
    String out;
    int c;
    while ((c = read()) >= 0) {
      if ((char)c == terminator) break;
      out.push_back((char)c);
    }
    return out;
  }

  int available() {
    if (!impl_ || !impl_->f) return 0;
    long pos = ftell(impl_->f);
    fseek(impl_->f, 0, SEEK_END);
    long end = ftell(impl_->f);
    fseek(impl_->f, pos, SEEK_SET);
    return (int)(end - pos);
  }
  bool seek(uint64_t pos) {
    if (!impl_ || !impl_->f) return false;
    return fseek(impl_->f, (long)pos, SEEK_SET) == 0;
  }
  uint64_t position() {
    if (!impl_ || !impl_->f) return 0;
    return (uint64_t)ftell(impl_->f);
  }
  uint64_t size() {
    if (!impl_ || !impl_->f) return 0;
    long pos = ftell(impl_->f);
    fseek(impl_->f, 0, SEEK_END);
    long end = ftell(impl_->f);
    fseek(impl_->f, pos, SEEK_SET);
    return (uint64_t)end;
  }
  void flush() {
    if (impl_ && impl_->f) fflush(impl_->f);
  }
  void close() {
    if (impl_) {
      if (impl_->f) { fclose(impl_->f); impl_->f = nullptr; }
      if (impl_->dir) { closedir(impl_->dir); impl_->dir = nullptr; }
    }
  }

  File openNextFile();

private:
  struct Impl {
    FILE *f = nullptr;
    DIR *dir = nullptr;
    std::string path;
    std::string basename;
    ~Impl() {
      if (f) fclose(f);
      if (dir) closedir(dir);
    }
  };
  std::shared_ptr<Impl> impl_;
};

class FS {
public:
  explicit FS(const std::string &root = "") : root_(root) {}
  void set_root(const std::string &root) { root_ = root; }

  std::string host_path(const char *path) const {
    std::string p = path ? path : "";
    if (!p.empty() && p[0] == '/') p = p.substr(1);
    if (root_.empty()) return p.empty() ? "." : p;
    return p.empty() ? root_ : root_ + "/" + p;
  }

  File open(const char *path, int mode = FILE_READ) {
    std::string hp = host_path(path);
    struct stat st;
    if (mode == FILE_READ && stat(hp.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
      return File::open_dir(hp);
    if (mode != FILE_READ) ensure_parent(hp);
    return File(hp, mode);
  }
  bool exists(const char *path) {
    struct stat st;
    return stat(host_path(path).c_str(), &st) == 0;
  }
  bool remove(const char *path) { return ::remove(host_path(path).c_str()) == 0; }
  bool mkdir(const char *path) {
    return xemu_mkdir(host_path(path).c_str()) == 0;
  }
  bool rename(const char *from, const char *to) {
    return ::rename(host_path(from).c_str(), host_path(to).c_str()) == 0;
  }
  bool rmdir(const char *path) { return ::rmdir(host_path(path).c_str()) == 0; }

  uint64_t usedSize() { return 0; }
  uint64_t totalSize() { return 960 * 1024; }
  bool format() { return true; }
  bool quickFormat() { return true; }

protected:
  void ensure_parent(const std::string &hp) {
    size_t slash = hp.find_last_of('/');
    if (slash == std::string::npos) return;
    std::string dir = hp.substr(0, slash);
    // mkdir -p
    std::string acc;
    size_t start = 0;
    while (start <= dir.size()) {
      size_t next = dir.find('/', start);
      std::string part = dir.substr(start, next == std::string::npos ? std::string::npos : next - start);
      if (!part.empty()) {
        acc += (acc.empty() ? "" : "/") + part;
        xemu_mkdir(acc.c_str());
      }
      if (next == std::string::npos) break;
      start = next + 1;
    }
  }
  std::string root_;
};

inline File File::openNextFile() {
  if (!impl_ || !impl_->dir) return File();
  for (;;) {
    struct dirent *e = readdir(impl_->dir);
    if (!e) return File();
    if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
    std::string child = impl_->path + "/" + e->d_name;
    struct stat st;
    if (stat(child.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
      return File::open_dir(child);
    return File(child, FILE_READ);
  }
}
