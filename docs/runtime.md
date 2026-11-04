# Runtime setup

The SDK DLL is `screenai.dll`. It loads Chrome's `chrome_screen_ai.dll` from the runtime directory you configure. The runtime directory also supplies the model files requested by ScreenAI's file callbacks.

Set the environment variable for repeated use:

    $env:SCREENAI_RUNTIME_DIR = 'C:\path\to\screen_ai\153.4'

Or pass the directory directly to the CLI:

    .\screenai.exe .\scan.png --runtime-dir 'C:\path\to\screen_ai\153.4'

The C++ wrapper accepts the same directory in its constructor. Python accepts it with `Recognizer(runtime_dir=...)`. If no explicit directory is passed, the SDK reads `SCREENAI_RUNTIME_DIR`.

The directory must contain `chrome_screen_ai.dll` and the model files that match that DLL. The SDK does not download or copy runtime files. Keep the SDK's `screenai.dll` beside the executable, or make it available through the normal Windows DLL search path.

This adapter supports one active recognizer per process because the recovered ScreenAI model callbacks are process-global. Recognition calls through one recognizer are serialized by the SDK.
