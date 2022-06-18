#pragma once
#ifndef MESSMER_CRYFS_SRC_CONFIG_CRYCONFIGCONSOLE_H
#define MESSMER_CRYFS_SRC_CONFIG_CRYCONFIGCONSOLE_H

#include <cpp-utils/pointer/unique_ref.h>
#include <cpp-utils/io/Console.h>
#include <boost/optional.hpp>

namespace cryfs {
    class CryConfigConsole final {
    public:
        CryConfigConsole();
        CryConfigConsole(CryConfigConsole &&rhs) = default;

        std::string askCipher();
        uint32_t askBlocksizeBytes();
        bool askMissingBlockIsIntegrityViolation();

        static constexpr const char *DEFAULT_CIPHER = "xchacha20-poly1305";
        static constexpr uint32_t DEFAULT_BLOCKSIZE_BYTES = 16 * 1024; // 16KB
        static constexpr uint32_t DEFAULT_MISSINGBLOCKISINTEGRITYVIOLATION = false;

    private:

        bool _checkUseDefaultSettings();

        boost::optional<bool> _useDefaultSettings;

        DISALLOW_COPY_AND_ASSIGN(CryConfigConsole);
    };
}

#endif
