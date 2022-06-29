#pragma once
#ifndef CRYFS_CRYPRESETPASSWORDFROMCONSOLEKEYPROVIDER_H
#define CRYFS_CRYPRESETPASSWORDFROMCONSOLEKEYPROVIDER_H

#include "CryKeyProvider.h"
#include <functional>
#include <cpp-utils/crypto/kdf/PasswordBasedKDF.h>
#include <cpp-utils/SizedData.h>

namespace cryfs {

    class CryPresetPasswordBasedKeyProvider final : public CryKeyProvider {
    public:
        explicit CryPresetPasswordBasedKeyProvider(
            std::string password,
            cpputils::unique_ref<cpputils::PasswordBasedKDF> kdf,
            SizedData* returnedHash
        );

        cpputils::EncryptionKey requestKeyForExistingFilesystem(size_t keySize, const cpputils::Data& kdfParameters) override;
        KeyResult requestKeyForNewFilesystem(size_t keySize) override;

    private:
        void saveEncryptionKey(cpputils::EncryptionKey encryptionKey);

        std::string _password;
        cpputils::unique_ref<cpputils::PasswordBasedKDF> _kdf;
        SizedData* _returnedHash;

        DISALLOW_COPY_AND_ASSIGN(CryPresetPasswordBasedKeyProvider);
    };

}

#endif
