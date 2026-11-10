from dataclasses import dataclass
from typing import Optional, Tuple


@dataclass(frozen=True)
class BoundingBox:
    x: int
    y: int
    width: int
    height: int
    angle: float


@dataclass(frozen=True)
class OcrLine:
    text: str
    bounding_box: Optional[BoundingBox]
    confidence: Optional[float]


@dataclass(frozen=True)
class OcrBlock:
    lines: Tuple[OcrLine, ...]


@dataclass(frozen=True)
class OcrResult:
    text: str
    image_width: int
    image_height: int
    blocks: Tuple[OcrBlock, ...]
