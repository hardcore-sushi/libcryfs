#include "Cli.h"

#include <blockstore/implementations/ondisk/OnDiskBlockStore2.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cpp-utils/assert/backtrace.h>

#include <fspp/impl/FilesystemImpl.h>
#include <cpp-utils/process/subprocess.h>
#include <cpp-utils/io/DontEchoStdinToStdoutRAII.h>
#include <cryfs/impl/filesystem/CryDevice.h>
#include <cryfs/impl/config/CryConfigLoader.h>
#include <cryfs/impl/config/CryDirectKeyProvider.h>
#include <cryfs/impl/config/CryPresetPasswordBasedKeyProvider.h>
#include <boost/filesystem.hpp>

#include <cryfs/impl/filesystem/CryDir.h>
#include <gitversion/gitversion.h>

#include "VersionChecker.h"
#include <gitversion/VersionCompare.h>
#include <cpp-utils/io/NoninteractiveConsole.h>
#include <cryfs/impl/localstate/LocalStateDir.h>
#include <cryfs/impl/localstate/BasedirMetadata.h>
#include "Environment.h"
#include <cryfs/impl/CryfsException.h>
#include <cpp-utils/thread/debugging.h>

//TODO Many functions accessing the ProgramOptions object. Factor out into class that stores it as a member.
//TODO Factor out class handling askPassword

using namespace cryfs_cli;
using namespace cryfs;
namespace bf = boost::filesystem;
using namespace cpputils::logging;

using blockstore::ondisk::OnDiskBlockStore2;
using program_options::ProgramOptions;

using cpputils::make_unique_ref;
using cpputils::NoninteractiveConsole;
using cpputils::TempFile;
using cpputils::RandomGenerator;
using cpputils::unique_ref;
using cpputils::SCrypt;
using cpputils::either;
using cpputils::SCryptSettings;
using cpputils::Console;
using cpputils::HttpClient;
using std::string;
using std::endl;
using std::shared_ptr;
using std::make_shared;
using std::unique_ptr;
using std::make_unique;
using std::function;
using boost::optional;
using boost::none;
using boost::chrono::minutes;
using boost::chrono::milliseconds;
using cpputils::dynamic_pointer_move;
using gitversion::VersionCompare;

//TODO Delete a large file in parallel possible? Takes a long time right now...
//TODO Improve parallelity.
//TODO Replace ASSERTs with other error handling when it is not a programming error but an environment influence (e.g. a block is missing)
//TODO Can we improve performance by setting compiler parameter -maes for scrypt?

namespace cryfs_cli {

    Cli::Cli(RandomGenerator &keyGenerator, const SCryptSettings &scryptSettings): _keyGenerator(keyGenerator), _scryptSettings(scryptSettings), _idleUnmounter(none), _device(none) {}

    bf::path Cli::_determineConfigFile(const ProgramOptions &options) {
        auto configFile = options.configFile();
        if (configFile == none) {
            return bf::path(options.baseDir()) / "cryfs.config";
        }
        return *configFile;
    }

    void Cli::_checkConfigIntegrity(const bf::path& basedir, const LocalStateDir& localStateDir, const CryConfigFile& config, bool allowReplacedFilesystem) {
        auto basedirMetadata = BasedirMetadata::load(localStateDir);
        if (!allowReplacedFilesystem && !basedirMetadata.filesystemIdForBasedirIsCorrect(basedir, config.config()->FilesystemId())) {
          throw CryfsException(
              "The filesystem id in the config file is different to the last time we loaded a filesystem from this basedir.", ErrorCode::FilesystemIdChanged);
        }
        // Update local state (or create it if it didn't exist yet)
        basedirMetadata.updateFilesystemIdForBasedir(basedir, config.config()->FilesystemId());
        basedirMetadata.save();
    }

    CryConfigLoader::ConfigLoadResult Cli::_loadOrCreateConfig(const ProgramOptions &options, const LocalStateDir& localStateDir, Credentials credentials) {
        auto configFile = _determineConfigFile(options);
        auto config = _loadOrCreateConfigFile(std::move(configFile), localStateDir, credentials, options.cipher(), options.blocksizeBytes(), options.allowFilesystemUpgrade(), options.missingBlockIsIntegrityViolation(), options.allowReplacedFilesystem());
        if (config.is_left()) {
            switch(config.left()) {
                case CryConfigFile::LoadError::DecryptionFailed:
                    throw CryfsException("Failed to decrypt the config file. Did you enter the correct password?", ErrorCode::WrongPassword);
                case CryConfigFile::LoadError::ConfigFileNotFound:
                    throw CryfsException("Could not find the cryfs.config file. Are you sure this is a valid CryFS file system?", ErrorCode::InvalidFilesystem);
            }
        }
        _checkConfigIntegrity(options.baseDir(), localStateDir, *config.right().configFile, options.allowReplacedFilesystem());
        return std::move(config.right());
    }

    unique_ref<CryKeyProvider> Cli::_createKeyProvider(Credentials credentials) {
        if (credentials.password == none) {
            return make_unique_ref<CryDirectKeyProvider>(credentials.givenHash);
        } else {
            return make_unique_ref<CryPresetPasswordBasedKeyProvider>(
                *credentials.password,
                make_unique_ref<SCrypt>(_scryptSettings),
                credentials.returnedHash
            );
        }
    }

    either<CryConfigFile::LoadError, CryConfigLoader::ConfigLoadResult> Cli::_loadOrCreateConfigFile(bf::path configFilePath, LocalStateDir localStateDir, Credentials credentials, const optional<string> &cipher, const optional<uint32_t> &blocksizeBytes, bool allowFilesystemUpgrade, const optional<bool> &missingBlockIsIntegrityViolation, bool allowReplacedFilesystem) {
        return CryConfigLoader(_keyGenerator, _createKeyProvider(credentials), std::move(localStateDir), cipher, blocksizeBytes, missingBlockIsIntegrityViolation).loadOrCreate(std::move(configFilePath), allowFilesystemUpgrade, allowReplacedFilesystem);
    }

    fspp::fuse::Fuse* Cli::initFilesystem(const ProgramOptions &options, Credentials credentials) {
        cpputils::showBacktraceOnCrash();
        cpputils::set_thread_name("cryfs");
        try {
	    _sanityChecks(options);
            LocalStateDir localStateDir(options.localStateDir());
            auto blockStore = make_unique_ref<OnDiskBlockStore2>(options.baseDir());
            auto config = _loadOrCreateConfig(options, localStateDir, credentials);
            fspp::fuse::Fuse* fuse = nullptr;

            auto onIntegrityViolation = [&fuse] () {
              if (fuse != nullptr) {
                LOG(ERR, "Integrity violation detected. Unmounting.");
                fuse->destroy();
              } else {
                // Usually on an integrity violation, the file system is unmounted.
                // Here, the file system isn't initialized yet, i.e. we failed in the initial steps when
                // setting up _device before running initFilesystem.
                // We can't unmount a not-mounted file system, but we can make sure it doesn't get mounted.
                throw CryfsException("Integrity violation detected. Unmounting.", ErrorCode::IntegrityViolation);
              }
            };
            const bool missingBlockIsIntegrityViolation = config.configFile->config()->missingBlockIsIntegrityViolation();
            _device = optional<unique_ref<CryDevice>>(make_unique_ref<CryDevice>(std::move(config.configFile), std::move(blockStore), std::move(localStateDir), config.myClientId, options.allowIntegrityViolations(), missingBlockIsIntegrityViolation, std::move(onIntegrityViolation)));
            _sanityCheckFilesystem(_device->get());

            auto initFilesystem = [&] (){
                ASSERT(_device != none, "File system not ready to be initialized. Was it already initialized before?");
                return make_shared<fspp::FilesystemImpl>(std::move(*_device));
            };

            fuse = new fspp::fuse::Fuse(initFilesystem);

	    fuse->init();
	    return fuse;
        } catch (const CryfsException &e) {
            if (e.what() != string()) {
              LOG(ERR, "Error {}: {}", static_cast<int>(e.errorCode()), e.what());
            }
        } catch (const std::exception &e) {
            LOG(ERR, "Crashed: {}", e.what());
        } catch (...) {
            LOG(ERR, "Crashed");
        }

	return nullptr;
    }

    void Cli::_sanityCheckFilesystem(CryDevice *device) {
        //Try to list contents of base directory
        auto _rootDir = device->Load("/"); // this might throw an exception if the root blob doesn't exist
        if (_rootDir == none) {
            throw CryfsException("Couldn't find root blob", ErrorCode::InvalidFilesystem);
        }
        auto rootDir = dynamic_pointer_move<CryDir>(*_rootDir);
        if (rootDir == none) {
            throw CryfsException("Base directory blob doesn't contain a directory", ErrorCode::InvalidFilesystem);
        }
        (*rootDir)->children(); // Load children
    }

	void Cli::_sanityChecks(const ProgramOptions &options) {
		_checkDirAccessible(bf::absolute(options.baseDir()), "base directory", options.createMissingBasedir(), ErrorCode::InaccessibleBaseDir);
    }

    void Cli::_checkDirAccessible(const bf::path &dir, const std::string &name, bool createMissingDir, ErrorCode errorCode) {
        if (!bf::exists(dir)) {
            if (createMissingDir) {
                LOG(INFO, "Automatically creating {}", name);
                if (!bf::create_directory(dir)) {
                    throw CryfsException("Error creating "+name, errorCode);
                }
            } else {
                //std::cerr << "Exit code: " << exitCode(errorCode) << std::endl;
                throw CryfsException(name + " not found.", errorCode);
            }
        }
        if (!bf::is_directory(dir)) {
            throw CryfsException(name+" is not a directory.", errorCode);
        }
    }
}
