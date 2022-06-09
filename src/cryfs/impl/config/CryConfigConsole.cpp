#include "CryConfigConsole.h"
#include "CryCipher.h"

using cpputils::Console;
using boost::none;
using std::string;
using std::vector;
using std::shared_ptr;

namespace cryfs {
    constexpr const char *CryConfigConsole::DEFAULT_CIPHER;
    constexpr uint32_t CryConfigConsole::DEFAULT_BLOCKSIZE_BYTES;

    CryConfigConsole::CryConfigConsole()
            : _useDefaultSettings(none) {
    }

    string CryConfigConsole::askCipher() {
        return DEFAULT_CIPHER;
    }

    uint32_t CryConfigConsole::askBlocksizeBytes() {
        return DEFAULT_BLOCKSIZE_BYTES;
    }

    bool CryConfigConsole::askMissingBlockIsIntegrityViolation() {
        return DEFAULT_MISSINGBLOCKISINTEGRITYVIOLATION;
    }

    bool CryConfigConsole::_checkUseDefaultSettings() {
        return true;
    }
}
