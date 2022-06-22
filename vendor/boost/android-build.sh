#!/bin/sh

if [ -d Boost-for-Android/build/out/$2 ]; then
	echo "boost already built for $2";
	exit 0
fi

BOOST_TAR=boost_1_76_0.tar.bz2

cd Boost-for-Android || exit 1

if [ ! -f $BOOST_TAR ]; then
	wget -O $BOOST_TAR https://boostorg.jfrog.io/artifactory/main/release/1.76.0/source/boost_1_76_0.tar.bz2 || exit 1
fi

sha256sum -c ../checksum.txt && \
./build-android.sh --boost=1.76.0 --arch=$2 --target-version=21 \
	--with-libraries=atomic,chrono,container,date_time,exception,filesystem,serialization,system,thread \
	$1
