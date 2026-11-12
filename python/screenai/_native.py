import ctypes
import os
from enum import IntEnum
from pathlib import Path

from .types import BoundingBox, OcrBlock, OcrLine, OcrResult


class PixelFormat(IntEnum):
    BGRA8 = 1
    RGBA8 = 2


class _BoundingBoxView(ctypes.Structure):
    _fields_ = [
        ('x', ctypes.c_int32),
        ('y', ctypes.c_int32),
        ('width', ctypes.c_int32),
        ('height', ctypes.c_int32),
        ('angle', ctypes.c_float),
    ]


class _LineView(ctypes.Structure):
    _fields_ = [
        ('text', ctypes.POINTER(ctypes.c_char)),
        ('text_length', ctypes.c_size_t),
        ('bounding_box', _BoundingBoxView),
        ('has_bounding_box', ctypes.c_int32),
        ('confidence', ctypes.c_float),
        ('has_confidence', ctypes.c_int32),
    ]


class _BlockView(ctypes.Structure):
    _fields_ = [
        ('lines', ctypes.POINTER(_LineView)),
        ('line_count', ctypes.c_size_t),
    ]


class _OcrResultView(ctypes.Structure):
    _fields_ = [
        ('text', ctypes.POINTER(ctypes.c_char)),
        ('text_length', ctypes.c_size_t),
        ('image_width', ctypes.c_uint32),
        ('image_height', ctypes.c_uint32),
        ('blocks', ctypes.POINTER(_BlockView)),
        ('block_count', ctypes.c_size_t),
    ]


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
    def __init__(self, batch_worker_count=1):
        if os.name != 'nt':
            raise OSError('ScreenAI currently supports Windows only.')
        if (
            not isinstance(batch_worker_count, int)
            or not 0 < batch_worker_count <= 0x7FFFFFFF
        ):
            raise ValueError('batch_worker_count must be a positive signed 32-bit integer.')

        self._library = _load_library()
        self._configure_functions()

        self._handle = ctypes.c_void_p()
        error = ctypes.create_string_buffer(1024)

        status = self._library.screenai_create(
            batch_worker_count,
            ctypes.byref(self._handle),
            error,
            len(error),
        )

        self._check(status, error)

    def _configure_functions(self):
        char_pointer = ctypes.POINTER(ctypes.c_char)

        self._library.screenai_create.argtypes = [
            ctypes.c_int32,
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

        self._library.screenai_recognize.argtypes = [
            ctypes.c_void_p,
            ctypes.c_wchar_p,
            ctypes.POINTER(ctypes.c_void_p),
            char_pointer,
            ctypes.c_size_t,
        ]
        self._library.screenai_recognize.restype = ctypes.c_int

        self._library.screenai_recognize_batch.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_wchar_p),
            ctypes.c_size_t,
            ctypes.POINTER(ctypes.c_void_p),
            ctypes.c_size_t,
            char_pointer,
            ctypes.c_size_t,
        ]
        self._library.screenai_recognize_batch.restype = ctypes.c_int

        self._library.screenai_recognize_pixels.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(ctypes.c_uint8),
            ctypes.c_size_t,
            ctypes.c_uint32,
            ctypes.c_uint32,
            ctypes.c_uint32,
            ctypes.c_int,
            ctypes.POINTER(ctypes.c_void_p),
            char_pointer,
            ctypes.c_size_t,
        ]
        self._library.screenai_recognize_pixels.restype = ctypes.c_int

        self._library.screenai_ocr_result_get_view.argtypes = [ctypes.c_void_p]
        self._library.screenai_ocr_result_get_view.restype = ctypes.POINTER(_OcrResultView)

        self._library.screenai_ocr_result_destroy.argtypes = [ctypes.c_void_p]
        self._library.screenai_ocr_result_destroy.restype = None

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

    def recognize(self, image_path):
        result = ctypes.c_void_p()
        error = ctypes.create_string_buffer(1024)

        status = self._library.screenai_recognize(
            self._handle,
            os.fsdecode(os.fspath(image_path)),
            ctypes.byref(result),
            error,
            len(error),
        )

        self._check(status, error)

        try:
            return self._copy_result(result)
        finally:
            self._library.screenai_ocr_result_destroy(result)

    def recognize_batch(self, image_paths):
        paths = [os.fsdecode(os.fspath(image_path)) for image_path in image_paths]

        if not paths:
            return []

        path_array = (ctypes.c_wchar_p * len(paths))(*paths)
        result_array = (ctypes.c_void_p * len(paths))()
        error = ctypes.create_string_buffer(1024)

        status = self._library.screenai_recognize_batch(
            self._handle,
            path_array,
            len(paths),
            result_array,
            len(result_array),
            error,
            len(error),
        )

        self._check(status, error)

        try:
            return [
                self._copy_result(result_handle) if result_handle else None
                for result_handle in result_array
            ]
        finally:
            for result_handle in result_array:
                if result_handle:
                    self._library.screenai_ocr_result_destroy(result_handle)

    def _copy_result(self, result_handle):
        view_pointer = self._library.screenai_ocr_result_get_view(result_handle)

        if not view_pointer:
            raise RuntimeError('Could not read the OCR result.')

        view = view_pointer.contents
        text = self._copy_text(view.text, view.text_length)
        blocks = []

        for block_index in range(view.block_count):
            source_block = view.blocks[block_index]
            lines = []

            for line_index in range(source_block.line_count):
                source_line = source_block.lines[line_index]
                line_text = self._copy_text(source_line.text, source_line.text_length)
                bounding_box = self._copy_bounding_box(
                    source_line.bounding_box,
                    source_line.has_bounding_box,
                )

                line_confidence = source_line.confidence if source_line.has_confidence else None

                lines.append(OcrLine(
                    line_text,
                    bounding_box,
                    line_confidence,
                ))

            blocks.append(OcrBlock(
                tuple(lines),
            ))

        return OcrResult(
            text,
            view.image_width,
            view.image_height,
            tuple(blocks),
        )

    @staticmethod
    def _copy_text(value, length):
        if not length:
            return ''

        return ctypes.string_at(value, length).decode('utf-8')

    @staticmethod
    def _copy_bounding_box(source, is_present):
        if not is_present:
            return None

        return BoundingBox(
            source.x,
            source.y,
            source.width,
            source.height,
            source.angle,
        )

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
        result = ctypes.c_void_p()
        error = ctypes.create_string_buffer(1024)

        status = self._library.screenai_recognize_pixels(
            self._handle,
            pixel_array,
            len(pixel_bytes),
            width,
            height,
            stride,
            int(pixel_format),
            ctypes.byref(result),
            error,
            len(error),
        )

        self._check(status, error)

        try:
            return self._copy_result(result)
        finally:
            self._library.screenai_ocr_result_destroy(result)

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
