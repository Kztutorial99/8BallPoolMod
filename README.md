# 8 Ball Pool Mod

Android JNI/C++ mod for 8 Ball Pool. Injected as a native shared library, it overlays an ImGui-based HUD on top of the game to provide aim assistance, auto-play, and ball detection features.

---

## Features

### AutoPlay
Automatically aims and shoots for you. Uses physics simulation to find the best valid shot each turn.
- Toggle: **Enable AutoPlay** switch inside the menu
- Works in both 8-ball and 9-ball game modes
- Calls `AutoAim::AIM()` each frame when it is your turn

### Enemy Line ESP
Draws aim-prediction lines for opponent balls so you can see which pockets they are threatening.
- Toggle: **Enemy Line** switch inside the menu
- In 8-ball: shows lines only for the opponent's ball class (solids vs stripes)
- In 9-ball: shows lines for all numbered balls

### 9-Ball One Shoot
Manual aim helper for 9-ball mode. Calculates the optimal shot angle once per turn and shows a floating button.
- Toggle: **9-Ball One Shoot** switch inside the menu
- Spinning arc animation = calculating; play triangle = shot found
- Independent of AutoPlay — works as a standalone visual cue
- Calculation runs **once per turn** (not per frame) to avoid lag

### Draggable Floating Button
A floating hamburger-icon button that opens/closes the main menu.
- Drag anywhere on screen using touch
- Red glow ring pulses to indicate it is active
- Position is remembered across frames

### Draggable Main Menu
The main mod menu can be freely repositioned on screen.
- Drag the title bar to move it
- Opens/closes via the floating button

---

## Prerequisites

| Tool | Version |
|------|---------|
| Android SDK | Platform 33, Build-tools 33.0.2 |
| Android NDK | r22.1 (`22.1.7171670`) |
| Java | JDK 8+ |
| Gradle | Bundled wrapper (`./gradlew`) |

> The SDK and NDK paths are set in `local.properties`. Update `sdk.dir` and `ndk.dir` to match your local installation before building.

```
sdk.dir=/path/to/android-sdk
ndk.dir=/path/to/android-sdk/ndk/22.1.7171670
```

---

## Build

```bash
export ANDROID_SDK_ROOT=/path/to/android-sdk
export JAVA_HOME=/path/to/jdk

./gradlew assembleDebug --no-daemon
```

Output APK:
```
app/build/outputs/apk/debug/app-debug.apk
```

For a release build (minified + shrunk resources):
```bash
./gradlew assembleRelease --no-daemon
```

---

## Project Structure

```
app/src/main/
├── jni/
│   ├── menu.h          # All HUD/UI code — ImGui menus, ESP, NineBall, AutoPlay
│   ├── main.cpp        # Entry point, IsShotValid(), g_CurrentCandidate
│   ├── Android.mk      # NDK build script
│   └── game/
│       ├── Ball.h              # Ball struct, Classification enum
│       ├── GameManager.h       # is9BallGame(), getPlayerClassification()
│       └── inc/
│           ├── AutoAim.h       # AutoAim::AIM(), setAimAngle(), shouldAutoAIM()
│           ├── AutoPlay.h      # AutoPlay state variables
│           └── Prediction.h    # Physics sim, determineShotResult(), getPockets()
└── java/               # Android loader / injector
```

---

## Development Notes

### Skip login during testing
In `app/src/main/jni/menu.h` line ~40:
```cpp
static bool DEBUG_BYPASS_LOGIN = true;
```
Set to `false` before releasing to require valid token authentication.

### NineBall namespace placement
`NineBall::Update()` **must** be called from `DrawMenu()` (~line 1148), **not** from `DrawESP()`. The `NineBall` namespace is defined at ~line 583, after the `AutoAim.h` include. Calling it from `DrawESP()` (~line 289) causes an undeclared-identifier compile error.

### Architecture
The mod targets **arm64-v8a** only (`abiFilters` in `app/build.gradle`).

---

## License

See [LICENSE](LICENSE).
