#include <jni.h>

jlong cryfs_init(JNIEnv *env, jstring jbaseDir, jstring jlocalSateDir, jbyteArray jpassword,
                 jbyteArray jgivenHash, jobject returnedHash, jboolean createBaseDir,
                 jstring jcipher);
jboolean cryfs_change_encryption_key(JNIEnv *env,
        jstring jbaseDir, jstring jlocalStateDir,
        jbyteArray jcurrentPassword, jbyteArray jgivenHash,
        jbyteArray jnewPassword, jobject jreturnedHash);
jlong cryfs_create(JNIEnv* env, jlong fusePtr, jstring jpath, mode_t mode);
jlong cryfs_open(JNIEnv* env, jlong fusePtr, jstring jpath, jint flags);
jint cryfs_read(JNIEnv* env, jlong fusePtr, jlong fileHandle, jlong fileOffset, jbyteArray buffer, jlong dstOffset, jlong length);
jint cryfs_write(JNIEnv* env, jlong fusePtr, jlong fileHandle, jlong fileOffset, jbyteArray buffer, jlong srcOffset, jlong length);
jint cryfs_truncate(JNIEnv* env, jlong fusePtr, jstring jpath, jlong size);
jint cryfs_unlink(JNIEnv* env, jlong fusePtr, jstring jpath);
jint cryfs_release(jlong fusePtr, jlong fileHandle);
jlong cryfs_readdir(JNIEnv* env, jlong fusePtr, jstring jpath ,void* data, int(void*, const char*, const struct stat*));
jint cryfs_mkdir(JNIEnv* env, jlong fusePtr, jstring jpath, mode_t mode);
jint cryfs_rmdir(JNIEnv* env, jlong fusePtr, jstring jpath);
jint cryfs_getattr(JNIEnv* env, jlong fusePtr, jstring jpath, struct stat* stat);
jint cryfs_rename(JNIEnv* env, jlong fusePtr, jstring jsrcPath, jstring jdstPath);
void cryfs_destroy(jlong fusePtr);
jboolean cryfs_is_closed(jlong fusePtr);
