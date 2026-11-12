# Python bindings

This package is a small `ctypes` wrapper around the same `screenai.dll` used by the C++ API and CLI. It does not load Chrome's `chrome_screen_ai.dll` directly.

Install the binding from this directory with `py -m pip install .`. Install Pillow support with `py -m pip install ".[pillow]"`.

Make `screenai.dll` available next to this package or set `SCREENAI_SDK_DLL` to its full path. The DLL requires `runtime/screenai-runtime-153.7.sai` under its own directory. Keep the `runtime` folder beside the DLL when distributing the Python binding. For `batch_worker_count` greater than one, also keep the built `screenai_worker.exe` beside the DLL; the SDK uses it as its persistent worker host.

    from screenai import Recognizer
    from screenai.pil import recognize_pil
    from PIL import Image

    with Recognizer() as ocr:
        result = ocr.recognize('scan.png')
        print(result.text)

        image_result = recognize_pil(ocr, Image.open('scan.png'))
        for block in image_result.blocks:
            for line in block.lines:
                print(line.text, line.bounding_box, line.confidence)

`Recognizer(batch_worker_count=1)` keeps the existing single-process behavior by default. `recognize_batch(paths)` returns one `OcrResult` or `None` per input path, in input order. A worker count above one starts persistent worker processes when the recognizer is created; batch calls distribute work across the local recognizer and those processes. `recognize_pixels` and `recognize_pil` also return `OcrResult`. Bounding boxes use pixel coordinates in the processed image; confidence is optional.
