package com.justivo.heavenwaves;

import android.content.Context;
import android.content.Intent;
import android.media.projection.MediaProjectionManager;
import android.os.Bundle;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import org.freedesktop.gstreamer.GStreamer;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("audio_bridge");
    }

    private MediaProjectionManager mediaProjectionManager;
    private Button startButton;
    private Button stopButton;
    private TextView statusText;

    private final ActivityResultLauncher<Intent> mediaProjectionLauncher =
            registerForActivityResult(new ActivityResultContracts.StartActivityForResult(), result -> {
                if (result.getResultCode() == RESULT_OK && result.getData() != null) {
                    // Permission granted, start the service
                    Intent serviceIntent = new Intent(this, AudioCaptureService.class);
                    serviceIntent.putExtra("MEDIA_PROJECTION", result.getData());
                    startForegroundService(serviceIntent);

                    updateUIState(true);
                    Toast.makeText(this, "Audio capture started", Toast.LENGTH_SHORT).show();
                } else {
                    Toast.makeText(this, "Media projection permission denied", Toast.LENGTH_SHORT).show();
                }
            });

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        EdgeToEdge.enable(this);

        setContentView(R.layout.activity_main);

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main), (v, insets) -> {
            Insets systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom);
            return insets;
        });

        // Initialize GStreamer
        try {
            GStreamer.init(this);
        } catch (Exception e) {
            Toast.makeText(this, "Failed to initialize GStreamer: " + e.getMessage(), Toast.LENGTH_LONG).show();
            finish();
            return;
        }

        // Get GStreamer version info and display it
        String gstreamerInfo = GStreamer.nativeGetGStreamerInfo();
        TextView textView = findViewById(R.id.gstreamer_info_text);
        textView.setText(gstreamerInfo);
    }
}