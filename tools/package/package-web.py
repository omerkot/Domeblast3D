#!/usr/bin/env python3
"""Package only the public game files for a Cloudflare Pages direct upload."""
from pathlib import Path
from zipfile import ZIP_DEFLATED, ZipFile

ROOT = Path(__file__).resolve().parents[2]
FILES = (
    "index.html", "app.js", "input.mjs", "xr.mjs", "xr-math.mjs", "controller-mapping.mjs", "controller-setup.mjs", "touch-controls.mjs", "style.css",
    "domeblast.js", "domeblast.wasm", "domeblast.data", "title.png", "browser-display.mjs", "manifest.webmanifest", "app-icon.png", "LICENSE", "LICENSE-ASSETS",
)


def main():
    # An explicit allowlist keeps source files and native caches out of the upload.
    for name in FILES:
        path = ROOT / "dist" / name
        if not path.is_file():
            raise SystemExit(f"Missing {name}: run npm run build first.")
        if path.stat().st_size > 25 * 1024 * 1024:
            raise SystemExit(f"{name} exceeds the Cloudflare Pages asset size limit.")

    output = ROOT / "build" / "domeblast-web-upload.zip"
    output.parent.mkdir(exist_ok=True)
    temporary = output.with_suffix(".zip.tmp")
    try:
        with ZipFile(temporary, "w", ZIP_DEFLATED) as archive:
            for name in FILES:
                archive.write(ROOT / "dist" / name, name)
        with ZipFile(temporary) as archive:
            if set(archive.namelist()) != set(FILES) or archive.testzip():
                raise RuntimeError("Web archive verification failed.")
        temporary.replace(output)
    finally:
        temporary.unlink(missing_ok=True)
    print(f"Upload package: {output} ({output.stat().st_size / 1048576:.1f} MiB)")


if __name__ == "__main__":
    main()
