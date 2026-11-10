from ._native import PixelFormat


def recognize_pil(recognizer, image):
    return recognize_pil_result(recognizer, image).text


def recognize_pil_result(recognizer, image):
    rgba = image.convert('RGBA')

    return recognizer.recognize_pixels_result(
        rgba.tobytes(),
        rgba.width,
        rgba.height,
        rgba.width * 4,
        PixelFormat.RGBA8,
    )
