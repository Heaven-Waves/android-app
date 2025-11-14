/*
 * native-audio-bridge.cpp
 *
 * GStreamer-based audio streaming bridge for HeavenWaves
 * Receives audio data from Java AudioCaptureService and processes it through
 * a GStreamer pipeline for Opus encoding and file output.
 */

#include <jni.h>
#include <string>
#include <pthread.h>
#include <android/log.h>
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#define LOG_TAG "NativeAudioBridge"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

// Pipeline state structure
typedef struct {
    GstElement *pipeline;
    GstElement *appsrc;
    GstElement *audioconvert;
    GstElement *audioresample;
    GstElement *opusenc;
    GstElement *oggmux;
    GstElement *filesink;

    GMainLoop *main_loop;
    pthread_t loop_thread;

    gint sample_rate;
    gint channels;
    gint bitrate;

    gboolean is_initialized;
    gboolean is_playing;
    gboolean mutex_initialized;

    std::string last_error;
    pthread_mutex_t error_mutex;

} PipelineState;

// Global pipeline state
static PipelineState g_state = {0};

// Forward declarations
static void* pipeline_loop_thread(void* data);
static gboolean bus_message_callback(GstBus *bus, GstMessage *msg, gpointer data);
static void cleanup_pipeline();

/**
 * Initialize the GStreamer pipeline with specified audio parameters
 */
static jboolean init_pipeline(JNIEnv *env, jint sample_rate, jint channels,
                              const char *output_path, jint bitrate) {
    LOGI("Initializing GStreamer pipeline: %dHz, %d channels, %d bps, output: %s",
         sample_rate, channels, bitrate, output_path);

    // Initialize mutex if not already done
    if (!g_state.mutex_initialized) {
        pthread_mutex_init(&g_state.error_mutex, NULL);
        g_state.mutex_initialized = TRUE;
    }

    // Clean up any existing pipeline
    if (g_state.is_initialized) {
        LOGW("Pipeline already initialized, cleaning up first");
        cleanup_pipeline();
    }

    // Create pipeline elements
    g_state.pipeline = gst_pipeline_new("audio-pipeline");
    g_state.appsrc = gst_element_factory_make("appsrc", "audio-source");
    g_state.audioconvert = gst_element_factory_make("audioconvert", "converter");
    g_state.audioresample = gst_element_factory_make("audioresample", "resampler");
    g_state.opusenc = gst_element_factory_make("opusenc", "encoder");
    g_state.oggmux = gst_element_factory_make("oggmux", "muxer");
    g_state.filesink = gst_element_factory_make("filesink", "file-output");

    // Check if all elements were created successfully
    if (!g_state.pipeline || !g_state.appsrc || !g_state.audioconvert ||
        !g_state.audioresample || !g_state.opusenc || !g_state.oggmux ||
        !g_state.filesink) {

        pthread_mutex_lock(&g_state.error_mutex);
        g_state.last_error = "Failed to create one or more GStreamer elements. "
                            "Check if required plugins (opus, ogg) are available.";
        pthread_mutex_unlock(&g_state.error_mutex);
        LOGE("%s", g_state.last_error.c_str());

        cleanup_pipeline();
        return JNI_FALSE;
    }

    // Configure appsrc
    GstCaps *caps = gst_caps_new_simple("audio/x-raw",
        "format", G_TYPE_STRING, "S16LE",      // 16-bit little-endian PCM
        "rate", G_TYPE_INT, sample_rate,
        "channels", G_TYPE_INT, channels,
        "layout", G_TYPE_STRING, "interleaved",
        NULL);

    g_object_set(G_OBJECT(g_state.appsrc),
        "caps", caps,
        "stream-type", GST_APP_STREAM_TYPE_STREAM,  // Live stream, no seeking
        "format", GST_FORMAT_TIME,
        "is-live", TRUE,
        "block", TRUE,                               // Block when queue is full
        "max-bytes", (guint64)(sample_rate * channels * 2 * 2), // 2 seconds buffer
        NULL);

    gst_caps_unref(caps);

    // Configure Opus encoder
    g_object_set(G_OBJECT(g_state.opusenc),
        "bitrate", bitrate,
        "audio-type", 2048,  // Generic audio
        NULL);

    // Configure file sink
    g_object_set(G_OBJECT(g_state.filesink),
        "location", output_path,
        "sync", FALSE,       // Don't sync to clock for file writing
        NULL);

    // Add elements to pipeline
    gst_bin_add_many(GST_BIN(g_state.pipeline),
        g_state.appsrc,
        g_state.audioconvert,
        g_state.audioresample,
        g_state.opusenc,
        g_state.oggmux,
        g_state.filesink,
        NULL);

    // Link elements: appsrc -> audioconvert -> audioresample -> opusenc -> oggmux -> filesink
    if (!gst_element_link_many(
            g_state.appsrc,
            g_state.audioconvert,
            g_state.audioresample,
            g_state.opusenc,
            g_state.oggmux,
            g_state.filesink,
            NULL)) {

        pthread_mutex_lock(&g_state.error_mutex);
        g_state.last_error = "Failed to link GStreamer pipeline elements";
        pthread_mutex_unlock(&g_state.error_mutex);
        LOGE("%s", g_state.last_error.c_str());

        cleanup_pipeline();
        return JNI_FALSE;
    }

    // Set up bus message handler
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(g_state.pipeline));
    gst_bus_add_watch(bus, bus_message_callback, NULL);
    gst_object_unref(bus);

    // Store configuration
    g_state.sample_rate = sample_rate;
    g_state.channels = channels;
    g_state.bitrate = bitrate;
    g_state.is_initialized = TRUE;
    g_state.is_playing = FALSE;

    LOGI("Pipeline initialized successfully");
    return JNI_TRUE;
}

/**
 * Start the GStreamer pipeline
 */
static jboolean start_pipeline() {
    if (!g_state.is_initialized) {
        pthread_mutex_lock(&g_state.error_mutex);
        g_state.last_error = "Pipeline not initialized";
        pthread_mutex_unlock(&g_state.error_mutex);
        LOGE("%s", g_state.last_error.c_str());
        return JNI_FALSE;
    }

    if (g_state.is_playing) {
        LOGW("Pipeline already playing");
        return JNI_TRUE;
    }

    LOGI("Starting GStreamer pipeline");

    // Create main loop
    g_state.main_loop = g_main_loop_new(NULL, FALSE);

    // Start loop thread
    if (pthread_create(&g_state.loop_thread, NULL, pipeline_loop_thread, NULL) != 0) {
        pthread_mutex_lock(&g_state.error_mutex);
        g_state.last_error = "Failed to create main loop thread";
        pthread_mutex_unlock(&g_state.error_mutex);
        LOGE("%s", g_state.last_error.c_str());

        g_main_loop_unref(g_state.main_loop);
        g_state.main_loop = NULL;
        return JNI_FALSE;
    }

    // Set pipeline to PLAYING state
    GstStateChangeReturn ret = gst_element_set_state(g_state.pipeline, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        pthread_mutex_lock(&g_state.error_mutex);
        g_state.last_error = "Failed to set pipeline to PLAYING state";
        pthread_mutex_unlock(&g_state.error_mutex);
        LOGE("%s", g_state.last_error.c_str());

        // Stop the loop thread
        if (g_state.main_loop) {
            g_main_loop_quit(g_state.main_loop);
            pthread_join(g_state.loop_thread, NULL);
            g_main_loop_unref(g_state.main_loop);
            g_state.main_loop = NULL;
        }

        return JNI_FALSE;
    }

    g_state.is_playing = TRUE;
    LOGI("Pipeline started successfully");
    return JNI_TRUE;
}

/**
 * Feed audio data to the GStreamer pipeline
 */
static jboolean feed_audio_data(JNIEnv *env, jbyteArray buffer, jint size) {
    if (!g_state.is_playing) {
        // Silently ignore data if pipeline isn't playing
        return JNI_TRUE;
    }

    // Get buffer data from Java
    jbyte *buffer_data = env->GetByteArrayElements(buffer, NULL);
    if (!buffer_data) {
        LOGE("Failed to get buffer data from Java");
        return JNI_FALSE;
    }

    // Create GStreamer buffer
    GstBuffer *gst_buffer = gst_buffer_new_allocate(NULL, size, NULL);
    if (!gst_buffer) {
        LOGE("Failed to allocate GStreamer buffer");
        env->ReleaseByteArrayElements(buffer, buffer_data, JNI_ABORT);
        return JNI_FALSE;
    }

    // Copy data to GStreamer buffer
    GstMapInfo map;
    if (!gst_buffer_map(gst_buffer, &map, GST_MAP_WRITE)) {
        LOGE("Failed to map GStreamer buffer");
        gst_buffer_unref(gst_buffer);
        env->ReleaseByteArrayElements(buffer, buffer_data, JNI_ABORT);
        return JNI_FALSE;
    }

    memcpy(map.data, buffer_data, size);
    gst_buffer_unmap(gst_buffer, &map);

    // Release Java buffer
    env->ReleaseByteArrayElements(buffer, buffer_data, JNI_ABORT);

    // Push buffer to appsrc
    GstFlowReturn flow_ret = gst_app_src_push_buffer(GST_APP_SRC(g_state.appsrc), gst_buffer);

    if (flow_ret != GST_FLOW_OK) {
        pthread_mutex_lock(&g_state.error_mutex);
        g_state.last_error = "Failed to push buffer to pipeline (flow error: " +
                            std::to_string(flow_ret) + ")";
        pthread_mutex_unlock(&g_state.error_mutex);
        LOGE("%s", g_state.last_error.c_str());
        return JNI_FALSE;
    }

    return JNI_TRUE;
}

/**
 * Stop the GStreamer pipeline
 */
static void stop_pipeline() {
    if (!g_state.is_initialized) {
        LOGW("Pipeline not initialized, nothing to stop");
        return;
    }

    LOGI("Stopping GStreamer pipeline");
    g_state.is_playing = FALSE;

    // Send EOS to appsrc
    if (g_state.appsrc) {
        gst_app_src_end_of_stream(GST_APP_SRC(g_state.appsrc));
        LOGD("Sent EOS to appsrc");
    }

    // Wait for EOS message or timeout (3 seconds)
    if (g_state.pipeline) {
        GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(g_state.pipeline));
        GstMessage *msg = gst_bus_timed_pop_filtered(bus,
            3 * GST_SECOND,
            (GstMessageType)(GST_MESSAGE_EOS | GST_MESSAGE_ERROR));

        if (msg) {
            if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
                GError *err;
                gchar *debug_info;
                gst_message_parse_error(msg, &err, &debug_info);
                LOGE("Error during pipeline shutdown: %s", err->message);
                g_error_free(err);
                g_free(debug_info);
            } else {
                LOGD("Received EOS during shutdown");
            }
            gst_message_unref(msg);
        } else {
            LOGW("Did not receive EOS within timeout");
        }

        gst_object_unref(bus);
    }

    // Set pipeline to NULL state
    if (g_state.pipeline) {
        gst_element_set_state(g_state.pipeline, GST_STATE_NULL);
        LOGD("Pipeline set to NULL state");
    }

    // Stop main loop
    if (g_state.main_loop) {
        g_main_loop_quit(g_state.main_loop);
        pthread_join(g_state.loop_thread, NULL);
        g_main_loop_unref(g_state.main_loop);
        g_state.main_loop = NULL;
        LOGD("Main loop stopped");
    }

    LOGI("Pipeline stopped successfully");
}

/**
 * Clean up and free all pipeline resources
 */
static void cleanup_pipeline() {
    LOGI("Cleaning up pipeline resources");

    if (g_state.is_playing) {
        stop_pipeline();
    }

    if (g_state.pipeline) {
        gst_object_unref(g_state.pipeline);
        g_state.pipeline = NULL;
    }

    // Reset element pointers (they were unreferenced with the pipeline)
    g_state.appsrc = NULL;
    g_state.audioconvert = NULL;
    g_state.audioresample = NULL;
    g_state.opusenc = NULL;
    g_state.oggmux = NULL;
    g_state.filesink = NULL;

    g_state.is_initialized = FALSE;
    g_state.is_playing = FALSE;

    LOGI("Pipeline cleanup complete");
}

/**
 * Get the last error message
 */
static std::string get_last_error() {
    pthread_mutex_lock(&g_state.error_mutex);
    std::string error = g_state.last_error;
    pthread_mutex_unlock(&g_state.error_mutex);
    return error;
}

/**
 * GStreamer bus message callback
 */
static gboolean bus_message_callback(GstBus *bus, GstMessage *msg, gpointer data) {
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err;
            gchar *debug_info;
            gst_message_parse_error(msg, &err, &debug_info);

            pthread_mutex_lock(&g_state.error_mutex);
            g_state.last_error = std::string("GStreamer Error: ") + err->message;
            pthread_mutex_unlock(&g_state.error_mutex);

            LOGE("GStreamer error from %s: %s",
                 GST_OBJECT_NAME(msg->src), err->message);
            LOGE("Debug info: %s", debug_info ? debug_info : "none");

            g_error_free(err);
            g_free(debug_info);

            // Stop the main loop on error
            if (g_state.main_loop) {
                g_main_loop_quit(g_state.main_loop);
            }
            break;
        }

        case GST_MESSAGE_WARNING: {
            GError *err;
            gchar *debug_info;
            gst_message_parse_warning(msg, &err, &debug_info);

            LOGW("GStreamer warning from %s: %s",
                 GST_OBJECT_NAME(msg->src), err->message);
            LOGW("Debug info: %s", debug_info ? debug_info : "none");

            g_error_free(err);
            g_free(debug_info);
            break;
        }

        case GST_MESSAGE_EOS:
            LOGI("End-of-stream reached");
            if (g_state.main_loop) {
                g_main_loop_quit(g_state.main_loop);
            }
            break;

        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(msg) == GST_OBJECT(g_state.pipeline)) {
                GstState old_state, new_state, pending_state;
                gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
                LOGD("Pipeline state changed: %s -> %s",
                     gst_element_state_get_name(old_state),
                     gst_element_state_get_name(new_state));
            }
            break;
        }

        default:
            break;
    }

    return TRUE;  // Keep receiving messages
}

/**
 * Pipeline main loop thread
 */
static void* pipeline_loop_thread(void* data) {
    LOGD("Pipeline loop thread started");
    g_main_loop_run(g_state.main_loop);
    LOGD("Pipeline loop thread exiting");
    return NULL;
}

// ============================================================================
// JNI Method Implementations
// ============================================================================

extern "C" {

/**
 * Initialize the GStreamer audio pipeline
 *
 * @param sample_rate Audio sample rate (e.g., 48000)
 * @param channels Number of audio channels (1=mono, 2=stereo)
 * @param output_path Full path to output file
 * @param bitrate Opus encoder bitrate in bps (e.g., 128000)
 * @return true if initialization successful, false otherwise
 */
JNIEXPORT jboolean JNICALL
Java_com_justivo_heavenwaves_AudioCaptureService_nativeInitPipeline(
        JNIEnv *env, jobject thiz,
        jint sample_rate, jint channels, jstring output_path, jint bitrate) {

    const char *path = env->GetStringUTFChars(output_path, NULL);
    jboolean result = init_pipeline(env, sample_rate, channels, path, bitrate);
    env->ReleaseStringUTFChars(output_path, path);

    return result;
}

/**
 * Feed audio data to the GStreamer pipeline
 *
 * @param buffer Audio data buffer (16-bit PCM)
 * @param size Size of data in bytes
 * @return true if data was accepted, false on error
 */
JNIEXPORT jboolean JNICALL
Java_com_justivo_heavenwaves_AudioCaptureService_nativeFeedAudioData(
        JNIEnv *env, jobject thiz,
        jbyteArray buffer, jint size) {

    return feed_audio_data(env, buffer, size);
}

/**
 * Start the GStreamer pipeline
 *
 * @return true if pipeline started successfully, false otherwise
 */
JNIEXPORT jboolean JNICALL
Java_com_justivo_heavenwaves_AudioCaptureService_nativeStartPipeline(
        JNIEnv *env, jobject thiz) {

    return start_pipeline();
}

/**
 * Stop the GStreamer pipeline
 */
JNIEXPORT void JNICALL
Java_com_justivo_heavenwaves_AudioCaptureService_nativeStopPipeline(
        JNIEnv *env, jobject thiz) {

    stop_pipeline();
}

/**
 * Get the last error message from the pipeline
 *
 * @return Last error message, or empty string if no error
 */
JNIEXPORT jstring JNICALL
Java_com_justivo_heavenwaves_AudioCaptureService_nativeGetLastError(
        JNIEnv *env, jobject thiz) {

    std::string error = get_last_error();
    return env->NewStringUTF(error.c_str());
}

} // extern "C"
