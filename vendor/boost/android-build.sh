#!/bin/sh

set -e

BOOST_VERSION="$1"
NDK_PATH="$2"
ABI="$3"

if [ "$(find build/"$ABI"/lib -name "libboost_*-$BOOST_VERSION.a" 2>/dev/null |wc -l)" -eq 10 ]; then
	echo "boost already built for $ABI";
	exit 0
fi

exec 9>/var/tmp/libcryfs-boost.lock
flock 9

mkdir -p build && rm -rf build/"$ABI" && cd Boost-for-Android

BOOST_TAR="boost_${BOOST_VERSION}_0.tar.bz2"
BOOST_VERSION_DOTTED="$(echo "$BOOST_VERSION" | tr _ .).0"
if [ ! -f "$BOOST_TAR" ]; then
	DL_URL="https://archives.boost.io/release/$BOOST_VERSION_DOTTED/source/$BOOST_TAR"
	if command -v wget; then
		wget -O "$BOOST_TAR" "$DL_URL"
	elif command -v curl; then
		curl -fLo "$BOOST_TAR" "$DL_URL"
	else
		echo "Neither curl or wget have been found">&2
		exit 1
	fi
fi
sha256sum -c ../checksum.txt

./build-android.sh --boost="$BOOST_VERSION_DOTTED" --arch="$ABI" --target-version=21 \
	--with-libraries=atomic,chrono,container,date_time,exception,filesystem,serialization,system,thread \
	"$NDK_PATH"

mv build/out/"$ABI" ../build
