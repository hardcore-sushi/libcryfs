#!/usr/bin/env bash

export ANDROID_NDK_ROOT=$1 && \
cd vendor_cryptopp && \
mkdir -p build/$2 && \
source TestScripts/setenv-android.sh 21 $2 && \
make -f GNUmakefile-cross static && \
mv libcryptopp.a build/$2
