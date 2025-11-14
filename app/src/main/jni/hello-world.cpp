#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <gst/gst.h>

#define LOG_TAG "GStreamerHelloWorld"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// Native method to initialize GStreamer
static void gst_native_init(JNIEnv *env, jclass klass) {
    GError *error = NULL;

    LOGD("Initializing GStreamer...");

    if (!gst_init_check(NULL, NULL, &error)) {
        LOGE("Failed to initialize GStreamer: %s", error ? error->message : "Unknown error");
        if (error) {
            g_error_free(error);
        }
        return;
    }

    LOGD("GStreamer initialized successfully");
}

// Native method to get GStreamer version information
static jstring gst_native_get_gstreamer_info(JNIEnv *env, jclass klass) {
    char *version_utf8 = gst_version_string();
    jstring version_jstring = env->NewStringUTF(version_utf8);
    g_free(version_utf8);

    LOGD("GStreamer version: %s", version_utf8);

    return version_jstring;
}

// JNI method registration
static JNINativeMethod native_methods[] = {
    {"nativeInit", "(Landroid/content/Context;)V", (void *)gst_native_init},
    {"nativeGetGStreamerInfo", "()Ljava/lang/String;", (void *)gst_native_get_gstreamer_info}
};

// JNI_OnLoad - called when the library is loaded
jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = NULL;

    if (vm->GetEnv((void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Failed to get JNI environment");
        return JNI_ERR;
    }

    // Find the GStreamer class (adjust package name if needed)
    jclass gstreamer_class = env->FindClass("org/freedesktop/gstreamer/GStreamer");
    if (!gstreamer_class) {
        LOGE("Failed to find GStreamer class");
        return JNI_ERR;
    }

    // Register native methods
    if (env->RegisterNatives(gstreamer_class, native_methods,
                            sizeof(native_methods) / sizeof(native_methods[0])) < 0) {
        LOGE("Failed to register native methods");
        return JNI_ERR;
    }

    LOGD("JNI_OnLoad completed successfully");

    return JNI_VERSION_1_6;
}