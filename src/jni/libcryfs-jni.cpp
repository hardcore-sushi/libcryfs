#include <jni.h>
#include <cryfs-cli/Cli.h>
#include <fspp/fuse/Fuse.h>
#include <cryfs/impl/CryfsException.h>
#include <cryfs/impl/config/CryKeyProvider.h>
#include <cryfs/impl/config/CryDirectKeyProvider.h>
#include <cryfs/impl/config/CryPresetPasswordBasedKeyProvider.h>

using boost::none;
using cpputils::Random;
using cpputils::SCrypt;
using cryfs_cli::Cli;
using cryfs_cli::program_options::ProgramOptions;
using fspp::fuse::Fuse;

std::set<jlong> validFusePtrs;

jfieldID getValueField(JNIEnv* env, jobject object) {
	return env->GetFieldID(env->GetObjectClass(object), "value", "Ljava/lang/Object;");
}

void setReturnedPasswordHash(JNIEnv* env, jobject jreturnedHash, const SizedData& returnedHash) {
	jbyteArray jpasswordHash = env->NewByteArray(returnedHash.size);
	env->SetByteArrayRegion(jpasswordHash, 0, returnedHash.size, reinterpret_cast<const jbyte*>(returnedHash.data));
	delete[] returnedHash.data;
	env->SetObjectField(jreturnedHash, getValueField(env, jreturnedHash), jpasswordHash);
}

extern "C" jlong
cryfs_init(JNIEnv *env, jstring jbaseDir, jstring jlocalStateDir, jbyteArray jpassword,
           jbyteArray jgivenHash, jobject jreturnedHash, jboolean createBaseDir,
           jstring jcipher, jobject jerrorCode) {
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
	env->ReleaseStringUTFChars(jbaseDir, baseDir);
	env->ReleaseStringUTFChars(jlocalStateDir, localStateDir);
	struct SizedData returnedHash;
	struct Cli::Credentials credentials;
	credentials.returnedHash = nullptr;
	if (jpassword == NULL) {
		credentials.password = none;
		credentials.givenHash.data = reinterpret_cast<unsigned char*>(env->GetByteArrayElements(jgivenHash, NULL));
		credentials.givenHash.size = env->GetArrayLength(jgivenHash);
	} else {
		jbyte* password = env->GetByteArrayElements(jpassword, NULL);
		credentials.password = string(reinterpret_cast<const char*>(password), env->GetArrayLength(jpassword));
		env->ReleaseByteArrayElements(jpassword, password, 0);
		if (jreturnedHash != NULL) {
			credentials.returnedHash = &returnedHash;
		}
	}

	Fuse* fuse = 0;
	try {
		fuse = Cli(keyGenerator, SCrypt::DefaultSettings).initFilesystem(options, credentials);
	} catch (const cryfs::CryfsException &e) {
		int errorCode = static_cast<int>(e.errorCode());
		if (e.what() != string()) {
			LOG(cpputils::logging::ERR, "Error {}: {}", errorCode, e.what());
		}
		jclass integerClass = env->FindClass("java/lang/Integer");
		jobject integer = env->NewObject(integerClass, env->GetMethodID(integerClass, "<init>", "(I)V"), errorCode);
		env->SetObjectField(jerrorCode, getValueField(env, jerrorCode), integer);
	}
	if (jpassword == NULL) {
		env->ReleaseByteArrayElements(jgivenHash, reinterpret_cast<jbyte*>(credentials.givenHash.data), 0);
	}
	if (credentials.returnedHash != nullptr) {
		setReturnedPasswordHash(env, jreturnedHash, returnedHash);
	}
	jlong fusePtr = reinterpret_cast<jlong>(fuse);
	if (fusePtr != 0) {
		validFusePtrs.insert(fusePtr);
	}
	return fusePtr;
}

extern "C" jboolean cryfs_change_encryption_key(JNIEnv* env, jstring jbaseDir, jstring jlocalStateDir,
                                                jbyteArray jcurrentPassword, jbyteArray jgivenHash,
                                                jbyteArray jnewPassword, jobject jreturnedHash) {
	using namespace cryfs;

	const char* baseDir = env->GetStringUTFChars(jbaseDir, NULL);
	const char* localStateDir = env->GetStringUTFChars(jlocalStateDir, NULL);

	struct SizedData givenHash;
	std::unique_ptr<CryKeyProvider> currentKeyProvider;
	if (jcurrentPassword == NULL) {
		givenHash.data = reinterpret_cast<unsigned char*>(env->GetByteArrayElements(jgivenHash, NULL));
		givenHash.size = env->GetArrayLength(jgivenHash);
		currentKeyProvider = std::make_unique<CryDirectKeyProvider>(givenHash);
	} else {
		jbyte* currentPassword = env->GetByteArrayElements(jcurrentPassword, NULL);
		currentKeyProvider = std::make_unique<CryPresetPasswordBasedKeyProvider>(
			string(reinterpret_cast<const char*>(currentPassword), env->GetArrayLength(jcurrentPassword)),
			cpputils::make_unique_ref<SCrypt>(SCrypt::DefaultSettings),
			nullptr
		);
		env->ReleaseByteArrayElements(jcurrentPassword, currentPassword, 0);
	}
	struct SizedData returnedHash = {nullptr, 0};
	jbyte* newPassword = env->GetByteArrayElements(jnewPassword, NULL);
	cpputils::unique_ref<CryKeyProvider> newKeyProvider = cpputils::make_unique_ref<CryPresetPasswordBasedKeyProvider>(
		string(reinterpret_cast<const char*>(newPassword), env->GetArrayLength(jnewPassword)),
		cpputils::make_unique_ref<SCrypt>(SCrypt::DefaultSettings),
		jreturnedHash == NULL ? nullptr : &returnedHash
	);
	env->ReleaseByteArrayElements(jnewPassword, newPassword, 0);
	CryConfigLoader configLoader = CryConfigLoader(
		Random::OSRandom(), std::move(*cpputils::nullcheck(std::move(currentKeyProvider))),
		LocalStateDir(localStateDir), none, none, none
	);
	env->ReleaseStringUTFChars(jlocalStateDir, localStateDir);

	auto result = configLoader.changeEncryptionKey(boost::filesystem::path(baseDir) / "cryfs.config", false, false, std::move(newKeyProvider));

	if (jcurrentPassword == NULL) {
		env->ReleaseByteArrayElements(jgivenHash, reinterpret_cast<jbyte*>(givenHash.data), 0);
	}
	env->ReleaseStringUTFChars(jbaseDir, baseDir);
	if (returnedHash.data != nullptr) {
		setReturnedPasswordHash(env, jreturnedHash, returnedHash);
	}
	return result == none;
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

extern "C" jint cryfs_read(JNIEnv* env, jlong fusePtr, jlong fileHandle, jlong fileOffset, jbyteArray jbuffer, jlong dstOffset, jlong length) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	char* buff = reinterpret_cast<char*>(env->GetByteArrayElements(jbuffer, NULL));

	int result = fuse->read(buff+dstOffset, length, fileOffset, fileHandle);

	env->ReleaseByteArrayElements(jbuffer, reinterpret_cast<jbyte*>(buff), 0);
	return result;
}

extern "C" jint cryfs_write(JNIEnv* env, jlong fusePtr, jlong fileHandle, jlong fileOffset, jbyteArray jbuffer, jlong srcOffset, jlong length) {
	Fuse* fuse = reinterpret_cast<Fuse*>(fusePtr);
	char* buff = reinterpret_cast<char*>(env->GetByteArrayElements(jbuffer, NULL));

	int result = fuse->write(buff+srcOffset, length, fileOffset, fileHandle);

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
