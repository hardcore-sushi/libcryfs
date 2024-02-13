#!/bin/sh

set -e

if [ $(find build/$2/lib -name libboost_*.a 2>/dev/null |wc -l) -eq 10 ]; then
	echo "boost already built for $2";
	exit 0
fi

exec 9>/var/tmp/libcryfs-boost.lock
flock 9

mkdir -p build && rm -rf build/$2 && cd Boost-for-Android

BOOST_TAR=boost_1_77_0.tar.bz2
if [ ! -f $BOOST_TAR ]; then
	wget -O $BOOST_TAR https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.tar.bz2
fi
sha256sum -c ../checksum.txt

./build-android.sh --boost=1.77.0 --arch=$2 --target-version=21 \
	--with-libraries=atomic,chrono,container,date_time,exception,filesystem,serialization,system,thread \
	$1

mv build/out/$2 ../build
