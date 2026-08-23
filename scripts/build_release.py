#!/usr/bin/env python3
"""Build versioned OTA and merged factory images for every board profile."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys


PROJECT_DIR = Path(__file__).resolve().parents[1]
BOARD_ENVIRONMENTS = (
    "esp32-s3-supermini",
    "esp32-s3-zero",
    "esp32-c3-supermini",
    "esp32-c3-zero",
    "seeed-xiao-esp32-s3",
    "adafruit-qtpy-esp32-s3-n4r2",
    "adafruit-qtpy-esp32-s3-nopsram",
    "esp32-s3-tiny",
)


def firmware_version() -> str:
    version_header = (PROJECT_DIR / "include" / "Version.h").read_text(encoding="utf-8")
    match = re.search(r'FirmwareVersion\[\]\s*=\s*"([^"]+)"', version_header)
    if not match:
        raise RuntimeError("FirmwareVersion was not found in include/Version.h")
    return match.group(1)


def platformio_command(explicit: str | None) -> str:
    command = explicit or os.environ.get("PLATFORMIO_CMD")
    if command:
        return command
    for candidate in ("pio", "platformio"):
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    raise RuntimeError("PlatformIO was not found; install it or pass --pio <path>")


def run_build(pio: str, environment: str) -> None:
    print(f"\n=== Building {environment} ===", flush=True)
    subprocess.run(
        [pio, "run", "--project-dir", str(PROJECT_DIR), "-e", environment],
        check=True,
    )


def flash_segments(build_dir: Path) -> list[tuple[int, Path]]:
    metadata_path = build_dir / "flash_manifest.json"
    if not metadata_path.is_file():
        raise RuntimeError(f"Missing flash layout manifest after build: {metadata_path}")
    metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
    images = metadata.get("segments")
    if not isinstance(images, list) or not images:
        raise RuntimeError(f"Incomplete flash layout in {metadata_path}")

    segments: list[tuple[int, Path]] = []
    for image in images:
        try:
            offset = int(str(image["offset"]), 0)
            path = Path(image["path"])
        except (KeyError, TypeError, ValueError) as error:
            raise RuntimeError(f"Invalid flash image entry in {metadata_path}: {image}") from error
        segments.append((offset, path))
    return sorted(segments, key=lambda item: item[0])


def merge_factory_image(segments: list[tuple[int, Path]], destination: Path) -> None:
    previous_end = 0
    prepared: list[tuple[int, bytes, Path]] = []
    for offset, path in segments:
        if not path.is_file():
            raise RuntimeError(f"Missing flash segment: {path}")
        data = path.read_bytes()
        if not data:
            raise RuntimeError(f"Flash segment is empty: {path}")
        if offset < previous_end:
            raise RuntimeError(f"Flash segment overlaps a previous segment: {path}")
        prepared.append((offset, data, path))
        previous_end = offset + len(data)

    image = bytearray(b"\xff" * previous_end)
    for offset, data, _ in prepared:
        image[offset : offset + len(data)] = data
    destination.write_bytes(image)


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_release_tag(version: str) -> None:
    tag = os.environ.get("GITHUB_REF_NAME", "")
    ref_type = os.environ.get("GITHUB_REF_TYPE", "")
    if ref_type == "tag" and tag != f"v{version}":
        raise RuntimeError(
            f"Git tag {tag!r} does not match FirmwareVersion {version!r}; expected v{version}"
        )


def parse_arguments() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pio", help="Path to the PlatformIO pio/platformio executable")
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=PROJECT_DIR / "dist",
        help="Base output directory; a v<version> subdirectory is created",
    )
    parser.add_argument(
        "--environment",
        action="append",
        choices=BOARD_ENVIRONMENTS,
        dest="environments",
        help="Build only this board environment; may be specified more than once",
    )
    parser.add_argument(
        "--skip-build",
        action="store_true",
        help="Package existing .pio build output without rebuilding",
    )
    return parser.parse_args()


def main() -> int:
    args = parse_arguments()
    version = firmware_version()
    validate_release_tag(version)
    pio = platformio_command(args.pio)
    environments = tuple(args.environments or BOARD_ENVIRONMENTS)
    release_dir = args.output_dir.resolve() / f"v{version}"
    release_dir.mkdir(parents=True, exist_ok=True)
    created: list[Path] = []

    for environment in environments:
        if not args.skip_build:
            run_build(pio, environment)
        build_dir = PROJECT_DIR / ".pio" / "build" / environment
        firmware = build_dir / "firmware.bin"
        if not firmware.is_file():
            raise RuntimeError(f"Missing application image after build: {firmware}")

        base_name = f"VinylChroma-v{version}-{environment}"
        ota_image = release_dir / f"{base_name}-ota.bin"
        factory_image = release_dir / f"{base_name}-factory.bin"
        shutil.copyfile(firmware, ota_image)
        merge_factory_image(flash_segments(build_dir), factory_image)
        created.extend((factory_image, ota_image))
        print(f"Created {factory_image.name} and {ota_image.name}", flush=True)

    checksum_file = release_dir / "SHA256SUMS.txt"
    checksum_file.write_text(
        "".join(f"{sha256(path)}  {path.name}\n" for path in sorted(created)),
        encoding="utf-8",
        newline="\n",
    )
    print(f"\nRelease assets: {release_dir}")
    print(f"Created {len(created)} images and {checksum_file.name}")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        raise SystemExit(1)
