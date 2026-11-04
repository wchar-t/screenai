import sys

from PIL import Image

from screenai import Recognizer
from screenai.pil import recognize_pil


def main():
    if len(sys.argv) != 2:
        print('Usage: python ocr_image.py <image-path>', file=sys.stderr)

        return 1

    with Recognizer() as recognizer:
        image = Image.open(sys.argv[1])

        print(recognize_pil(recognizer, image))

    return 0



if __name__ == '__main__':
    raise SystemExit(main())
