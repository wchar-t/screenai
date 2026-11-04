# Python bindings

This package is a small `ctypes` wrapper around the same `screenai.dll` used by the C++ API and CLI. It does not load Chrome's `chrome_screen_ai.dll` directly.

Install the binding from this directory with `pip install .`. Install Pillow support with `pip install '.[pillow]'`.

Make `screenai.dll` available next to this package or set `SCREENAI_SDK_DLL` to its full path. Set `SCREENAI_RUNTIME_DIR` to the Chrome ScreenAI runtime directory, or pass `runtime_dir` when creating a recognizer.

    from screenai import Recognizer
    from screenai.pil import recognize_pil
    from PIL import Image

    with Recognizer() as ocr:
        print(ocr.recognize_file('scan.png'))
        print(recognize_pil(ocr, Image.open('scan.png')))

`Recognizer.recognize_pixels` accepts a byte buffer, dimensions, stride, and either `PixelFormat.RGBA8` or `PixelFormat.BGRA8`. The supplied bytes must remain valid only for the duration of that call.
