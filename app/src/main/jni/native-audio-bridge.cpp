/*
 * native-audio-bridge.cpp
 *
 * GStreamer-based audio streaming bridge for HeavenWaves
 * Simplified C++ implementation following GStreamer best practices
 */

#include <jni.h>
#include <string>
#include <memory>
#include <android/log.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#define LOG_TAG "NativeAudioBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

/**
 * AudioPipeline - Encapsulates GStreamer pipeline state and operations
 *
 * Design follows GStreamer best practices:
 * - Uses gst_parse_launch for simple pipeline creation
 * - Proper reference counting with GStreamer objects
 * - Bus watch for message handling
 * - Clean state transitions
 */
class AudioPipeline {
    private:
        GstElement *pipeline = nullptr;
        GstElement *appsrc = nullptr;
        GstBus *bus = nullptr;
        guint bus_watch_id = 0;

        std::string last_error;
        bool is_initialized = false;

        // Audio parameters
        gint _sample_rate = 0;
        gint _channels = 0;

        /**
         * Bus message callback - handles pipeline messages
         */
        static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data) {
            AudioPipeline *pipeline = static_cast<AudioPipeline*>(data);

            switch (GST_MESSAGE_TYPE(msg)) {
                case GST_MESSAGE_ERROR: {
                    GError *err = nullptr;
                    gchar *debug_info = nullptr;
                    gst_message_parse_error(msg, &err, &debug_info);

                    pipeline->last_error = std::string("GStreamer error: ") + err->message;
                    LOGE("Pipeline error from %s: %s", GST_OBJECT_NAME(msg->src), err->message);
                    LOGE("Debug info: %s", debug_info ? debug_info : "none");

                    g_clear_error(&err);
                    g_free(debug_info);
                    break;
                }

                case GST_MESSAGE_WARNING: {
                    GError *err = nullptr;
                    gchar *debug_info = nullptr;
                    gst_message_parse_warning(msg, &err, &debug_info);

                    LOGW("Pipeline warning from %s: %s", GST_OBJECT_NAME(msg->src), err->message);

                    g_clear_error(&err);
                    g_free(debug_info);
                    break;
                }

                case GST_MESSAGE_EOS:
                    LOGI("End-of-stream reached");
                    break;

                case GST_MESSAGE_STATE_CHANGED:
                    if (GST_MESSAGE_SRC(msg) == GST_OBJECT(pipeline->pipeline)) {
                        GstState old_state, new_state, pending_state;
                        gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                        LOGD("Pipeline state: %s -> %s",
                             gst_element_state_get_name(old_state),
                             gst_element_state_get_name(new_state));
                    }
                    break;

                default:
                    break;
            }

            return TRUE;
        }

    public:
        /**
         * Initialize the GStreamer pipeline
         *
         * Creates pipeline: appsrc ! audioconvert ! audioresample ! opusenc ! oggmux ! filesink
         * Following GStreamer best practice: use gst_parse_launch for simple pipelines
         */
        bool init(
                const std::string &host,
                gint sample_rate,
                gint channels,
                const std::string &output_path,
                gint bitrate
                ) {
            if (is_initialized) {
                LOGW("Pipeline already initialized");
                cleanup();
            }

            this->_sample_rate = sample_rate;
            this->_channels = channels;

            LOGI("Initializing pipeline: %dHz, %dch, %dbps -> %s",
                 sample_rate, channels, bitrate, output_path.c_str());

            // Build pipeline string
            std::string pipeline_desc =
                "appsrc name=audiosrc is-live=true format=time "
                "! audioconvert "
                "! audioresample "
                "! opusenc bitrate=" + std::to_string(bitrate) + " "
                "! rtpopuspay "
                "! udpsink host=" + host + " port=5004 sync=false";

            // Parse and create pipeline
            GError *error = nullptr;
            pipeline = gst_parse_launch(pipeline_desc.c_str(), &error);

            if (!pipeline || error) {
                last_error = error ? error->message : "Failed to create pipeline";
                LOGE("%s", last_error.c_str());
                g_clear_error(&error);
                return false;
            }

            // Get appsrc element
            appsrc = gst_bin_get_by_name(GST_BIN(pipeline), "audiosrc");
            if (!appsrc) {
                last_error = "Failed to get appsrc element";
                LOGE("%s", last_error.c_str());
                cleanup();
                return false;
            }

            // Configure appsrc caps
            GstCaps *caps = gst_caps_new_simple("audio/x-raw",
                "format", G_TYPE_STRING, "S16LE",
                "rate", G_TYPE_INT, sample_rate,
                "channels", G_TYPE_INT, channels,
                "layout", G_TYPE_STRING, "interleaved",
                nullptr);

            g_object_set(G_OBJECT(appsrc),
                "caps", caps,
                "stream-type", GST_APP_STREAM_TYPE_STREAM,
                "format", GST_FORMAT_TIME,
                "max-bytes", (guint64)(sample_rate * channels * 2 * 2), // 2 seconds buffer
                nullptr);

            gst_caps_unref(caps);

            // Setup bus watch for messages
            bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
            bus_watch_id = gst_bus_add_watch(bus, bus_callback, this);
            gst_object_unref(bus);

            is_initialized = true;
            LOGI("Pipeline initialized successfully");
            return true;
        }

        /**
         * Start the pipeline
         */
        bool start() {
            if (!is_initialized) {
                last_error = "Pipeline not initialized";
                LOGE("%s", last_error.c_str());
                return false;
            }

            LOGI("Starting pipeline");

            GstStateChangeReturn ret = gst_element_set_state(pipeline, GST_STATE_PLAYING);
            if (ret == GST_STATE_CHANGE_FAILURE) {
                last_error = "Failed to start pipeline";
                LOGE("%s", last_error.c_str());
                return false;
            }

            LOGI("Pipeline started successfully");
            return true;
        }

        /**
         * Feed audio data to the pipeline
         * Following GStreamer best practice: use gst_app_src_push_buffer
         */
        bool push_data(const guint8 *data, gsize size) {
            if (!is_initialized || !appsrc) {
                return false;
            }

            // Create buffer and copy data
            GstBuffer *buffer = gst_buffer_new_allocate(nullptr, size, nullptr);
            if (!buffer) {
                LOGE("Failed to allocate buffer");
                return false;
            }

            // Map and fill buffer
            GstMapInfo map;
            if (!gst_buffer_map(buffer, &map, GST_MAP_WRITE)) {
                LOGE("Failed to map buffer");
                gst_buffer_unref(buffer);
                return false;
            }

            memcpy(map.data, data, size);
            gst_buffer_unmap(buffer, &map);

            // Push to appsrc
            GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);

            if (ret != GST_FLOW_OK) {
                last_error = "Flow error: " + std::to_string(ret);
                LOGW("Push buffer failed: %s", gst_flow_get_name(ret));
                return false;
            }

            return true;
        }

        /**
         * Stop the pipeline gracefully
         * Following GStreamer best practice: send EOS and wait for completion
         */
        void stop() {
            if (!is_initialized) {
                return;
            }

            LOGI("Stopping pipeline");

            // Send EOS to appsrc for graceful shutdown
            if (appsrc) {
                gst_app_src_end_of_stream(GST_APP_SRC(appsrc));
            }

            // Wait for EOS message on the bus (with timeout)
            if (pipeline) {
                GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
                GstMessage *msg = gst_bus_timed_pop_filtered(bus,
                    3 * GST_SECOND,
                    static_cast<GstMessageType>(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

                if (msg) {
                    if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                        LOGW("Error during shutdown");
                    }
                    gst_message_unref(msg);
                } else {
                    LOGW("Timeout waiting for EOS");
                }

                gst_object_unref(bus);
            }

            // Set to NULL state
            if (pipeline) {
                gst_element_set_state(pipeline, GST_STATE_NULL);
            }

            LOGI("Pipeline stopped");
        }

        /**
         * Cleanup all resources
         * Following RAII principles for resource management
         */
        void cleanup() {
            LOGD("Cleaning up pipeline");

            if (bus_watch_id > 0) {
                g_source_remove(bus_watch_id);
                bus_watch_id = 0;
            }

            if (appsrc) {
                gst_object_unref(appsrc);
                appsrc = nullptr;
            }

            if (pipeline) {
                gst_object_unref(pipeline);
                pipeline = nullptr;
            }

            is_initialized = false;
            LOGD("Cleanup complete");
        }

        /**
         * Get last error message
         */
        std::string get_last_error() const {
            return last_error;
        }

        /**
         * Destructor - ensure cleanup
         */
        ~AudioPipeline() {
            stop();
            cleanup();
        }
};

// Global pipeline instance
static std::unique_ptr<AudioPipeline> g_pipeline;

// Forward declaration - implemented in gstreamer-info.cpp
extern "C" jint register_gstreamer_methods(JNIEnv *env);

// ============================================================================
// JNI Native Method Implementations
// ============================================================================

/**
 * Initialize the GStreamer audio pipeline
 */
static jboolean native_init_pipeline(JNIEnv *env, jobject thiz,
                                      jstring host,
                                      jint sample_rate, jint channels,
                                      jstring output_path, jint bitrate) {
    // Get host string
    const char *host_str = env->GetStringUTFChars(host, nullptr);
    if (!host_str) {
        LOGE("Failed to get host string");
        return JNI_FALSE;
    }

    // Get output path string
    const char *path_str = env->GetStringUTFChars(output_path, nullptr);
    if (!path_str) {
        LOGE("Failed to get output path string");
        env->ReleaseStringUTFChars(host, host_str);
        return JNI_FALSE;
    }

    // Create new pipeline
    g_pipeline = std::make_unique<AudioPipeline>();

    // Initialize
    bool result = g_pipeline->init(host_str, sample_rate, channels, path_str, bitrate);

    // Release strings
    env->ReleaseStringUTFChars(host, host_str);
    env->ReleaseStringUTFChars(output_path, path_str);

    if (!result) {
        g_pipeline.reset();
    }

    return result ? JNI_TRUE : JNI_FALSE;
}

/**
 * Start the GStreamer pipeline
 */
static jboolean native_start_pipeline(JNIEnv *env, jobject thiz) {
    if (!g_pipeline) {
        LOGE("Pipeline not initialized");
        return JNI_FALSE;
    }

    return g_pipeline->start() ? JNI_TRUE : JNI_FALSE;
}

/**
 * Feed audio data to the pipeline
 */
static jboolean native_feed_audio_data(JNIEnv *env, jobject thiz,
                                        jbyteArray buffer, jint size) {
    if (!g_pipeline) {
        return JNI_TRUE; // Silently ignore if no pipeline
    }

    // Get buffer data
    jbyte *buffer_data = env->GetByteArrayElements(buffer, nullptr);
    if (!buffer_data) {
        LOGE("Failed to get buffer data");
        return JNI_FALSE;
    }

    // Push data to pipeline
    bool result = g_pipeline->push_data(
        reinterpret_cast<const guint8*>(buffer_data),
        static_cast<gsize>(size)
    );

    // Release buffer (no need to copy back)
    env->ReleaseByteArrayElements(buffer, buffer_data, JNI_ABORT);

    return result ? JNI_TRUE : JNI_FALSE;
}

/**
 * Stop the GStreamer pipeline
 */
static void native_stop_pipeline(JNIEnv *env, jobject thiz) {
    if (g_pipeline) {
        g_pipeline->stop();
        g_pipeline.reset();
    }
}

/**
 * Get last error message
 */
static jstring native_get_last_error(JNIEnv *env, jobject thiz) {
    if (!g_pipeline) {
        return env->NewStringUTF("Pipeline not initialized");
    }

    std::string error = g_pipeline->get_last_error();
    return env->NewStringUTF(error.c_str());
}

// ============================================================================
// JNI Method Registration
// ============================================================================

/**
 * Native method table for AudioCaptureService
 */
static JNINativeMethod native_methods[] = {
    {"nativeInitPipeline", "(Ljava/lang/String;IILjava/lang/String;I)Z", (void *) native_init_pipeline},
    {"nativeStartPipeline", "()Z", (void *) native_start_pipeline},
    {"nativeFeedAudioData", "([BI)Z", (void *) native_feed_audio_data},
    {"nativeStopPipeline", "()V", (void *) native_stop_pipeline},
    {"nativeGetLastError", "()Ljava/lang/String;", (void *) native_get_last_error}
};

/**
 * JNI_OnLoad - Called when the library is loaded
 * Registers native methods for both AudioCaptureService and GStreamer classes
 */
JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env = nullptr;

    if (vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        LOGE("Failed to get JNI environment");
        return JNI_ERR;
    }

    // Register AudioCaptureService methods
    jclass audio_service_class = env->FindClass("com/justivo/heavenwaves/AudioCaptureService");
    if (!audio_service_class) {
        LOGE("Failed to find AudioCaptureService class");
        return JNI_ERR;
    }

    if (env->RegisterNatives(audio_service_class, native_methods, G_N_ELEMENTS(native_methods))) {
        LOGE("Failed to register AudioCaptureService native methods");
        return JNI_ERR;
    }

    LOGI("AudioCaptureService native methods registered successfully");

    // Register GStreamer class methods (implemented in gstreamer-info.cpp)
    if (register_gstreamer_methods(env) != JNI_OK) {
        LOGE("Failed to register GStreamer native methods");
        return JNI_ERR;
    }

    LOGI("All native methods registered successfully");

    return JNI_VERSION_1_6;
}
