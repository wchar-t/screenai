# Compatibility

The embedded runtime asset package is a snapshot of ScreenAI 153.5 on Windows x64. The adapter's exported function declarations and bitmap layout were verified against Chrome ScreenAI 153.4; the private ABI compatibility of 153.5 has not yet been verified.

At startup, the SDK loads `chrome_screen_ai.dll` by full path and checks for the required exports. That check confirms the expected names exist; it cannot prove that the component uses the same function signatures, bitmap layout, model files, or serialized result format.

The 153.5 package is included so its assets can be preserved and tried without depending on a Chrome installation. If it fails, pass a 153.4 runtime directory with `--runtime-dir`, `SCREENAI_RUNTIME_DIR`, or the API's runtime path. Supporting another component build should mean reviewing its ABI and result format, then adding or updating a versioned adapter and verifying it with that runtime. The public C and C++ SDK interfaces can remain unchanged.
