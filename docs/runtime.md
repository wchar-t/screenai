# Runtime setup

The SDK DLL is `screenai.dll`. It requires the exact sidecar package `runtime/screenai-runtime-153.5.sai`, located under the same directory as the DLL. The SDK searches only that fixed path and checks that the package version is 153.5.

The package remains a deployment dependency; it is not embedded in `screenai.dll`. The loader memory-maps the package and serves model/data files directly from it through ScreenAI's file callbacks. It extracts only native DLLs to `%TMP%\chrome_screen_ai_<version>_<package-id>` so Windows can load them. Existing extracted DLLs are reused when their SHA-256 hashes match the package.

CMake requires the fixed package at `runtime/screenai-runtime-153.5.sai`, copies it under the build output's `runtime` directory beside `screenai.dll`, and installs it under `bin/runtime`. Include that `runtime` folder when distributing the SDK DLL.

Generate the local package from the matching Chrome ScreenAI 153.5 runtime directory:

    py tools/pack_runtime.py 'C:\path\to\screen_ai\153.5'

The packer accepts only the source directory. It checks that `manifest.json` reports version 153.5 and writes the fixed filename under the repository's `runtime` directory. That directory is ignored by Git.

This adapter supports one active recognizer per process because the recovered ScreenAI model callbacks are process-global. Recognition calls through one recognizer are serialized by the SDK.
