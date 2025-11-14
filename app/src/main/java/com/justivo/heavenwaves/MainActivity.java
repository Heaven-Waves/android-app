package com.justivo.heavenwaves;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.Context;
import android.content.Intent;
import android.media.projection.MediaProjectionManager;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Settings;
import android.util.Log;
import android.widget.Button;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.EdgeToEdge;
import androidx.activity.result.ActivityResultLauncher;
import androidx.activity.result.contract.ActivityResultContracts;
import androidx.appcompat.app.AppCompatActivity;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.fragment.app.DialogFragment;

import android.content.pm.PackageManager;

import com.justivo.heavenwaves.permission_text_providers.PermissionTextProvider;
import com.justivo.heavenwaves.permission_text_providers.PostNotificationsPermissionTextProvider;
import com.justivo.heavenwaves.permission_text_providers.RecordAudioPermissionTextProvider;

import org.freedesktop.gstreamer.GStreamer;

import java.util.ArrayDeque;

public class MainActivity extends AppCompatActivity {

    static {
        System.loadLibrary("gstreamer_android");
        System.loadLibrary("audio_bridge");
    }

    private MediaProjectionManager mediaProjectionManager;
    private Button startButton;
    private Button stopButton;
    private TextView statusText;
    private android.widget.EditText hostInput;
    private android.widget.CheckBox saveToFile;

    @SuppressLint("InlinedApi")
    private final String[] permissionsToRequest = {
            Manifest.permission.RECORD_AUDIO,
            Manifest.permission.POST_NOTIFICATIONS
    };

    private final ArrayDeque<String> permissionDialogQueue = new ArrayDeque<>();

    private DialogFragment permissionDialogFragment;



    private final ActivityResultLauncher<String[]> permissionsLauncher = registerForActivityResult(
            new ActivityResultContracts.RequestMultiplePermissions(),
            permissions -> {
                // Add denied permissions to the queue for showing rationale dialogs
                for (String permission: permissionsToRequest) {
                    if(Boolean.FALSE.equals(permissions.get(permission))
                            && !permissionDialogQueue.contains(permission)
                    ) {
                        permissionDialogQueue.add(permission);
                       }
                }
                // After processing results, show dialog for first denied permission
                showNextPermissionDialog();
            });
    private final ActivityResultLauncher<Intent> mediaProjectionLauncher = registerForActivityResult(
            new ActivityResultContracts.StartActivityForResult(),
            result -> {
                if (result.getResultCode() == RESULT_OK && result.getData() != null) {
                    // Get host value from input field
                    String host = hostInput.getText().toString().trim();
                    if (host.isEmpty()) {
                        Toast.makeText(this, "Please enter a host address", Toast.LENGTH_SHORT).show();
                        return;
                    }

                    // Permission granted, start the service
                    Intent serviceIntent = new Intent(this, AudioCaptureService.class);
                    serviceIntent.putExtra("MEDIA_PROJECTION", result.getData());
                    serviceIntent.putExtra("HOST", host);
                    serviceIntent.putExtra("SAVE_TO_FILE", saveToFile.isChecked());
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

        // Initialize MediaProjectionManager
        mediaProjectionManager = (MediaProjectionManager)
                getSystemService(Context.MEDIA_PROJECTION_SERVICE);

        // Initialize UI elements
        startButton = findViewById(R.id.start_button);
        stopButton = findViewById(R.id.stop_button);
        statusText = findViewById(R.id.status_text);
        hostInput = findViewById(R.id.host_input);
        saveToFile = findViewById(R.id.save_to_file_checkbox);

        // Set up button listeners
        startButton.setOnClickListener(v -> requestMediaProjection());
        stopButton.setOnClickListener(v -> stopAudioCapture());

        // Request necessary permissions on startup
        permissionsLauncher.launch(permissionsToRequest);
        updateUIState(false);
    }

    private void showNextPermissionDialog() {
        Log.d("MainActivity", "invoked next permission");
        if (permissionDialogQueue.isEmpty()) {
            return;
        }

        String permission = permissionDialogQueue.peek();
        if (permission == null) {
            return;
        }

        PermissionTextProvider permissionTextProvider;
        if (permission.equals(Manifest.permission.RECORD_AUDIO)) {
            permissionTextProvider = new RecordAudioPermissionTextProvider();
        } else if (permission.equals(Manifest.permission.POST_NOTIFICATIONS)) {
            permissionTextProvider = new PostNotificationsPermissionTextProvider();
        } else {
            return;
        }

        boolean isPermanentlyDeclined = !shouldShowRequestPermissionRationale(permission);

        // Remove current permission
        permissionDialogFragment = PermissionDialogFragment.builder()
                .permissionTextProvider(permissionTextProvider)
                .isPermanentlyDeclined(isPermanentlyDeclined)
                .onDismiss(permissionDialogQueue::removeFirst)
                .onContinue(() -> {
                    permissionsLauncher.launch(new String[] { permission });
                    updateUIState(false);
                })
                .onGoToAppSettingsClick(this::openAppSettings)
                .build();

        permissionDialogFragment.show(getSupportFragmentManager(), "PermissionDialog");
    }

    private void requestMediaProjection() {
        Intent intent = mediaProjectionManager.createScreenCaptureIntent();
        mediaProjectionLauncher.launch(intent);
    }

    private void stopAudioCapture() {
        Intent serviceIntent = new Intent(this, AudioCaptureService.class);
        serviceIntent.setAction("AudioCaptureService:Stop");
        startService(serviceIntent);

        updateUIState(false);
        Toast.makeText(this, "Audio capture stopped", Toast.LENGTH_SHORT).show();
    }

    private void updateUIState(boolean isRecording) {
        startButton.setEnabled(!isRecording);
        stopButton.setEnabled(isRecording);
        saveToFile.setEnabled(!isRecording);
        statusText.setText(isRecording ? "Recording..." : "Not recording");
    }

    private void openAppSettings() {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        Uri uri = Uri.fromParts("package", getPackageName(), null);
        intent.setData(uri);
        startActivity(intent);
    }
}