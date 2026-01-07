from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageOps


@dataclass(frozen=True)
class AndroidDensity:
    name: str
    size_px: int


DENSITIES: list[AndroidDensity] = [
    AndroidDensity("mipmap-mdpi", 48),
    AndroidDensity("mipmap-hdpi", 72),
    AndroidDensity("mipmap-xhdpi", 96),
    AndroidDensity("mipmap-xxhdpi", 144),
    AndroidDensity("mipmap-xxxhdpi", 192),
]


def _square_icon(logo: Image.Image, size_px: int) -> Image.Image:
    # Keep transparency for Android launcher icons.
    logo_rgba = logo.convert("RGBA")

    # Fit the logo into the square while keeping aspect ratio.
    fitted = ImageOps.contain(logo_rgba, (size_px, size_px), method=Image.Resampling.LANCZOS)

    canvas = Image.new("RGBA", (size_px, size_px), (0, 0, 0, 0))
    x = (size_px - fitted.width) // 2
    y = (size_px - fitted.height) // 2
    canvas.paste(fitted, (x, y), fitted)
    return canvas


def _round_mask(size_px: int) -> Image.Image:
    mask = Image.new("L", (size_px, size_px), 0)
    draw = ImageDraw.Draw(mask)
    draw.ellipse((0, 0, size_px - 1, size_px - 1), fill=255)
    return mask


def main() -> None:
    repo_root = Path(__file__).resolve().parents[1]

    input_logo = repo_root / "res" / "logo.png"
    if not input_logo.exists():
        raise SystemExit(f"Input logo not found: {input_logo}")

    android_res_dir = repo_root / "platform" / "android" / "res"

    logo = Image.open(input_logo)

    generated = 0
    for density in DENSITIES:
        out_dir = android_res_dir / density.name
        out_dir.mkdir(parents=True, exist_ok=True)

        square = _square_icon(logo, density.size_px)
        square.save(out_dir / "ic_launcher.png", format="PNG")

        # For round icon, Android expects a separate asset. Using a circular mask gives a nicer result.
        round_mask = _round_mask(density.size_px)
        round_icon = square.copy()
        round_icon.putalpha(ImageChops.multiply(square.getchannel("A"), round_mask))
        round_icon.save(out_dir / "ic_launcher_round.png", format="PNG")

        generated += 2

    print(f"Generated {generated} Android launcher icons in {android_res_dir}")


if __name__ == "__main__":
    main()
