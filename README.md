# ScreenAI SDK

ScreenAI SDK is a Windows x64 C++ library for running image OCR through the ScreenAI runtime installed with Chrome. It provides one shared implementation for C++ applications, Python callers, and the included command-line program.

The project does not include Chrome's runtime DLL or model files. Point the SDK at the directory containing them with `SCREENAI_RUNTIME_DIR` or the `--runtime-dir` option.

## Build

Requirements: Windows 10 or later, CMake 3.25 or later, and a Visual Studio 2022 C++ toolchain.

From a Visual Studio developer PowerShell:

    cmake -S . -B build -A x64
    cmake --build build --config Release

The build produces `screenai.exe`, `screenai.dll`, and the `screenai.lib` import library. The executable and SDK DLL are placed together in `build/Release` for a multi-configuration generator.

## Run OCR

Set the runtime directory and pass an image path:

    $env:SCREENAI_RUNTIME_DIR = 'C:\path\to\screen_ai\153.4'
    .\build\Release\screenai.exe .\sample.png

The image is decoded with Windows Imaging Component, scaled to the runtime's maximum dimension when needed, and converted to BGRA pixels before OCR.

The current API returns the recognized text as UTF-8. It accepts one active recognizer per process; concurrent calls through that recognizer are serialized.

## Use from C++

Include `screenai/screenai.hpp`, link against the generated `screenai.lib`, and keep `screenai.dll` beside the application or on the DLL search path. The header uses the C ABI exported by the DLL and provides a small RAII wrapper.

See `examples/cpp/ocr_image.cpp` and `docs/runtime.md`.

## Use from Python

The `python/screenai` package calls the same DLL through `ctypes`. Build the SDK first, then make `screenai.dll` available next to the package or set `SCREENAI_SDK_DLL`. The Chrome runtime location can be passed to `Recognizer` or supplied through `SCREENAI_RUNTIME_DIR`.

The optional Pillow helper accepts a Pillow image and forwards its RGBA pixels to the SDK. See `python/README.md`.

## Compatibility

The recovered Chrome ABI and bitmap layout were verified against the ScreenAI component in Chrome 153.4 on Windows x64. The ABI is private to Chrome and can change between component versions. See `docs/compatibility.md` before using a different runtime.
