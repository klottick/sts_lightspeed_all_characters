#!/usr/bin/env python3
"""Explicitly populate a local cache of optional Slay the Spire artwork."""

import argparse
import json
import sys
import urllib.request
from pathlib import Path
from urllib.parse import urlparse


ATTRIBUTION = "Game artwork © Mega Crit; sourced from Slay the Spire Wiki"
SIGNATURES = (
    (b"\x89PNG\r\n\x1a\n", ".png"),
    (b"\xff\xd8\xff", ".jpg"),
    (b"RIFF", ".webp"),
)


class AssetError(Exception):
    pass


def image_extension(data):
    for signature, extension in SIGNATURES:
        if data.startswith(signature) and (extension != ".webp" or data[8:12] == b"WEBP"):
            return extension
    raise AssetError("unsupported image signature (PNG, JPEG, or WebP required; SVG is not allowed)")


def read_sources(path):
    try:
        sources = json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError) as exc:
        raise AssetError(f"cannot read source catalog: {exc}") from exc
    if not isinstance(sources, dict) or not sources:
        raise AssetError("source catalog must be a non-empty object")
    for key, url in sources.items():
        if not isinstance(key, str) or not key or ".." in key.split("/") or key.startswith("/"):
            raise AssetError(f"unsafe asset key: {key!r}")
        parsed = urlparse(url if isinstance(url, str) else "")
        filename = Path(parsed.path).name
        if (parsed.scheme != "https" or not parsed.netloc or not filename or
                filename in (".", "..") or ".." in parsed.path.split("/")):
            raise AssetError(f"unsafe asset source URL for {key}")
    return sources


def checked_data(path):
    data = path.read_bytes()
    ext = image_extension(data)
    return data, ext


def run(sources_path, output_path, local_dir=None):
    sources = read_sources(sources_path)
    output = Path(output_path)
    local = Path(local_dir).resolve() if local_dir else None
    if local is not None and not local.is_dir():
        raise AssetError(f"local asset directory does not exist: {local}")

    allowed = {"manifest.json"}
    for url in sources.values():
        stem = Path(urlparse(url).path).stem
        allowed.update(stem + extension for extension in (".png", ".jpg", ".webp"))
    if output.exists():
        if output.is_symlink():
            raise AssetError(f"output directory must not be a symlink: {output}")
        if not output.is_dir():
            raise AssetError(f"output is not a directory: {output}")
        extras = [p.name for p in output.iterdir() if p.name not in allowed]
        if extras:
            raise AssetError(f"output directory is not empty of unrelated files: {', '.join(sorted(extras))}")
        for entry in output.iterdir():
            if entry.is_symlink():
                raise AssetError(f"output entries must not be symlinks: {entry}")
    else:
        output.mkdir(parents=True)

    assets = {}
    warnings = []
    for key, url in sources.items():
        filename = Path(urlparse(url).path).name
        target = output / filename
        if local is not None:
            candidate = (local / filename).resolve()
            if candidate.parent != local and local not in candidate.parents:
                raise AssetError(f"unsafe local asset path for {key}")
            if not candidate.is_file():
                warnings.append(f"warning: missing local asset for {key}: {filename}")
                continue
            data, extension = checked_data(candidate)
        elif any((output / (Path(filename).stem + ext)).is_file() for ext in (".png", ".jpg", ".webp")):
            cached = next(output / (Path(filename).stem + ext) for ext in (".png", ".jpg", ".webp")
                          if (output / (Path(filename).stem + ext)).is_file())
            data, extension = checked_data(cached)
            filename = cached.name
            target = cached
        else:
            try:
                request = urllib.request.Request(url, headers={"User-Agent": "sts-report-asset-preparer/1.0"})
                with urllib.request.urlopen(request, timeout=30) as response:
                    data = response.read()
                extension = image_extension(data)
            except AssetError:
                raise
            except Exception as exc:
                raise AssetError(f"failed to download {url}: {exc}") from exc
        if extension != ".png":
            filename = Path(filename).stem + extension
            target = output / filename
        if target.exists() and not target.is_file():
            raise AssetError(f"asset destination is not a regular file: {target}")
        if local is not None or not target.exists():
            if target.exists():
                if target.read_bytes() != data:
                    raise AssetError(f"refusing to overwrite different existing artwork: {target}")
            else:
                with target.open('xb') as stream:
                    stream.write(data)
        assets[key] = {"path": filename, "source": url}

    manifest = {"schemaVersion": 1, "assets": assets, "attribution": ATTRIBUTION}
    manifest_path = output / "manifest.json"
    content = json.dumps(manifest, indent=2, ensure_ascii=False) + "\n"
    if manifest_path.exists():
        if manifest_path.read_text(encoding="utf-8") != content:
            raise AssetError(f"refusing to overwrite a different existing manifest: {manifest_path}")
    else:
        with manifest_path.open('x', encoding='utf-8') as stream:
            stream.write(content)
    return warnings


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sources", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--local-dir")
    try:
        args = parser.parse_args(argv)
    except SystemExit as exc:
        return int(exc.code)
    try:
        warnings = run(args.sources, args.output, args.local_dir)
    except (AssetError, OSError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1 if isinstance(exc, AssetError) else 2
    for warning in warnings:
        print(warning, file=sys.stderr)
    print(f"Wrote {Path(args.output) / 'manifest.json'} ({len(json.loads((Path(args.output) / 'manifest.json').read_text())['assets'])} assets)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
