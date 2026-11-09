# ScreenAI SDK

ScreenAI SDK is a Windows x64 C++ library for running image OCR through the ScreenAI runtime. The SDK DLL requires the matching runtime package beside it and does not need a Chrome installation. The C++ API, Python binding, and CLI use the same shared implementation.

## Build

Requirements: Windows 10 or later, CMake 3.25 or later, and a Visual Studio 2022 C++ toolchain.

Create the fixed ScreenAI 153.5 dependency from its runtime directory:

    py tools/pack_runtime.py "C:\path\to\screen_ai\153.5"

The packer writes `runtime/screenai-runtime-153.5.sai`. This package is ignored by Git and must be present before configuring the project. CMake copies it to `build/Release/runtime` beside `screenai.dll` and installs it under `bin/runtime`.

    cmake -S . -B build -A x64
    cmake --build build --config Release

The build produces `screenai.exe`, `screenai.dll`, and the `screenai.lib` import library. Ship the `runtime` directory beside `screenai.dll`; the DLL requires the exact package filename and version.

## Run OCR

The SDK loads the fixed `runtime/screenai-runtime-153.5.sai` package relative to `screenai.dll`:

    .\build\Release\screenai.exe .\sample.png

The image is decoded with Windows Imaging Component, scaled to the runtime's maximum dimension when needed, and converted to BGRA pixels before OCR.

The current API returns the recognized text as UTF-8. It accepts one active recognizer per process; concurrent calls through that recognizer are serialized.

## Use from C++

Include `screenai/screenai.hpp`, link against the generated `screenai.lib`, and keep `screenai.dll` beside the application or on the DLL search path. Include the required `runtime` folder beside the DLL. The header uses the C ABI exported by the DLL and provides a small RAII wrapper.

See `examples/cpp/ocr_image.cpp` and `docs/runtime.md`.

## Use from Python

The `python/screenai` package calls the same DLL through `ctypes`. Build the SDK first, then make `screenai.dll` available next to the package or set `SCREENAI_SDK_DLL`. Keep the fixed `runtime/screenai-runtime-153.5.sai` dependency beside the DLL.

The optional Pillow helper accepts a Pillow image and forwards its RGBA pixels to the SDK. See `python/README.md`.

## Compatibility

The required runtime package is locked to ScreenAI 153.5. The recovered Chrome ABI and bitmap layout were verified against Chrome 153.4 on Windows x64; compatibility with 153.5 has not yet been verified. See `docs/compatibility.md`.
