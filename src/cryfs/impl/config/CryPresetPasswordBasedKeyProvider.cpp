#include "CryPresetPasswordBasedKeyProvider.h"

using cpputils::unique_ref;
using cpputils::EncryptionKey;
using cpputils::unique_ref;
using cpputils::PasswordBasedKDF;
using cpputils::Data;

namespace cryfs {

CryPresetPasswordBasedKeyProvider::CryPresetPasswordBasedKeyProvider(std::string password, unique_ref<PasswordBasedKDF> kdf, SizedData* returnedHash)
: _password(std::move(password)), _kdf(std::move(kdf)), _returnedHash(returnedHash) {}

EncryptionKey CryPresetPasswordBasedKeyProvider::requestKeyForExistingFilesystem(size_t keySize, const Data& kdfParameters) {
    EncryptionKey encryptionKey = _kdf->deriveExistingKey(keySize, _password, kdfParameters);
    if (_returnedHash != nullptr) {
        _returnedHash->data = static_cast<unsigned char*>(encryptionKey.data());
        _returnedHash->size = encryptionKey.binaryLength();
    }
    return encryptionKey;
}

CryPresetPasswordBasedKeyProvider::KeyResult CryPresetPasswordBasedKeyProvider::requestKeyForNewFilesystem(size_t keySize) {
    auto keyResult = _kdf->deriveNewKey(keySize, _password);
    if (_returnedHash != nullptr) {
        _returnedHash->data = static_cast<unsigned char*>(keyResult.key.data());
        _returnedHash->size = keyResult.key.binaryLength();
    }
    return {std::move(keyResult.key), std::move(keyResult.kdfParameters)};
}

}
