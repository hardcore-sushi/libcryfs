#!/usr/bin/env bash

set -e

if [ -f "vendor_cryptopp/build/$2/libcryptopp.a" ]; then
	echo "Crypto++ already built for $2";
	exit 0;
fi

exec 9>/var/tmp/libcryfs-cryptopp.lock
flock 9

export ANDROID_NDK_ROOT=$1
cd vendor_cryptopp
make clean
mkdir -p build/$2
source TestScripts/setenv-android.sh 21 $2
make -f GNUmakefile-cross static
mv libcryptopp.a build/$2
