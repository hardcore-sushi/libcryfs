#pragma once
#ifndef MESSMER_FSPP_FUSE_FUSE_H_
#define MESSMER_FSPP_FUSE_FUSE_H_

#include <sys/statvfs.h>
#include <cstdio>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <boost/filesystem.hpp>
#include <boost/optional.hpp>
#include <cpp-utils/macros.h>
#include <atomic>
#include <memory>
#include "stat_compatibility.h"
#include <fspp/fs_interface/Context.h>

typedef int (*fuse_fill_dir_t)(void*, const char*, fspp::fuse::STAT*);

namespace fspp {
class Device;

namespace fuse {
class Filesystem;

class Fuse final {
public:
  explicit Fuse(std::function<std::shared_ptr<Filesystem> ()> init, std::string fstype, boost::optional<std::string> fsname);
  ~Fuse();

  void runInBackground(const boost::filesystem::path &mountdir, std::vector<std::string> fuseOptions);
  void runInForeground(const boost::filesystem::path &mountdir, std::vector<std::string> fuseOptions);
  bool running() const;
  void stop();

  static void unmount(const boost::filesystem::path &mountdir, bool force = false);

  int getattr(const boost::filesystem::path &path, fspp::fuse::STAT *stbuf);
  int fgetattr(const boost::filesystem::path &path, fspp::fuse::STAT *stbuf, uint64_t fh);
  int readlink(const boost::filesystem::path &path, char *buf, size_t size);
  int mknod(const boost::filesystem::path &path, ::mode_t mode, dev_t rdev);
  int mkdir(const boost::filesystem::path &path, ::mode_t mode);
  int unlink(const boost::filesystem::path &path);
  int rmdir(const boost::filesystem::path &path);
  int symlink(const boost::filesystem::path &from, const boost::filesystem::path &to);
  int rename(const boost::filesystem::path &from, const boost::filesystem::path &to);
  int link(const boost::filesystem::path &from, const boost::filesystem::path &to);
  int chmod(const boost::filesystem::path &path, ::mode_t mode);
  int chown(const boost::filesystem::path &path, ::uid_t uid, ::gid_t gid);
  int truncate(const boost::filesystem::path &path, int64_t size);
  int ftruncate(int64_t size, uint64_t fh);
  int utimens(const boost::filesystem::path &path, const timespec lastAccessTime, const timespec lastModificationTime);
  int open(const boost::filesystem::path &path, uint64_t* fh, int flags);
  int release(uint64_t fh);
  int read(char *buf, size_t size, int64_t offset, uint64_t fh);
  int write(const char *buf, size_t size, int64_t offset, uint64_t fh);
  int statfs(const boost::filesystem::path &path, struct ::statvfs *fsstat);
  int flush(uint64_t fh);
  int fsync(int flags, uint64_t fh);
  int readdir(const boost::filesystem::path &path, void *buf, fuse_fill_dir_t filler);
  void init();
  void destroy();
  int access(const boost::filesystem::path &path, int mask);
  int create(const boost::filesystem::path &path, ::mode_t mode, uint64_t* fh);

private:
  static void _logException(const std::exception &e);
  static void _logUnknownException();
  static char *_create_c_string(const std::string &str);
  static void _removeAndWarnIfExists(std::vector<std::string> *fuseOptions, const std::string &option);
  void _run(const boost::filesystem::path &mountdir, std::vector<std::string> fuseOptions);
  static bool _has_option(const std::vector<char *> &vec, const std::string &key);
  static bool _has_entry_with_prefix(const std::string &prefix, const std::vector<char *> &vec);
  std::vector<char *> _build_argv(const boost::filesystem::path &mountdir, const std::vector<std::string> &fuseOptions);
  void _add_fuse_option_if_not_exists(std::vector<char *> *argv, const std::string &key, const std::string &value);
  void _createContext(const std::vector<std::string> &fuseOptions);

  std::function<std::shared_ptr<Filesystem> ()> _init;
  std::shared_ptr<Filesystem> _fs;
  boost::filesystem::path _mountdir;
  std::vector<char*> _argv;
  std::atomic<bool> _running;
  std::string _fstype;
  boost::optional<std::string> _fsname;
  boost::optional<Context> _context;
  ::uid_t uid;
  ::gid_t gid;
};
}
}

#endif
