package com.justivo.heavenwaves;

import android.os.Bundle;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
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