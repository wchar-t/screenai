import sys

from PIL import Image

from screenai import Recognizer
from screenai.pil import recognize_pil_result


def main():
    if len(sys.argv) != 2:
        print('Usage: python ocr_image.py <image-path>', file=sys.stderr)

        return 1

    with Recognizer() as recognizer:
        image = Image.open(sys.argv[1])

        result = recognize_pil_result(recognizer, image)

        print(result.text)

        for block in result.blocks:
            for line in block.lines:
                if line.bounding_box:
                    box = line.bounding_box

                    print(f'line box: {box.x}, {box.y}, {box.width}, {box.height}')

                if line.confidence is not None:
                    print(f'line confidence: {line.confidence:.3f}')

    return 0


if __name__ == '__main__':
    raise SystemExit(main())
