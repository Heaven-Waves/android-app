package com.justivo.heavenwaves;

import android.app.Activity;
import android.app.Notification;
import android.app.NotificationChannel;
import android.app.NotificationManager;
import android.app.Service;
import android.content.Intent;
import android.content.Context;
import android.content.pm.ServiceInfo;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.media.AudioAttributes;
import android.media.AudioFormat;
import android.media.AudioPlaybackCaptureConfiguration;
import android.media.AudioRecord;
import android.media.projection.MediaProjection;
import android.media.projection.MediaProjectionManager;
import android.os.Build;
import android.os.IBinder;
import android.util.Log;

import androidx.core.app.NotificationCompat;

import java.nio.ByteBuffer;
import java.util.Objects;

public class AudioCaptureService extends Service {
    private static final String TAG = "AudioCaptureService";

    private static final String ACTION_START = "AudioCaptureService:Start";
    private static final String ACTION_STOP = "AudioCaptureService:Stop";
    private static final String CHANNEL_ID = "HeavenWavesAudioCaptureChannel";
    private static final int NOTIFICATION_ID = 1;

    // Audio configuration for maximum quality
    private static final int SAMPLE_RATE = 48000; // 48kHz for best quality
    private static final int CHANNEL_CONFIG = AudioFormat.CHANNEL_IN_STEREO;
    private static final int AUDIO_FORMAT = AudioFormat.ENCODING_PCM_16BIT; // 16-bit PCM for compatibility
    private static final int BUFFER_SIZE_MULTIPLIER = 2;
    private static final int NUM_CHANNELS = 2; // Stereo

    private MediaProjectionManager mediaProjectionManager;
    private MediaProjection mediaProjection;
    private AudioRecord audioRecord;
    private Thread captureThread;
    private volatile boolean isCapturing = false;


    @Override
    public void onCreate() {
        super.onCreate();
        createNotificationChannel();

        var applicationContext = getApplicationContext();

        mediaProjectionManager = (MediaProjectionManager) applicationContext
                .getSystemService(Context.MEDIA_PROJECTION_SERVICE);
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        if (Objects.equals(intent.getAction(), ACTION_STOP)) {
            stopAudioCapture();
            stopForeground(true);
            stopSelf();
            return START_NOT_STICKY;
        }

        if (intent.hasExtra("MEDIA_PROJECTION")) {
            // IMPORTANT: Start foreground service BEFORE getting MediaProjection
            // Android requires the service to be in foreground mode with MEDIA_PROJECTION type
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.UPSIDE_DOWN_CAKE) {
                startForeground(NOTIFICATION_ID, createNotification(),
                        ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION);
            } else {
                startForeground(NOTIFICATION_ID, createNotification());
            }

            // Now get the MediaProjection after service is in foreground
            mediaProjection = mediaProjectionManager.getMediaProjection(
                    Activity.RESULT_OK,
                    Objects.requireNonNull(intent.getParcelableExtra("MEDIA_PROJECTION"))
            );

            startAudioCapture();
            return  START_STICKY;
        }
        return START_NOT_STICKY;
    }


    private void startAudioCapture() {
        if (mediaProjection == null) {
            Log.e(TAG, "MediaProjection is null");
            return;
        }

        // Create AudioPlaybackCaptureConfiguration
        AudioPlaybackCaptureConfiguration config =
                new AudioPlaybackCaptureConfiguration.Builder(mediaProjection)
                        .addMatchingUsage(AudioAttributes.USAGE_MEDIA)
                        .addMatchingUsage(AudioAttributes.USAGE_GAME)
                        .addMatchingUsage(AudioAttributes.USAGE_UNKNOWN)
                        .build();

        // Calculate optimal buffer size
        int minBufferSize = AudioRecord.getMinBufferSize(
                SAMPLE_RATE,
                CHANNEL_CONFIG,
                AUDIO_FORMAT
        );

        if (minBufferSize == AudioRecord.ERROR || minBufferSize == AudioRecord.ERROR_BAD_VALUE) {
            Log.e(TAG, "Invalid audio configuration - cannot determine buffer size");
            return;
        }

        int bufferSize = minBufferSize * BUFFER_SIZE_MULTIPLIER;

        // Create AudioRecord with the configuration
        AudioFormat audioFormat = new AudioFormat.Builder()
                .setEncoding(AUDIO_FORMAT)
                .setSampleRate(SAMPLE_RATE)
                .setChannelMask(CHANNEL_CONFIG)
                .build();

        try {
            audioRecord = new AudioRecord.Builder()
                    .setAudioFormat(audioFormat)
                    .setBufferSizeInBytes(bufferSize)
                    .setAudioPlaybackCaptureConfig(config)
                    .build();

            if (audioRecord.getState() != AudioRecord.STATE_INITIALIZED) {
                Log.e(TAG, "AudioRecord initialization failed - state: " + audioRecord.getState());
                if (audioRecord != null) {
                    audioRecord.release();
                    audioRecord = null;
                }
                return;
            }

            isCapturing = true;
            audioRecord.startRecording();

            if (audioRecord.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
                Log.e(TAG, "Failed to start recording - state: " + audioRecord.getRecordingState());
                isCapturing = false;
                audioRecord.release();
                audioRecord = null;
                return;
            }

            // Start capture thread
            captureThread = new Thread(new AudioCaptureRunnable(bufferSize));
            captureThread.setPriority(Thread.MAX_PRIORITY);
            captureThread.start();

        } catch (SecurityException e) {
            Log.e(TAG, "Security exception - missing permissions: " + e.getMessage());
            isCapturing = false;
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "Invalid argument for AudioRecord: " + e.getMessage());
            isCapturing = false;
        } catch (UnsupportedOperationException e) {
            Log.e(TAG, "AudioPlaybackCapture not supported on this device: " + e.getMessage());
            isCapturing = false;
        } catch (Exception e) {
            Log.e(TAG, "Unexpected error initializing AudioRecord: " + e.getMessage());
            isCapturing = false;
        }
    }

    private class AudioCaptureRunnable implements Runnable {
        private final int bufferSize;
        private final ByteBuffer audioBuffer;
        private final java.io.File outputFile;
        private java.io.FileOutputStream fileOutputStream;

        AudioCaptureRunnable(int bufferSize) {
            this.bufferSize = bufferSize;
            this.audioBuffer = ByteBuffer.allocateDirect(bufferSize);

            // Create output file in app's external files directory
            java.io.File outputDir = getExternalFilesDir(null);
            if (outputDir == null) {
                outputDir = getFilesDir();
            }

            String timestamp = new java.text.SimpleDateFormat("yyyyMMdd_HHmmss", java.util.Locale.US)
                    .format(new java.util.Date());
            this.outputFile = new java.io.File(outputDir, "audio_capture_" + timestamp + ".wav");

            try {
                this.fileOutputStream = new java.io.FileOutputStream(outputFile);
                writeWavHeader(fileOutputStream, SAMPLE_RATE, NUM_CHANNELS);
                Log.i(TAG, "Recording audio to: " + outputFile.getAbsolutePath());
            } catch (java.io.IOException e) {
                Log.e(TAG, "Failed to create output file: " + e.getMessage());
                this.fileOutputStream = null;
            }
        }

        private void writeWavHeader(java.io.FileOutputStream out, int sampleRate, int channels) throws java.io.IOException {
            byte[] header = new byte[44];
            int byteRate = sampleRate * channels * 2; // 2 bytes per sample for 16-bit

            // RIFF header
            header[0] = 'R'; header[1] = 'I'; header[2] = 'F'; header[3] = 'F';
            // File size (will be updated when closing)
            header[4] = 0; header[5] = 0; header[6] = 0; header[7] = 0;
            // WAVE header
            header[8] = 'W'; header[9] = 'A'; header[10] = 'V'; header[11] = 'E';
            // fmt chunk
            header[12] = 'f'; header[13] = 'm'; header[14] = 't'; header[15] = ' ';
            // fmt chunk size (16 for PCM)
            header[16] = 16; header[17] = 0; header[18] = 0; header[19] = 0;
            // Audio format (1 = PCM)
            header[20] = 1; header[21] = 0;
            // Number of channels
            header[22] = (byte) channels; header[23] = 0;
            // Sample rate
            header[24] = (byte) (sampleRate & 0xff);
            header[25] = (byte) ((sampleRate >> 8) & 0xff);
            header[26] = (byte) ((sampleRate >> 16) & 0xff);
            header[27] = (byte) ((sampleRate >> 24) & 0xff);
            // Byte rate
            header[28] = (byte) (byteRate & 0xff);
            header[29] = (byte) ((byteRate >> 8) & 0xff);
            header[30] = (byte) ((byteRate >> 16) & 0xff);
            header[31] = (byte) ((byteRate >> 24) & 0xff);
            // Block align
            header[32] = (byte) (channels * 2); header[33] = 0;
            // Bits per sample
            header[34] = 16; header[35] = 0;
            // data chunk
            header[36] = 'd'; header[37] = 'a'; header[38] = 't'; header[39] = 'a';
            // Data size (will be updated when closing)
            header[40] = 0; header[41] = 0; header[42] = 0; header[43] = 0;

            out.write(header);
        }

        private void updateWavHeader(java.io.File file) {
            try {
                java.io.RandomAccessFile raf = new java.io.RandomAccessFile(file, "rw");
                long fileSize = raf.length();
                long dataSize = fileSize - 44; // Subtract WAV header size

                // Update file size in RIFF header (total size - 8)
                raf.seek(4);
                raf.write((int) ((fileSize - 8) & 0xff));
                raf.write((int) (((fileSize - 8) >> 8) & 0xff));
                raf.write((int) (((fileSize - 8) >> 16) & 0xff));
                raf.write((int) (((fileSize - 8) >> 24) & 0xff));

                // Update data size in data chunk
                raf.seek(40);
                raf.write((int) (dataSize & 0xff));
                raf.write((int) ((dataSize >> 8) & 0xff));
                raf.write((int) ((dataSize >> 16) & 0xff));
                raf.write((int) ((dataSize >> 24) & 0xff));

                raf.close();
            } catch (java.io.IOException e) {
                Log.e(TAG, "Error updating WAV header: " + e.getMessage());
            }
        }

        @Override
        public void run() {
            android.os.Process.setThreadPriority(
                    android.os.Process.THREAD_PRIORITY_URGENT_AUDIO
            );

            byte[] buffer = new byte[bufferSize];

            while (isCapturing) {
                int dataSize = 0;

                if (AUDIO_FORMAT == AudioFormat.ENCODING_PCM_FLOAT) {
                    // For float audio
                    float[] floatBuffer = new float[bufferSize / 4];
                    int readFloats = audioRecord.read(
                            floatBuffer, 0,
                            floatBuffer.length,
                            AudioRecord.READ_BLOCKING
                    );

                    if (readFloats > 0) {
                        // Convert float to bytes for JNI transfer
                        audioBuffer.clear();
                        audioBuffer.asFloatBuffer().put(floatBuffer, 0, readFloats);
                        audioBuffer.position(0);
                        audioBuffer.get(buffer, 0, readFloats * 4);
                        dataSize = readFloats * 4;

                        // TODO: Process audio data here - send to JNI or callback
                        // processAudioData(buffer, dataSize);
                        // Write audio data to file
                        if (fileOutputStream != null) {
                            try {
                                fileOutputStream.write(buffer, 0, dataSize);
                            } catch (java.io.IOException e) {
                                Log.e(TAG, "Error writing audio data: " + e.getMessage());
                            }
                        }
                    }
                } else {
                    // For PCM16 audio
                    int bytesRead = audioRecord.read(
                            buffer, 0,
                            buffer.length,
                            AudioRecord.READ_BLOCKING
                    );

                    if (bytesRead > 0) {
                        dataSize = bytesRead;
                        // TODO: Process audio data here - send to JNI or callback
                        // processAudioData(buffer, dataSize);

                        // Write audio data to file
                        if (fileOutputStream != null) {
                            try {
                                fileOutputStream.write(buffer, 0, dataSize);
                            } catch (java.io.IOException e) {
                                Log.e(TAG, "Error writing audio data: " + e.getMessage());
                            }
                        }
                    }
                }

                if (audioRecord.getRecordingState() != AudioRecord.RECORDSTATE_RECORDING) {
                    Log.w(TAG, "AudioRecord stopped recording");
                    break;
                }
            }

            // Close file and update WAV header
            if (fileOutputStream != null) {
                try {
                    fileOutputStream.close();
                    // Update WAV header with correct file size
                    updateWavHeader(outputFile);
                    Log.d(TAG, "Audio capture finished. File: " + outputFile.getAbsolutePath() +
                          ", Size: " + outputFile.length() + " bytes");
                } catch (java.io.IOException e) {
                    Log.e(TAG, "Error closing output file: " + e.getMessage());
                }
            }
        }
    }

    private void stopAudioCapture() {
        isCapturing = false;

        if (audioRecord != null) {
            try {
                audioRecord.stop();
                audioRecord.release();
            } catch (Exception e) {
                Log.e(TAG, "Error stopping audio record: " + e.getMessage());
            }
            audioRecord = null;
        }

        if (captureThread != null) {
            try {
                captureThread.join(1000);
            } catch (InterruptedException e) {
                Log.e(TAG, "Thread join interrupted");
            }
            captureThread = null;
        }
    }

    private void createNotificationChannel() {
        NotificationChannel channel = new NotificationChannel(
                CHANNEL_ID,
                "Audio Capture Service Channel",
                NotificationManager.IMPORTANCE_HIGH
        );
        NotificationManager manager = getSystemService(NotificationManager.class);
        manager.createNotificationChannel(channel);
    }

    private Notification createNotification() {
        Bitmap largeIcon = BitmapFactory.decodeResource(getResources(), R.mipmap.ic_launcher);

        return new NotificationCompat.Builder(this, CHANNEL_ID)
                .setContentTitle("Audio Capture Active")
                .setContentText("Capturing system audio...")
                .setSmallIcon(R.mipmap.ic_launcher)
                .setLargeIcon(largeIcon)
                .setSilent(true)
                .setOngoing(true)
                .build();
    }

    @Override
    public void onDestroy() {
        stopAudioCapture();

        if (mediaProjection != null) {
            mediaProjection.stop();
            mediaProjection = null;
        }

        super.onDestroy();
    }

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }
}
