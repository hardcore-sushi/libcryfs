#!/bin/sh

cd vendor_cryptopp && \
	mkdir -p build/$1 && \
	TestScripts/setenv-android.sh 21 $1 && \
	make -f GNUmakefile-cross static && \
	mv libcryptopp.a build/$1
