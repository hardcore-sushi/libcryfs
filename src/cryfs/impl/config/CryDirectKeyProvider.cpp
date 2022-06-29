#include "CryDirectKeyProvider.h"

namespace cryfs {

    CryDirectKeyProvider::CryDirectKeyProvider(SizedData encryptionKey) : _encryptionKey(encryptionKey) {}

    cpputils::EncryptionKey CryDirectKeyProvider::requestKeyForExistingFilesystem(size_t keySize, const cpputils::Data& kdfParameters) {
        ASSERT(_encryptionKey.size == keySize, "CryDirectKeyProvider: Invalid key size requested");
        cpputils::EncryptionKey encryptionKey = cpputils::EncryptionKey::Null(_encryptionKey.size);
        memcpy(encryptionKey.data(), _encryptionKey.data, _encryptionKey.size);
        return encryptionKey;
    }

    CryKeyProvider::KeyResult CryDirectKeyProvider::requestKeyForNewFilesystem(size_t keySize) {
        throw std::logic_error("CryDirectKeyProvider can't be used for new filesystems");
    }

}
