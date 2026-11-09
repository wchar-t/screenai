# Compatibility

The SDK requires the ScreenAI 153.5 package at the fixed path `runtime/screenai-runtime-153.5.sai` beside `screenai.dll`. The package filename and version are locked by the build and loader; the SDK does not scan for other package versions.

The adapter's exported function declarations and bitmap layout were verified against Chrome ScreenAI 153.4. Compatibility of the private ABI with 153.5 has not yet been verified. At startup, the SDK checks for the expected exports, but their presence cannot prove matching function signatures, bitmap layout, model files, or serialized result format.

The 153.5 package is kept as a separate dependency so its assets can be preserved without relying on a Chrome installation. If it fails, an external 153.4 runtime directory can be selected for testing with `--runtime-dir`, `SCREENAI_RUNTIME_DIR`, or the API runtime path. Supporting another bundled release requires changing the fixed version and filename together and reviewing the ABI and result format.