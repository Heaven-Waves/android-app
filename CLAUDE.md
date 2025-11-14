3# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

HeavenWaves is an Android application targeting Android 10+ (API 29-36). The project uses Java with Gradle build system and the Android Gradle Plugin (AGP) 8.13.0.

**Key Details:**
- Package: `com.justivo.heavenwaves`
- Min SDK: `29` (Android 10)
- Target SDK: `36`
- Compile SDK: `36`
- Java Version: `11`
- GStreamer Version: `1.24.13`
- NDK Version: `25.2.9519653`
- Main Activity: `MainActivity.java` with native method declaration for GStreamer integration

## Build Commands

### Building the Project
```bash
./gradlew build
```

### Running Tests
```bash
# Unit tests (run on JVM)
./gradlew test

# Instrumented tests (require Android device/emulator)
./gradlew connectedAndroidTest
```

### Running Specific Tests
```bash
# Run a specific test class
./gradlew test --tests com.justivo.heavenwaves.ExampleUnitTest

# Run a specific test method
./gradlew test --tests com.justivo.heavenwaves.ExampleUnitTest.addition_isCorrect
```

### Other Useful Commands
```bash
# Clean build artifacts
./gradlew clean

# Assemble debug APK
./gradlew assembleDebug

# Assemble release APK
./gradlew assembleRelease

# Install debug build on connected device
./gradlew installDebug

# Lint checks
./gradlew lint
```

## Project Structure

```
HeavenWaves/
├── app/
│   ├── src/
│   │   ├── main/
│   │   │   ├── java/com/justivo/heavenwaves/
│   │   │   │   └── MainActivity.java        # Main entry point
│   │   │   ├── res/                          # Android resources
│   │   │   │   ├── layout/
│   │   │   │   ├── values/
│   │   │   │   └── ...
│   │   │   └── AndroidManifest.xml
│   │   ├── test/                             # Unit tests
│   │   └── androidTest/                      # Instrumented tests
│   └── build.gradle                          # App module build config
├── gradle/
│   └── libs.versions.toml                    # Centralized dependency versions
├── build.gradle                              # Root project build config
└── settings.gradle                           # Project settings
```

## Architecture Notes

### Native Integration (GStreamer)
The project integrates GStreamer 1.x multimedia framework via JNI/NDK:

**Native Module:** `audio_bridge`
- Built using ndk-build with Android.mk and Application.mk
- Source files: `native-audio-bridge.cpp`, `hello-world.cpp`
- Target ABIs: armeabi-v7a, arm64-v8a, x86, x86_64
- Links against: `gstreamer_android`, `-llog`, `-landroid`, `-liconv`

**Java Interface:**
- `GStreamer.java`: Helper class for initialization
- `MainActivity.java`: Loads native libraries (`gstreamer_android`, `audio_bridge`) and calls native methods

**Native Methods:**
- `nativeInit()`: Initializes GStreamer runtime
- `nativeGetGStreamerInfo()`: Returns GStreamer version information

**Environment Requirements:**
- `GSTREAMER_ROOT_ANDROID` environment variable must point to GStreamer Android SDK
- NDK version 25.2.9519653 required for builds

### Dependency Management
The project uses Gradle Version Catalogs (libs.versions.toml) for centralized dependency management. When adding dependencies:
1. Add version to `[versions]` section
2. Add library reference to `[libraries]` section
3. Reference in app/build.gradle as `implementation libs.library-name`

### Android Resources
The app uses standard Android resource structure with:
- Edge-to-edge display support
- Material Design components
- Window insets handling for modern Android UI
- Night theme support (values-night/)

## Development Notes

- The project is configured for Java 11 source/target compatibility
- ProGuard is disabled in release builds (minifyEnabled false)
- Test instrumentation runner: AndroidJUnitRunner
- Dependencies include AndroidX AppCompat, Material Design, Activity, and ConstraintLayout
- Native code uses C++ (`.cpp` files) rather than C