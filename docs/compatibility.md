# Compatibility

The current adapter targets the Chrome ScreenAI component observed in Chrome 153.4 on Windows x64. Its exported functions and the bitmap layout passed to `PerformOCR` are private implementation details rather than a stable public ABI.

At startup, the SDK loads `chrome_screen_ai.dll` by full path and checks for the required exports. That check confirms the expected names exist; it cannot prove that a different component build uses the same function signatures, bitmap layout, model files, or serialized result format.

Use the tested 153.4 runtime for this adapter. Supporting another component build should mean reviewing its ABI and result format, then adding or updating a versioned adapter and verifying it with that runtime. The public C and C++ SDK interfaces can remain unchanged.
