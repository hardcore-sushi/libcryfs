libcryfs is a re-desing of the original [CryFS](https://github.com/cryfs/cryfs) code to work as a library. Volumes are not mounted with [FUSE](https://www.kernel.org/doc/html/latest/filesystems/fuse.html) but rather opened in memory and accessed through API calls. What the purpose ?
- Allow the use of CryFS on platforms where FUSE is not available (such as Android)
- Reduce attack surface by restricting volumes access to only one process rather than one user

## Warning !
The only goal of this library is to be integrated in [DroidFS](https://forge.chapril.org/hardcoresushi/DroidFS). Thus, the current API has been designed to be accessed only via [Java Native Interface](https://docs.oracle.com/javase/8/docs/technotes/guides/jni), and logging has been redirected to Android logcat. You cannot use this library as is outside of Android. Moreover, libcryfs doesn't implement all features provided by CryFS such as symbolic links, editing attributes, flushing files... Use it at your own risk !

## Changes:
Here is what has been modified from the original project:
- Update checks disabled
- FUSE, [curl](https://curl.se), [range-v3](https://github.com/ericniebler/range-v3) dependencies removed
- [Conan](https://conan.io) configuration removed (switched to git submodules for [boost](https://www.boost.org) and [spdlog](https://github.com/gabime/spdlog))
- `src/cryfs-unmount` and `src/stats` directories deleted
- sh scripts, `cpack`, `doc`, `test` and `vendor/googletest` deleted
- Main program `cryfs-cli` removed
- boost build configured with [Boost-for-Android](https://github.com/moritz-wundke/Boost-for-Android)
- Interactive mode removed (including any writes to stdout)
- Logging output redirected to logcat
- JNI API created in `src/jni`
