#!/usr/bin/env python3
"""Embed the standalone viewer sources into a C++17 header."""

import argparse
from pathlib import Path
import sys


ORDERED_JS = ("model.js", "dom.js", "combat.js", "screens.js", "viewer.js")
DELIMITERS = {"html": "STS_REPORT_HTML", "css": "STS_REPORT_CSS", "js": "STS_REPORT_JS"}


def raw_string(name, content):
    delimiter = DELIMITERS[name]
    if f"){delimiter}\"" in content:
        raise ValueError(f"raw-string delimiter collision in {name} resource")
    return f'inline constexpr char {name}[] = R"{delimiter}({content}){delimiter}";\n'


def generate(source_dir, output):
    source = Path(source_dir)
    html = (source / "viewer.html").read_text(encoding="utf-8")
    css = (source / "viewer.css").read_text(encoding="utf-8")
    js = "\n".join((source / filename).read_text(encoding="utf-8") for filename in ORDERED_JS)
    header = "#ifndef STS_REPORT_RESOURCES_H\n#define STS_REPORT_RESOURCES_H\nnamespace sts::report::resources {\n"
    for name, content in (("html", html), ("css", css), ("js", js)):
        header += raw_string(name, content)
    header += "}\n#endif\n"
    destination = Path(output)
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(header, encoding="utf-8")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True)
    parser.add_argument("--source-dir", default="report/viewer")
    args = parser.parse_args(argv)
    try:
        generate(args.source_dir, args.output)
    except (OSError, ValueError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
