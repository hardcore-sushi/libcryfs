// NOMINMAX works around an MSVC issue, see https://github.com/microsoft/cppwinrt/issues/479
#if defined(_MSC_VER)
#define NOMINMAX
#endif

#include "Fuse.h"
#include <memory>
#include <cassert>

#include "../fs_interface/FuseErrnoException.h"
#include "Filesystem.h"
#include <iostream>
#include <cpp-utils/assert/assert.h>
#include <cpp-utils/logging/logging.h>
#include <cpp-utils/process/subprocess.h>
#include <cpp-utils/thread/debugging.h>
#include <csignal>
#include "InvalidFilesystem.h"
#include <codecvt>
#include <boost/algorithm/string/replace.hpp>

#if defined(_MSC_VER)
#include <dokan/dokan.h>
#endif

using std::vector;
using std::string;

namespace bf = boost::filesystem;
using namespace cpputils::logging;
using std::make_shared;
using std::shared_ptr;
using std::string;
using namespace fspp::fuse;
using cpputils::set_thread_name;

namespace {
bool is_valid_fspp_path(const bf::path& path) {
  // TODO In boost 1.63, we can use path.generic() or path.generic_path() instead of path.generic_string()
  return path.has_root_directory()                     // must be absolute path
         && !path.has_root_name()                      // on Windows, it shouldn't have a device specifier (i.e. no "C:")
         && (path.string() == path.generic_string());  // must use portable '/' as directory separator
}

class ThreadNameForDebugging final {
public:
  ThreadNameForDebugging(const string& threadName) {
    std::string name = "fspp_" + threadName;
    set_thread_name(name.c_str());
  }

  ~ThreadNameForDebugging() {
    set_thread_name("fspp_idle");
  }
};
}

// Remove the following line, if you don't want to output each fuse operation on the console
#define FSPP_LOG 1

Fuse::~Fuse() {
  for(char *arg : _argv) {
    delete[] arg;
    arg = nullptr;
  }
  _argv.clear();
}

Fuse::Fuse(std::function<shared_ptr<Filesystem> ()> init, std::string fstype, boost::optional<std::string> fsname)
  :_init(std::move(init)), _fs(make_shared<InvalidFilesystem>()), _mountdir(), _running(false), _fstype(std::move(fstype)), _fsname(std::move(fsname)) {
  ASSERT(static_cast<bool>(_init), "Invalid init given");
}

void Fuse::_logException(const std::exception &e) {
  LOG(ERR, "Exception thrown: {}", e.what());
}

void Fuse::_logUnknownException() {
  LOG(ERR, "Unknown exception thrown");
}

void Fuse::_removeAndWarnIfExists(vector<string> *fuseOptions, const std::string &option) {
  auto found = std::find(fuseOptions->begin(), fuseOptions->end(), option);
  if (found != fuseOptions->end()) {
    LOG(WARN, "The fuse option {} only works when running in foreground. Removing fuse option.", option);
    do {
      fuseOptions->erase(found);
      found = std::find(fuseOptions->begin(), fuseOptions->end(), option);
    } while (found != fuseOptions->end());
  }
}

bool Fuse::running() const {
  return _running;
}

int Fuse::getattr(const bf::path &path, fspp::fuse::STAT *stbuf) {
  ThreadNameForDebugging _threadName("getattr");
#ifdef FSPP_LOG
  LOG(DEBUG, "getattr({}, _, _)", path);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    _fs->lstat(path, stbuf);
#ifdef FSPP_LOG
    LOG(DEBUG, "getattr({}, _, _): success", path);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::getattr: {}", e.what());
    return -EIO;
  } catch(const fspp::fuse::FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "getattr({}, _, _): failed with errno {}", path, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::fgetattr(const bf::path &path, fspp::fuse::STAT *stbuf, uint64_t fh) {
  ThreadNameForDebugging _threadName("fgetattr");
#ifdef FSPP_LOG
  LOG(DEBUG, "fgetattr({}, _, _)", path);
#endif

  // On FreeBSD, trying to do anything with the mountpoint ends up
  // opening it, and then using the FD for an fgetattr.  So in the
  // special case of a path of "/", I need to do a getattr on the
  // underlying base directory instead of doing the fgetattr().
  // TODO Check if necessary
  if (path.string() == "/") {
    int result = getattr(path, stbuf);
#ifdef FSPP_LOG
    LOG(DEBUG, "fgetattr({}, _, _): success", path);
#endif
    return result;
  }

  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    _fs->fstat(fh, stbuf);
#ifdef FSPP_LOG
    LOG(DEBUG, "fgetattr({}, _, _): success", path);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::fgetattr: {}", e.what());
    return -EIO;
  } catch(const fspp::fuse::FuseErrnoException &e) {
#ifdef FSPP_LOG
      LOG(ERR, "fgetattr({}, _, _): error", path);
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::readlink(const bf::path &path, char *buf, size_t size) {
  ThreadNameForDebugging _threadName("readlink");
#ifdef FSPP_LOG
  LOG(DEBUG, "readlink({}, _, {})", path, size);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    _fs->readSymlink(path, buf, fspp::num_bytes_t(size));
#ifdef FSPP_LOG
    LOG(DEBUG, "readlink({}, _, {}): success", path, size);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::readlink: {}", e.what());
    return -EIO;
  } catch (fspp::fuse::FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "readlink({}, _, {}): failed with errno {}", path, size, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::mkdir(const bf::path &path, ::mode_t mode) {
  ThreadNameForDebugging _threadName("mkdir");
#ifdef FSPP_LOG
  LOG(DEBUG, "mkdir({}, {})", path, mode);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
	// DokanY seems to call mkdir("/"). Ignore that
	if ("/" == path) {
#ifdef FSPP_LOG
        LOG(DEBUG, "mkdir({}, {}): ignored", path, mode);
#endif
		return 0;
	}

    _fs->mkdir(path, mode, uid, gid);
#ifdef FSPP_LOG
    LOG(DEBUG, "mkdir({}, {}): success", path, mode);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::mkdir: {}", e.what());
    return -EIO;
  } catch(const fspp::fuse::FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "mkdir({}, {}): failed with errno {}", path, mode, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::unlink(const bf::path &path) {
  ThreadNameForDebugging _threadName("unlink");
#ifdef FSPP_LOG
  LOG(DEBUG, "unlink({})", path);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    _fs->unlink(path);
#ifdef FSPP_LOG
    LOG(DEBUG, "unlink({}): success", path);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::unlink: {}", e.what());
    return -EIO;
  } catch(const fspp::fuse::FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "unlink({}): failed with errno {}", path, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::rmdir(const bf::path &path) {
  ThreadNameForDebugging _threadName("rmdir");
#ifdef FSPP_LOG
  LOG(DEBUG, "rmdir({})", path);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    _fs->rmdir(path);
#ifdef FSPP_LOG
    LOG(DEBUG, "rmdir({}): success", path);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::rmdir: {}", e.what());
    return -EIO;
  } catch(const fspp::fuse::FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "rmdir({}): failed with errno {}", path, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::symlink(const bf::path &to, const bf::path &from) {
  ThreadNameForDebugging _threadName("symlink");
#ifdef FSPP_LOG
  LOG(DEBUG, "symlink({}, {})", to, from);
#endif
  try {
    ASSERT(is_valid_fspp_path(from), "has to be an absolute path");
    _fs->createSymlink(to, from, uid, gid);
#ifdef FSPP_LOG
    LOG(DEBUG, "symlink({}, {}): success", to, from);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::symlink: {}", e.what());
    return -EIO;
  } catch(const fspp::fuse::FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "symlink({}, {}): failed with errno {}", to, from, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::rename(const bf::path &from, const bf::path &to) {
  ThreadNameForDebugging _threadName("rename");
#ifdef FSPP_LOG
  LOG(DEBUG, "rename({}, {})", from, to);
#endif
  try {
    ASSERT(is_valid_fspp_path(from), "from has to be an absolute path");
    ASSERT(is_valid_fspp_path(to), "rename target has to be an absolute path. If this assert throws, we have to add code here that makes the path absolute.");
    _fs->rename(from, to);
#ifdef FSPP_LOG
    LOG(DEBUG, "rename({}, {}): success", from, to);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::rename: {}", e.what());
    return -EIO;
  } catch(const fspp::fuse::FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "rename({}, {}): failed with errno {}", from, to, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

//TODO
int Fuse::link(const bf::path &from, const bf::path &to) {
  ThreadNameForDebugging _threadName("link");
  LOG(WARN, "NOT IMPLEMENTED: link({}, {})", from, to);
  //auto real_from = _impl->RootDir() / from;
  //auto real_to = _impl->RootDir() / to;
  //int retstat = ::link(real_from.string().c_str(), real_to.string().c_str());
  //return errcode_map(retstat);
  return ENOSYS;
}

int Fuse::chmod(const bf::path &path, ::mode_t mode) {
  ThreadNameForDebugging _threadName("chmod");
#ifdef FSPP_LOG
  LOG(DEBUG, "chmod({}, {})", path, mode);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
	_fs->chmod(path, mode);
#ifdef FSPP_LOG
    LOG(DEBUG, "chmod({}, {}): success", path, mode);
#endif
	return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::chmod: {}", e.what());
    return -EIO;
  } catch (fspp::fuse::FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "chmod({}, {}): failed with errno {}", path, mode, e.getErrno());
#endif
	return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::chown(const bf::path &path, ::uid_t uid, ::gid_t gid) {
  ThreadNameForDebugging _threadName("chown");
#ifdef FSPP_LOG
  LOG(DEBUG, "chown({}, {}, {})", path, uid, gid);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
	_fs->chown(path, uid, gid);
#ifdef FSPP_LOG
    LOG(DEBUG, "chown({}, {}, {}): success", path, uid, gid);
#endif
	return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::chown: {}", e.what());
    return -EIO;
  } catch (fspp::fuse::FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "chown({}, {}, {}): failed with errno {}", path, uid, gid, e.getErrno());
#endif
	return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::truncate(const bf::path &path, int64_t size) {
  ThreadNameForDebugging _threadName("truncate");
#ifdef FSPP_LOG
  LOG(DEBUG, "truncate({}, {})", path, size);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    _fs->truncate(path, fspp::num_bytes_t(size));
#ifdef FSPP_LOG
    LOG(DEBUG, "truncate({}, {}): success", path, size);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::truncate: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "truncate({}, {}): failed with errno {}", path, size, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::ftruncate(int64_t size, uint64_t fh) {
  ThreadNameForDebugging _threadName("ftruncate");
#ifdef FSPP_LOG
  LOG(DEBUG, "ftruncate({}, {})", fh, size);
#endif
  try {
    _fs->ftruncate(fh, fspp::num_bytes_t(size));
#ifdef FSPP_LOG
    LOG(DEBUG, "ftruncate({}, {}): success", fh, size);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::ftruncate: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "ftruncate({}, {}): failed with errno {}", fh, size, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::utimens(const bf::path &path, const timespec lastAccessTime, const timespec lastModificationTime) {
  ThreadNameForDebugging _threadName("utimens");
#ifdef FSPP_LOG
  LOG(DEBUG, "utimens({}, _)", path);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    _fs->utimens(path, lastAccessTime, lastModificationTime);
#ifdef FSPP_LOG
    LOG(DEBUG, "utimens({}, _): success", path);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::utimens: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "utimens({}, _): failed with errno {}", path, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::open(const bf::path &path, uint64_t* fh, int flags) {
  ThreadNameForDebugging _threadName("open");
#ifdef FSPP_LOG
  LOG(DEBUG, "open({}, _)", path);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    *fh = _fs->openFile(path, flags);
#ifdef FSPP_LOG
    LOG(DEBUG, "open({}, _): success", path);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::open: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "open({}, _): failed with errno {}", path, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::release(uint64_t fh) {
  ThreadNameForDebugging _threadName("release");
#ifdef FSPP_LOG
  LOG(DEBUG, "release({}, _)", fh);
#endif
  try {
    _fs->closeFile(fh);
#ifdef FSPP_LOG
    LOG(DEBUG, "release({}, _): success", fh);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::release: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "release({}, _): failed with errno {}", fh, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::read(char *buf, size_t size, int64_t offset, uint64_t fh) {
  ThreadNameForDebugging _threadName("read");
#ifdef FSPP_LOG
  LOG(DEBUG, "read({}, _, {}, {}, _)", fh, size, offset);
#endif
  try {
    int result = _fs->read(fh, buf, fspp::num_bytes_t(size), fspp::num_bytes_t(offset)).value();
#ifdef FSPP_LOG
    LOG(DEBUG, "read({}, _, {}, {}, _): success with {}", fh, size, offset, result);
#endif
    return result;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::read: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "read({}, _, {}, {}, _): failed with errno {}", fh, size, offset, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::write(const char *buf, size_t size, int64_t offset, uint64_t fh) {
  ThreadNameForDebugging _threadName("write");
#ifdef FSPP_LOG
  LOG(DEBUG, "write({}, _, {}, {}, _)", fh, size, offset);
#endif
  try {
    _fs->write(fh, buf, fspp::num_bytes_t(size), fspp::num_bytes_t(offset));
#ifdef FSPP_LOG
    LOG(DEBUG, "write({}, _, {}, {}, _): success", fh, size, offset);
#endif
    return size;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::write: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "write({}, _, {}, {}, _): failed with errno {}", fh, size, offset, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::statfs(const bf::path &path, struct ::statvfs *fsstat) {
  ThreadNameForDebugging _threadName("statfs");
#ifdef FSPP_LOG
  LOG(DEBUG, "statfs({}, _)", path);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    _fs->statfs(fsstat);
#ifdef FSPP_LOG
    LOG(DEBUG, "statfs({}, _): success", path);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::statfs: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "statfs({}, _): failed with errno {}", path, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::flush(uint64_t fh) {
  ThreadNameForDebugging _threadName("flush");
#ifdef FSPP_LOG
  LOG(WARN, "flush({}, _)", fh);
#endif
  try {
    _fs->flush(fh);
#ifdef FSPP_LOG
    LOG(WARN, "flush({}, _): success", fh);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::flush: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "flush({}, _): failed with errno {}", fh, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::fsync(int datasync, uint64_t fh) {
  ThreadNameForDebugging _threadName("fsync");
#ifdef FSPP_LOG
  LOG(DEBUG, "fsync({}, {}, _)", fh, datasync);
#endif
  try {
    if (datasync) {
      _fs->fdatasync(fh);
    } else {
      _fs->fsync(fh);
    }
#ifdef FSPP_LOG
  LOG(DEBUG, "fsync({}, {}, _): success", fh, datasync);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::fsync: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "fsync({}, {}, _): failed with errno {}", fh, datasync, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::readdir(const bf::path &path, void *buf, fuse_fill_dir_t filler) {
  ThreadNameForDebugging _threadName("readdir");
#ifdef FSPP_LOG
  LOG(DEBUG, "readdir({}, _, _)", path);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    auto entries = _fs->readDir(path);
    fspp::fuse::STAT stbuf{};
    for (const auto &entry : entries) {
      //We could pass more file metadata to filler() in its third parameter,
      //but it doesn't help performance since fuse ignores everything in stbuf
      //except for file-type bits in st_mode and (if used) st_ino.
      //It does getattr() calls on all entries nevertheless.
      if (entry.type == Dir::EntryType::DIR) {
        stbuf.st_mode = S_IFDIR;
      } else if (entry.type == Dir::EntryType::FILE) {
        stbuf.st_mode = S_IFREG;
      } else if (entry.type == Dir::EntryType::SYMLINK) {
        stbuf.st_mode = S_IFLNK;
      } else {
        ASSERT(false, "Unknown entry type");
      }
      if (filler(buf, entry.name.c_str(), &stbuf) != 0) {
#ifdef FSPP_LOG
        LOG(DEBUG, "readdir({}, _, _): failure with ENOMEM", path);
#endif
        return -ENOMEM;
      }
    }
#ifdef FSPP_LOG
    LOG(DEBUG, "readdir({}, _, _): success", path);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::readdir: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "readdir({}, _, _): failed with errno {}", path, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

void Fuse::init() {
  ThreadNameForDebugging _threadName("init");
  _fs = _init();

  _context = Context(noatime());
  ASSERT(_context != boost::none, "Context should have been initialized in Fuse::run() but somehow didn't");
  _fs->setContext(fspp::Context { *_context });

  LOG(INFO, "Filesystem started.");

  _running = true;

#ifdef FSPP_LOG
  cpputils::logging::setLevel(DEBUG);
#endif
}

void Fuse::destroy() {
  ThreadNameForDebugging _threadName("destroy");
  _fs = make_shared<InvalidFilesystem>();
  LOG(INFO, "Filesystem stopped.");
  _running = false;
  cpputils::logging::logger()->flush();
}

int Fuse::access(const bf::path &path, int mask) {
  ThreadNameForDebugging _threadName("access");
#ifdef FSPP_LOG
  LOG(DEBUG, "access({}, {})", path, mask);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    _fs->access(path, mask);
#ifdef FSPP_LOG
    LOG(DEBUG, "access({}, {}): success", path, mask);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::access: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "access({}, {}): failed with errno {}", path, mask, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}

int Fuse::create(const bf::path &path, ::mode_t mode, uint64_t* fh) {
  ThreadNameForDebugging _threadName("create");
#ifdef FSPP_LOG
  LOG(DEBUG, "create({}, {}, _)", path, mode);
#endif
  try {
    ASSERT(is_valid_fspp_path(path), "has to be an absolute path");
    *fh = _fs->createAndOpenFile(path, mode, uid, gid);
#ifdef FSPP_LOG
    LOG(DEBUG, "create({}, {}, _): success", path, mode);
#endif
    return 0;
  } catch(const cpputils::AssertFailed &e) {
    LOG(ERR, "AssertFailed in Fuse::create: {}", e.what());
    return -EIO;
  } catch (FuseErrnoException &e) {
#ifdef FSPP_LOG
    LOG(WARN, "create({}, {}, _): failed with errno {}", path, mode, e.getErrno());
#endif
    return -e.getErrno();
  } catch(const std::exception &e) {
    _logException(e);
    return -EIO;
  } catch(...) {
    _logUnknownException();
    return -EIO;
  }
}
