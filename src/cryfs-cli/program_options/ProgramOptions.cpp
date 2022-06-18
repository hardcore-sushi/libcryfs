#include "ProgramOptions.h"
#include <cstring>
#include <cpp-utils/assert/assert.h>
#include <cpp-utils/system/path.h>

using namespace cryfs_cli::program_options;
using std::string;
using std::vector;
using boost::optional;
namespace bf = boost::filesystem;

ProgramOptions::ProgramOptions(bf::path baseDir, optional<bf::path> configFile,
                               boost::filesystem::path localStateDir,
                               bool allowFilesystemUpgrade, bool allowReplacedFilesystem, 
                               bool createMissingBasedir,
                               optional<string> cipher,
                               optional<uint32_t> blocksizeBytes,
                               bool allowIntegrityViolations,
                               boost::optional<bool> missingBlockIsIntegrityViolation)
    : _baseDir(bf::absolute(std::move(baseDir))), _configFile(std::move(configFile)),
	_localStateDir(std::move(localStateDir)),
	  _allowFilesystemUpgrade(allowFilesystemUpgrade), _allowReplacedFilesystem(allowReplacedFilesystem),
      _createMissingBasedir(createMissingBasedir),
      _cipher(std::move(cipher)), _blocksizeBytes(std::move(blocksizeBytes)),
      _allowIntegrityViolations(allowIntegrityViolations),
      _missingBlockIsIntegrityViolation(std::move(missingBlockIsIntegrityViolation)) {
}

const bf::path &ProgramOptions::baseDir() const {
    return _baseDir;
}

const optional<bf::path> &ProgramOptions::configFile() const {
    return _configFile;
}

const bf::path &ProgramOptions::localStateDir() const {
    return _localStateDir;
}

bool ProgramOptions::allowFilesystemUpgrade() const {
  return _allowFilesystemUpgrade;
}

bool ProgramOptions::createMissingBasedir() const {
    return _createMissingBasedir;
}

const optional<string> &ProgramOptions::cipher() const {
    return _cipher;
}

const optional<uint32_t> &ProgramOptions::blocksizeBytes() const {
    return _blocksizeBytes;
}

bool ProgramOptions::allowIntegrityViolations() const {
    return _allowIntegrityViolations;
}

bool ProgramOptions::allowReplacedFilesystem() const {
    return _allowReplacedFilesystem;
}

const optional<bool> &ProgramOptions::missingBlockIsIntegrityViolation() const {
    return _missingBlockIsIntegrityViolation;
}
