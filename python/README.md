# Python bindings

This package is a small `ctypes` wrapper around the same `screenai.dll` used by the C++ API and CLI. It does not load Chrome's `chrome_screen_ai.dll` directly.

Install the binding from this directory with `pip install .`. Install Pillow support with `pip install `.[pillow]`.

Make `screenai.dll` available next to this package or set `SCREENAI_SDK_DLL` to its full path. The DLL requires `runtime/screenai-runtime-153.5.sai` under its own directory. Keep the `runtime` folder beside the DLL when distributing the Python binding.

    from screenai import Recognizer
    from screenai.pil import recognize_pil_result
    from PIL import Image

    with Recognizer() as ocr:
        result = ocr.recognize_file_result('scan.png')
        print(result.text)

        image_result = recognize_pil_result(ocr, Image.open('scan.png'))
        for block in image_result.blocks:
            for line in block.lines:
                print(line.text, line.bounding_box, line.confidence)

`Recognizer.recognize_pixels_result` accepts a byte buffer, dimensions, stride, and either `PixelFormat.RGBA8` or `PixelFormat.BGRA8`, then returns the same structured result. `recognize_file` and `recognize_pixels` remain text-only conveniences. Bounding boxes use pixel coordinates in the processed image; confidence is optional.
