# ScreenAI SDK

ScreenAI SDK is a Windows x64 C++ library for running image OCR through the ScreenAI runtime. The SDK DLL embeds the ScreenAI 153.5 runtime package, so applications do not need to use a Chrome installation. The C++ API, Python binding, and CLI all use the same shared implementation.

## Build

Requirements: Windows 10 or later, CMake 3.25 or later, and a Visual Studio 2022 C++ toolchain.

The repository includes `runtime/screenai-runtime-153.5.sai`, a package made from the ScreenAI 153.5 runtime directory. CMake embeds it into `screenai.dll`.

    cmake -S . -B build -A x64
    cmake --build build --config Release

The build produces `screenai.exe`, `screenai.dll`, and the `screenai.lib` import library. The executable and SDK DLL are placed together in `build/Release` for a multi-configuration generator. The SDK DLL is large because it contains the runtime assets.

To create a package from another runtime directory:

    py tools/pack_runtime.py "C:\path\to\screen_ai\153.5" runtime\screenai-runtime-153.5.sai

Then configure with `-DSCREENAI_RUNTIME_BUNDLE=<path-to-package>`.

## Run OCR

The embedded runtime is used by default:

    .\build\Release\screenai.exe .\sample.png

To use an external runtime directory instead, pass it on the command line or set the environment variable:

    .\build\Release\screenai.exe .\sample.png --runtime-dir 'C:\path\to\screen_ai\153.4'
    $env:SCREENAI_RUNTIME_DIR = 'C:\path\to\screen_ai\153.4'

The image is decoded with Windows Imaging Component, scaled to the runtime's maximum dimension when needed, and converted to BGRA pixels before OCR.

The current API returns the recognized text as UTF-8. It accepts one active recognizer per process; concurrent calls through that recognizer are serialized.

## Use from C++

Include `screenai/screenai.hpp`, link against the generated `screenai.lib`, and keep `screenai.dll` beside the application or on the DLL search path. The header uses the C ABI exported by the DLL and provides a small RAII wrapper.

See `examples/cpp/ocr_image.cpp` and `docs/runtime.md`.

## Use from Python

The `python/screenai` package calls the same DLL through `ctypes`. Build the SDK first, then make `screenai.dll` available next to the package or set `SCREENAI_SDK_DLL`. The embedded runtime is used unless `runtime_dir` or `SCREENAI_RUNTIME_DIR` selects an external runtime.

The optional Pillow helper accepts a Pillow image and forwards its RGBA pixels to the SDK. See `python/README.md`.

## Compatibility

The embedded asset package comes from ScreenAI 153.5. The recovered Chrome ABI and bitmap layout were verified against Chrome 153.4 on Windows x64; compatibility with 153.5 has not yet been verified. See `docs/compatibility.md`.
