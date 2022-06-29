#pragma once
#ifndef CRYFS_CRYDIRECTKEYPROVIDER_H
#define CRYFS_CRYDIRECTKEYPROVIDER_H

#include "CryKeyProvider.h"
#include <cpp-utils/SizedData.h>

namespace cryfs {

    class CryDirectKeyProvider final : public CryKeyProvider {
    public:
        explicit CryDirectKeyProvider(SizedData encryptionKey);

        cpputils::EncryptionKey requestKeyForExistingFilesystem(size_t keySize, const cpputils::Data& kdfParameters) override;
        KeyResult requestKeyForNewFilesystem(size_t keySize) override;

    private:
        SizedData _encryptionKey;

        DISALLOW_COPY_AND_ASSIGN(CryDirectKeyProvider);
    };

}

#endif