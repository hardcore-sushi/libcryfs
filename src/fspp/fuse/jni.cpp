#include <jni.h>
#include <cryfs-cli/Cli.h>
#include "Fuse.h"

using std::unique_ptr;
using std::make_unique;
using boost::none;
using cpputils::Random;
using cpputils::SCrypt;
using cryfs_cli::Cli;
using cryfs_cli::program_options::ProgramOptions;
using fspp::fuse::Fuse;

std::set<jlong> validFusePtrs;

extern "C" jlong cryfs_init(JNIEnv* env, jstring jbaseDir, jstring jlocalStateDir, jbyteArray jpassword, jboolean createBaseDir, jstring jcipher) {
	const char* baseDir = env->GetStringUTFChars(jbaseDir, NULL);
	const char* localStateDir = env->GetStringUTFChars(jlocalStateDir, NULL);
	boost::optional<string> cipher = none;
	if (jcipher != NULL) {
		const char* cipherName = env->GetStringUTFChars(jcipher, NULL);
		cipher = boost::optional<string>(cipherName);
		env->ReleaseStringUTFChars(jcipher, cipherName);
	}
        auto &keyGenerator = Random::OSRandom();
	ProgramOptions options = ProgramOptions(baseDir, none, localStateDir, false, false, createBaseDir, cipher, none, false, none);
	char* password = reinterpret_cast<char*>(env->GetByteArrayElements(jpassword, NULL));

        Fuse* fuse = Cli(keyGenerator, SCrypt::DefaultSettings).initFilesystem(options, make_unique<string>(password));

	env->ReleaseByteArrayElements(jpassword, reinterpret_cast<jbyte*>(password), 0);
	env->ReleaseStringUTFChars(jbaseDir, baseDir);
	env->ReleaseStringUTFChars(jlocalStateDir, localStateDir);

	jlong fusePtr = reinterpret_cast<jlong>(fuse);
	if (fusePtr != 0) {
		validFusePtrs.insert(fusePtr);
	}
	return fusePtr;
}

extern "C" jlong cryfs_create(JNIEnv* env, jlong fusePtr, jstring jpath, mode_t mode) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	const char* path = env->GetStringUTFChars(jpath, NULL);
	uint64_t fh;

	int result = fuse->create(path, mode, &fh);

	env->ReleaseStringUTFChars(jpath, path);
	if (result == 0) {
		return fh;
	} else {
		return -1;
	}
}

extern "C" jlong cryfs_open(JNIEnv* env, jlong fusePtr, jstring jpath, jint flags) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	const char* path = env->GetStringUTFChars(jpath, NULL);
	uint64_t fh;

	int result = fuse->open(path, &fh, flags);

	env->ReleaseStringUTFChars(jpath, path);
	if (result == 0) {
		return fh;
	} else {
		return -1;
	}
}

extern "C" jint cryfs_read(JNIEnv* env, jlong fusePtr, jlong fileHandle, jbyteArray jbuffer, jlong offset) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	const jsize size = env->GetArrayLength(jbuffer);
	char* buff = reinterpret_cast<char*>(env->GetByteArrayElements(jbuffer, NULL));

	int result = fuse->read(buff, size, offset, fileHandle);

	env->ReleaseByteArrayElements(jbuffer, reinterpret_cast<jbyte*>(buff), 0);
	return result;
}

extern "C" jint cryfs_write(JNIEnv* env, jlong fusePtr, jlong fileHandle, jlong offset, jbyteArray jbuffer, jint size) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	char* buff = reinterpret_cast<char*>(env->GetByteArrayElements(jbuffer, NULL));

	int result = fuse->write(buff, size, offset, fileHandle);

	env->ReleaseByteArrayElements(jbuffer, reinterpret_cast<jbyte*>(buff), 0);
	return result;
}

extern "C" jint cryfs_truncate(JNIEnv* env, jlong fusePtr, jstring jpath, jlong size) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	const char* path = env->GetStringUTFChars(jpath, NULL);

	int result = fuse->truncate(path, size);

	env->ReleaseStringUTFChars(jpath, path);
	return result;
}

extern "C" jint cryfs_unlink(JNIEnv* env, jlong fusePtr, jstring jpath) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	const char* path = env->GetStringUTFChars(jpath, NULL);

	int result = fuse->unlink(path);

	env->ReleaseStringUTFChars(jpath, path);
	return result;
}

extern "C" jint cryfs_release(jlong fusePtr, jlong fileHandle) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	return fuse->release(fileHandle);
}

struct readDirHelper {
	Fuse* fuse;
	boost::filesystem::path path;
	void* data;
	fuse_fill_dir_t filler;
};

int readDir(void* data, const char* name, fspp::fuse::STAT* stat) {
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
	    return 0;
	}
	struct readDirHelper* helper = reinterpret_cast<readDirHelper*>(data);
	mode_t mode = stat->st_mode; // saving mode because getattr sometimes modifies it badly
	helper->fuse->getattr(helper->path / name, stat);
	stat->st_mode = mode;
	return helper->filler(helper->data, name, stat);
}

extern "C" jint cryfs_readdir(JNIEnv* env, jlong fusePtr, jstring jpath, void* data, fuse_fill_dir_t filler) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	const char* path = env->GetStringUTFChars(jpath, NULL);
	struct readDirHelper helper;
	helper.fuse = fuse;
	helper.path = boost::filesystem::path(path);
	helper.data = data;
	helper.filler = filler;
	
	int result = fuse->readdir(path, &helper, readDir);

	env->ReleaseStringUTFChars(jpath, path);
	return result;
}

extern "C" jint cryfs_mkdir(JNIEnv* env, jlong fusePtr, jstring jpath, mode_t mode) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	const char* path = env->GetStringUTFChars(jpath, NULL);

	int result = fuse->mkdir(path, mode);

	env->ReleaseStringUTFChars(jpath, path);
	return result;
}

extern "C" jint cryfs_rmdir(JNIEnv* env, jlong fusePtr, jstring jpath) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	const char* path = env->GetStringUTFChars(jpath, NULL);

	int result = fuse->rmdir(path);

	env->ReleaseStringUTFChars(jpath, path);
	return result;
}

extern "C" jint cryfs_getattr(JNIEnv* env, jlong fusePtr, jstring jpath, fspp::fuse::STAT* stat) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	const char* path = env->GetStringUTFChars(jpath, NULL);

	int result = fuse->getattr(path, stat);

	env->ReleaseStringUTFChars(jpath, path);
	return result;
}

extern "C" jint cryfs_rename(JNIEnv* env, jlong fusePtr, jstring jsrcPath, jstring jdstPath) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	const char* srcPath = env->GetStringUTFChars(jsrcPath, NULL);
	const char* dstPath = env->GetStringUTFChars(jdstPath, NULL);

	int result = fuse->rename(srcPath, dstPath);

	env->ReleaseStringUTFChars(jsrcPath, srcPath);
	env->ReleaseStringUTFChars(jdstPath, dstPath);
	return result;
}

extern "C" void cryfs_destroy(jlong fusePtr) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	fuse->destroy();
	delete fuse;
	validFusePtrs.erase(fusePtr);
}

extern "C" jboolean cryfs_is_closed(jlong fusePtr) {
	return validFusePtrs.find(fusePtr) == validFusePtrs.end();
}
