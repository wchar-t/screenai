from ._native import PixelFormat


def recognize_pil(recognizer, image):
    rgba = image.convert('RGBA')

    return recognizer.recognize_pixels(
        rgba.tobytes(),
        rgba.width,
        rgba.height,
        rgba.width * 4,
        PixelFormat.RGBA8,
    )
