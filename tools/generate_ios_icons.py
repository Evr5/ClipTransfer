from __future__ import annotations

from pathlib import Path

from PIL import Image


def main() -> None:
    repo_root = Path(__file__).resolve().parents[1]
    src = repo_root / "res" / "logo.png"
    out_dir = repo_root / "platform" / "ios" / "icons"
    out_dir.mkdir(parents=True, exist_ok=True)

    img = Image.open(src).convert("RGBA")

    # Common iOS icon sizes (pixels).
    sizes: dict[str, int] = {
        "Icon-20@2x.png": 40,
        "Icon-20@3x.png": 60,
        "Icon-29@2x.png": 58,
        "Icon-29@3x.png": 87,
        "Icon-40@2x.png": 80,
        "Icon-40@3x.png": 120,
        "Icon-60@2x.png": 120,
        "Icon-60@3x.png": 180,
        "Icon-76@1x.png": 76,
        "Icon-76@2x.png": 152,
        "Icon-83.5@2x.png": 167,
        "Icon-1024.png": 1024,
    }

    for name, size in sizes.items():
        resized = img.resize((size, size), Image.Resampling.LANCZOS)

        # Flatten alpha onto white background (iOS icons should not have transparency).
        bg = Image.new("RGBA", (size, size), (255, 255, 255, 255))
        bg.alpha_composite(resized)
        final = bg.convert("RGB")
        final.save(out_dir / name, format="PNG", optimize=True)

    print(f"Generated {len(sizes)} icons in {out_dir}")


if __name__ == "__main__":
    main()
