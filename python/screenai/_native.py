import ctypes
import os
from enum import IntEnum
from pathlib import Path


class PixelFormat(IntEnum):
    BGRA8 = 1
    RGBA8 = 2


def _load_library():
    candidates = []
    configured_path = os.environ.get('SCREENAI_SDK_DLL')

    if configured_path:
        candidates.append(Path(configured_path))

    package_dir = Path(__file__).resolve().parent

    candidates.append(package_dir / 'screenai.dll')
    candidates.append(package_dir / 'bin' / 'screenai.dll')

    for candidate in candidates:
        if candidate.is_file():
            return ctypes.WinDLL(str(candidate))

    try:
        return ctypes.WinDLL('screenai.dll')
    except OSError as error:
        searched = ', '.join(str(candidate) for candidate in candidates)
        message = (
            'Could not load the ScreenAI SDK DLL. Set SCREENAI_SDK_DLL or place '
            f'screenai.dll next to this package. Searched: {searched}'
        )
        raise OSError(message) from error


class Recognizer:
    def __init__(self, runtime_dir=None):
        if os.name != 'nt':
            raise OSError('ScreenAI currently supports Windows only.')

        self._library = _load_library()
        self._configure_functions()

        self._handle = ctypes.c_void_p()
        error = ctypes.create_string_buffer(1024)
        runtime_path = os.fsdecode(os.fspath(runtime_dir)) if runtime_dir is not None else None

        status = self._library.screenai_create(
            runtime_path,
            ctypes.byref(self._handle),
            error,
            len(error),
        )

        self._check(status, error)

    def _configure_functions(self):
        char_pointer = ctypes.POINTER(ctypes.c_char)

        self._library.screenai_create.argtypes = [
            ctypes.c_wchar_p,
            ctypes.POINTER(ctypes.c_void_p),
            char_pointer,
            ctypes.c_size_t,
        ]
        self._library.screenai_create.restype = ctypes.c_int

        self._library.screenai_destroy.argtypes = [ctypes.c_void_p]
        self._library.screenai_destroy.restype = None

        self._library.screenai_get_max_image_dimension.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint32),
            char_pointer,
            ctypes.c_size_t,
        ]
        self._library.screenai_get_max_image_dimension.restype = ctypes.c_int

        self._library.screenai_recognize_file.argtypes = [
            ctypes.c_void_p,
            ctypes.c_wchar_p,
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.POINTER(ctypes.c_size_t),
            char_pointer,
            ctypes.c_size_t,
        ]
        self._library.screenai_recognize_file.restype = ctypes.c_int

        self._library.screenai_recognize_pixels.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
            ctypes.c_uint32,
            ctypes.c_uint32,
            ctypes.c_uint32,
            ctypes.c_int,
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.POINTER(ctypes.c_size_t),
            char_pointer,
            ctypes.c_size_t,
        ]
        self._library.screenai_recognize_pixels.restype = ctypes.c_int

        self._library.screenai_free_buffer.argtypes = [ctypes.c_void_p]
        self._library.screenai_free_buffer.restype = None

    @staticmethod
    def _check(status, error):
        if status != 0:
            message = error.value.decode('utf-8', errors='replace')
            raise RuntimeError(message or f'ScreenAI failed with status {status}.')

    def get_max_image_dimension(self):
        dimension = ctypes.c_uint32()
        error = ctypes.create_string_buffer(1024)

        status = self._library.screenai_get_max_image_dimension(
            self._handle,
            ctypes.byref(dimension),
            error,
            len(error),
        )

        self._check(status, error)

        return dimension.value

    def recognize_file(self, image_path):
        output = ctypes.c_void_p()
        output_length = ctypes.c_size_t()
        error = ctypes.create_string_buffer(1024)

        status = self._library.screenai_recognize_file(
            self._handle,
            os.fsdecode(os.fspath(image_path)),
            ctypes.byref(output),
            ctypes.byref(output_length),
            error,
            len(error),
        )

        self._check(status, error)

        try:
            return ctypes.string_at(output, output_length.value).decode('utf-8')
        finally:
            self._library.screenai_free_buffer(output)

    def recognize_pixels(
        self,
        pixels,
        width,
        height,
        stride,
        pixel_format=PixelFormat.RGBA8,
    ):
        pixel_bytes = bytes(pixels)

        if not pixel_bytes:
            raise ValueError('Pixel buffer must not be empty.')

        pixel_array = (ctypes.c_uint8 * len(pixel_bytes)).from_buffer_copy(pixel_bytes)
        output = ctypes.c_void_p()
        output_length = ctypes.c_size_t()
        error = ctypes.create_string_buffer(1024)

        status = self._library.screenai_recognize_pixels(
            self._handle,
            pixel_array,
            len(pixel_bytes),
            width,
            height,
            stride,
            int(pixel_format),
            ctypes.byref(output),
            ctypes.byref(output_length),
            error,
            len(error),
        )

        self._check(status, error)

        try:
            return ctypes.string_at(output, output_length.value).decode('utf-8')
        finally:
            self._library.screenai_free_buffer(output)

    def close(self):
        if getattr(self, '_handle', None):
            self._library.screenai_destroy(self._handle)

            self._handle = ctypes.c_void_p()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_value, traceback):
        self.close()

    def __del__(self):
        self.close()
