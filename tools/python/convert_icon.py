from pathlib import Path
import sys

from PIL import Image


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit("usage: convert_icon.py INPUT.webp OUTPUT.ico")
    source = Path(sys.argv[1])
    target = Path(sys.argv[2])
    target.parent.mkdir(parents=True, exist_ok=True)

    with Image.open(source) as image:
        image = image.convert("RGBA")
        sizes = [256, 128, 64, 48, 32, 16]
        frames = [image.resize((size, size), Image.Resampling.LANCZOS) for size in sizes]
        frames[0].save(target, format="ICO", sizes=[(size, size) for size in sizes])


if __name__ == "__main__":
    main()
