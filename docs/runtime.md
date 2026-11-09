# Runtime setup

The SDK DLL is `screenai.dll`. It embeds the ScreenAI 153.5 runtime package, including its native DLLs and model/data files. With the default configuration, the SDK does not read files from a Chrome installation.

When initialized without a runtime path or `SCREENAI_RUNTIME_DIR`, the SDK reads model/data files directly from the embedded package through the ScreenAI file callbacks. It extracts only the native DLLs to `%TMP%\chrome_screen_ai_<version>_<package-id>` so Windows can load them. The cache is reused when the extracted DLLs match their packaged SHA-256 hashes; a changed or incomplete cache entry is replaced.

Set the environment variable or pass a directory to use an external runtime instead:

    $env:SCREENAI_RUNTIME_DIR = 'C:\path\to\screen_ai\153.4'

    .\screenai.exe .\scan.png --runtime-dir 'C:\path\to\screen_ai\153.4'

The C++ wrapper accepts the same directory in its constructor. Python accepts it with `Recognizer(runtime_dir=...)`. An explicit directory or environment variable takes precedence over the embedded package and must contain `chrome_screen_ai.dll` plus matching model files.

The embedded package is generated with `tools/pack_runtime.py` from a runtime directory containing `manifest.json` and `chrome_screen_ai.dll`. CMake embeds the selected archive into `screenai.dll`; set `SCREENAI_RUNTIME_BUNDLE` at configure time to use a different package.

This adapter supports one active recognizer per process because the recovered ScreenAI model callbacks are process-global. Recognition calls through one recognizer are serialized by the SDK.
