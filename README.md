# HeavenWaves

An Android application for audio capture and streaming using GStreamer multimedia framework.

## Overview

HeavenWaves is a native Android application that integrates GStreamer 1.x for advanced audio processing capabilities. The app provides real-time audio recording with a foreground service and status notifications.

## Features

- Real-time audio capture using Android's AudioRecord API
- GStreamer integration for audio processing pipeline
- Foreground service with persistent notifications
- Live recording status updates
- Edge-to-edge UI with Material Design
- Support for Android 10 through Android 14

## Technical Specifications

- **Package Name:** `com.justivo.heavenwaves`
- **Min SDK:** API 29 (Android 10)
- **Target SDK:** API 36 (Android 14)
- **Compile SDK:** API 36
- **Java Version:** 11
- **GStreamer Version:** 1.24.13
- **NDK Version:** 25.2.9519653
- **Build System:** Gradle with AGP 8.13.0

## Prerequisites

### Development Environment

1. **Android Studio** (latest stable version recommended)
2. **Java Development Kit (JDK) 11** or higher
3. **Android SDK** with API levels 29-36
4. **Android NDK** version 25.2.9519653

### GStreamer Setup

1. Download [GStreamer Android SDK](https://gstreamer.freedesktop.org/download/) version 1.24.13
2. Extract the SDK to a location on your system
3. Set the `GSTREAMER_ROOT_ANDROID` environment variable:

```bash
export GSTREAMER_ROOT_ANDROID=/path/to/gstreamer-1.0-android-universal-1.24.13
```

Add this to your `~/.bashrc`, `~/.zshrc`, or equivalent shell configuration file.

## Project Structure

```
HeavenWaves/
├── app/
│   ├── src/
│   │   ├── main/
│   │   │   ├── java/com/justivo/heavenwaves/
│   │   │   │   ├── MainActivity.java              # Main UI and lifecycle management
│   │   │   │   ├── AudioCaptureService.java       # Foreground service for audio capture
│   │   │   │   └── GStreamer.java                 # GStreamer initialization helper
│   │   │   ├── jni/                               # Native code (C++)
│   │   │   │   ├── native-audio-bridge.cpp        # JNI bridge for audio processing
│   │   │   │   ├── hello-world.cpp                # GStreamer test implementation
│   │   │   │   ├── Android.mk                     # NDK build configuration
│   │   │   │   └── Application.mk                 # Native build settings
│   │   │   ├── res/                               # Android resources
│   │   │   │   ├── layout/
│   │   │   │   │   └── activity_main.xml          # Main activity layout
│   │   │   │   ├── values/                        # Default theme and strings
│   │   │   │   ├── values-night/                  # Dark theme support
│   │   │   │   └── drawable/                      # App icons and graphics
│   │   │   └── AndroidManifest.xml                # App manifest with permissions
│   │   ├── test/                                  # Unit tests (JVM)
│   │   └── androidTest/                           # Instrumented tests (device/emulator)
│   └── build.gradle                               # App module build configuration
├── gradle/
│   └── libs.versions.toml                         # Centralized dependency versions
├── build.gradle                                   # Root project build configuration
├── settings.gradle                                # Project settings
├── CLAUDE.md                                      # AI assistant context
└── README.md                                      # This file
```

## Building the Project

### Clone the Repository

```bash
git clone <repository-url>
cd HeavenWaves
```

### Build Commands

```bash
# Clean build artifacts
./gradlew clean

# Build the project (debug and release variants)
./gradlew build

# Build debug APK only
./gradlew assembleDebug

# Build release APK
./gradlew assembleRelease

# Install debug build on connected device
./gradlew installDebug
```

### Build Output

APK files are generated in:
- Debug: `app/build/outputs/apk/debug/app-debug.apk`
- Release: `app/build/outputs/apk/release/app-release.apk`

## Running the Application

### From Android Studio

1. Open the project in Android Studio
2. Connect an Android device (API 29+) or start an emulator
3. Click the "Run" button or press `Shift + F10`

### From Command Line

```bash
# Install and launch debug build
./gradlew installDebug
adb shell am start -n com.justivo.heavenwaves/.MainActivity
```

## Testing

### Unit Tests

Run JVM-based unit tests:

```bash
# Run all unit tests
./gradlew test

# Run specific test class
./gradlew test --tests com.justivo.heavenwaves.ExampleUnitTest

# Run specific test method
./gradlew test --tests com.justivo.heavenwaves.ExampleUnitTest.addition_isCorrect
```

### Instrumented Tests

Run tests on Android device/emulator:

```bash
# Run all instrumented tests
./gradlew connectedAndroidTest
```

### Code Quality

```bash
# Run lint checks
./gradlew lint

# View lint report
open app/build/reports/lint-results.html
```

## Architecture

### Native Integration

HeavenWaves uses JNI to integrate GStreamer's powerful multimedia processing capabilities:

#### Native Module: `audio_bridge`

- **Build System:** ndk-build with Android.mk and Application.mk
- **Source Files:**
  - `native-audio-bridge.cpp` - JNI bridge implementation
  - `hello-world.cpp` - GStreamer pipeline demonstration
- **Target ABIs:** armeabi-v7a, arm64-v8a, x86, x86_64
- **Linked Libraries:** gstreamer_android, log, android, iconv

#### Java-Native Interface

**GStreamer.java:**
- Initializes GStreamer runtime
- Loads native libraries

**MainActivity.java:**
- Loads `gstreamer_android` and `audio_bridge` native libraries
- Declares native methods for JNI calls

**Native Methods:**
- `nativeInit()` - Initializes GStreamer runtime
- `nativeGetGStreamerInfo()` - Returns GStreamer version info

### Audio Capture Service

**AudioCaptureService.java** implements a foreground service that:
- Captures audio using Android's AudioRecord API
- Runs in the foreground with a persistent notification
- Provides real-time status updates via notifications
- Broadcasts recording state changes to the UI

**Recording Configuration:**
- Sample Rate: 44,100 Hz
- Channel: MONO
- Encoding: PCM 16-bit
- Audio Source: MIC

### User Interface

**MainActivity.java** provides:
- Start/Stop recording controls
- Real-time status display
- BroadcastReceiver for service state updates
- Proper lifecycle management for receiver registration
- Material Design components with edge-to-edge display

## Permissions

The app requires the following permissions (declared in AndroidManifest.xml):

- `RECORD_AUDIO` - For audio capture
- `FOREGROUND_SERVICE` - For background recording service
- `FOREGROUND_SERVICE_MEDIA_PROJECTION` - For media projection service type
- `POST_NOTIFICATIONS` - For displaying recording status notifications (Android 13+)

## Dependencies

### Core Dependencies

Managed via Gradle Version Catalog (`gradle/libs.versions.toml`):

```toml
[versions]
androidxActivity = "1.9.3"
androidxAppcompat = "1.7.0"
constraintlayout = "2.2.0"
material = "1.12.0"

[libraries]
androidx-activity = { group = "androidx.activity", name = "activity", version.ref = "androidxActivity" }
androidx-appcompat = { group = "androidx.appcompat", name = "appcompat", version.ref = "androidxAppcompat" }
androidx-constraintlayout = { group = "androidx.constraintlayout", name = "constraintlayout", version.ref = "constraintlayout" }
material = { group = "com.google.android.material", name = "material", version.ref = "material" }
```

### Test Dependencies

- JUnit 4.13.2 - Unit testing framework
- AndroidX JUnit 1.2.1 - Android instrumentation testing
- Espresso Core 3.6.1 - UI testing framework

## Development Workflow

### Adding Dependencies

1. Add version to `[versions]` section in `gradle/libs.versions.toml`
2. Add library reference to `[libraries]` section
3. Reference in `app/build.gradle` as `implementation libs.library-name`

### Working with Native Code

1. Modify C++ files in `app/src/main/jni/`
2. Update `Android.mk` if adding new source files
3. Rebuild native libraries: `./gradlew clean build`
4. Native libraries are automatically packaged into the APK

### Resource Organization

- **Layouts:** `app/src/main/res/layout/`
- **Strings:** `app/src/main/res/values/strings.xml`
- **Themes:** `app/src/main/res/values/themes.xml`
- **Dark Theme:** `app/src/main/res/values-night/themes.xml`
- **Icons:** `app/src/main/res/drawable/` and `app/src/main/res/mipmap/`

## User Experience Flow

1. **Start Recording:**
   - User grants audio recording permission (if not already granted)
   - Taps "Start Recording" button
   - AudioCaptureService starts as foreground service
   - Persistent notification displays "Audio recording in progress..."
   - UI shows "Status: Recording" and disables start button

2. **During Recording:**
   - Notification remains visible with high priority
   - User cannot dismiss notification (`.setOngoing(true)`)
   - Service continues recording in background if user navigates away

3. **Stop Recording:**
   - User taps "Stop Recording" button
   - Service stops audio capture and terminates
   - Notification is dismissed
   - UI shows "Status: Not Recording" and enables start button

## Troubleshooting

### GStreamer Not Found

**Error:** `GSTREAMER_ROOT_ANDROID is not set`

**Solution:** Ensure the environment variable is set correctly:
```bash
export GSTREAMER_ROOT_ANDROID=/path/to/gstreamer-1.0-android-universal-1.24.13
```

### NDK Build Failures

**Error:** NDK version mismatch

**Solution:** Install NDK version 25.2.9519653 via Android Studio SDK Manager:
1. Tools → SDK Manager → SDK Tools tab
2. Check "Show Package Details"
3. Install NDK (Side by side) version 25.2.9519653

### Native Library Not Found

**Error:** `java.lang.UnsatisfiedLinkError`

**Solution:**
1. Clean and rebuild: `./gradlew clean build`
2. Verify native libraries are in `app/build/intermediates/merged_native_libs/`
3. Check that `System.loadLibrary()` calls match library names in `Android.mk`

### Audio Permission Denied

**Error:** AudioRecord initialization fails

**Solution:** Ensure `RECORD_AUDIO` permission is granted at runtime (Android 6.0+)

## Contributing

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/your-feature-name`
3. Commit your changes: `git commit -am 'Add new feature'`
4. Push to the branch: `git push origin feature/your-feature-name`
5. Submit a pull request

## License

[Specify your license here]

## Contact

[Add contact information or links to issue tracker]

## Acknowledgments

- GStreamer Project - https://gstreamer.freedesktop.org/
- Android Open Source Project - https://source.android.com/
- Material Design - https://material.io/