#pragma once
#ifndef MESSMER_CRYFSCLI_PROGRAMOPTIONS_PROGRAMOPTIONS_H
#define MESSMER_CRYFSCLI_PROGRAMOPTIONS_PROGRAMOPTIONS_H

#include <vector>
#include <string>
#include <boost/optional.hpp>
#include <cpp-utils/macros.h>
#include <boost/filesystem.hpp>

namespace cryfs_cli {
    namespace program_options {
        class ProgramOptions final {
        public:
            ProgramOptions(boost::filesystem::path baseDir,
                           boost::optional<boost::filesystem::path> configFile,
			   boost::filesystem::path localStateDir,
                           bool allowFilesystemUpgrade, bool allowReplacedFilesystem,
                           bool createMissingBasedir,
                           boost::optional<std::string> cipher,
                           boost::optional<uint32_t> blocksizeBytes,
                           bool allowIntegrityViolations,
                           boost::optional<bool> missingBlockIsIntegrityViolation);
            ProgramOptions(ProgramOptions &&rhs) = default;

            const boost::filesystem::path &baseDir() const;
            const boost::optional<boost::filesystem::path> &configFile() const;
            const boost::filesystem::path &localStateDir() const;
            bool allowFilesystemUpgrade() const;
            bool allowReplacedFilesystem() const;
            bool createMissingBasedir() const;
            const boost::optional<std::string> &cipher() const;
            const boost::optional<uint32_t> &blocksizeBytes() const;
            bool allowIntegrityViolations() const;
            const boost::optional<bool> &missingBlockIsIntegrityViolation() const;

        private:
            boost::filesystem::path _baseDir; // this is always absolute
			boost::optional<boost::filesystem::path> _configFile;
            boost::filesystem::path _localStateDir;
            bool _allowFilesystemUpgrade;
            bool _allowReplacedFilesystem;
            bool _createMissingBasedir;
            boost::optional<std::string> _cipher;
            boost::optional<uint32_t> _blocksizeBytes;
            bool _allowIntegrityViolations;
            boost::optional<bool> _missingBlockIsIntegrityViolation;

            DISALLOW_COPY_AND_ASSIGN(ProgramOptions);
        };
    }
}

#endif
