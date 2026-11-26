#!/usr/bin/env python3
"""Generate the GitHub wiki from the documentation in docs/.

The pages in docs/ are the single source of truth: they also build the MkDocs site. This script
turns them into a flat set of GitHub wiki pages, following the navigation in mkdocs.yml:

  * every page in the nav becomes a wiki page named after its nav title
    ("Axes and ticks" -> Axes-and-ticks.md); docs/index.md becomes Home.md;
  * links between pages are rewritten to wiki links, keeping #anchors;
  * images are copied to images/ in the wiki and their paths rewritten;
  * the first-level heading is dropped, because the wiki shows the page name as the title;
  * _Sidebar.md (the navigation) and _Footer.md are generated.

Usage:
    python3 tools/make_wiki.py OUTPUT_DIR [--repo OWNER/NAME]

OUTPUT_DIR is normally a clone of <repo>.wiki.git. Pages this script generated earlier but that are
no longer in the nav are removed; other files in OUTPUT_DIR are left alone.

Requires PyYAML.
"""

from __future__ import annotations

import argparse
import posixpath
import re
import shutil
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
DOCS = ROOT / "docs"
MKDOCS = ROOT / "mkdocs.yml"
MARKER = "<!-- Generated from docs/ by tools/make_wiki.py. Do not edit in the wiki. -->"
DEFAULT_REPO = "StochasticCactus/CactusPlot"


def wiki_name(title: str) -> str:
    """Turn a nav title into a wiki page file name (without .md)."""
    name = re.sub(r"[^\w\s-]", " ", title)  # drop punctuation such as . , / ( )
    name = re.sub(r"\s+", "-", name.strip())
    return name


def read_nav() -> tuple[list[tuple[str, list[tuple[str, str]]]], dict[str, str]]:
    """Return the nav as [(section, [(title, docs path)])] and a map docs path -> wiki name."""
    # mkdocs.yml may use !!python tags in other setups; ignore unknown tags.
    class Loader(yaml.SafeLoader):
        pass

    Loader.add_multi_constructor("", lambda loader, suffix, node: None)
    config = yaml.load(MKDOCS.read_text(encoding="utf-8"), Loader=Loader)

    sections: list[tuple[str, list[tuple[str, str]]]] = []
    pages: dict[str, str] = {}
    loose: list[tuple[str, str]] = []
    for entry in config["nav"]:
        (title, value), = entry.items()
        if isinstance(value, str):
            loose.append((title, value))
        else:
            items = []
            for sub in value:
                (sub_title, path), = sub.items()
                items.append((sub_title, path))
            sections.append((title, items))

    for title, path in loose + [item for _, items in sections for item in items]:
        pages[path] = "Home" if path == "index.md" else wiki_name(title)

    if len(set(pages.values())) != len(pages):
        sys.exit("error: two nav entries give the same wiki page name")

    # Top-level entries other than Home go into a group at the end of the sidebar.
    rest = [(t, p) for t, p in loose if p != "index.md"]
    if rest:
        sections.append(("More", rest))
    return sections, pages


LINK = re.compile(r"(!?)\[((?:[^\[\]]|\[[^\]]*\])*)\]\(([^)\s]+)(\s+\"[^\"]*\")?\)")
IMG_TAG = re.compile(r'(<img\b[^>]*\bsrc=")([^"]+)(")')
FENCE = re.compile(r"^\s*(```|~~~)")


def convert(text: str, src: str, pages: dict[str, str], repo: str, images: set[str],
            problems: list[str]) -> str:
    base = posixpath.dirname(src)

    def target(url: str, is_image: bool) -> str:
        if re.match(r"^[a-z][a-z0-9+.-]*:", url, re.I) or url.startswith("//"):
            return url
        if url.startswith("#"):
            return url
        path, _, anchor = url.partition("#")
        resolved = posixpath.normpath(posixpath.join(base, path))
        if is_image or not resolved.endswith(".md"):
            if not (DOCS / resolved).is_file():
                problems.append(f"{src}: missing file {url}")
                return url
            if resolved.startswith("images/"):
                images.add(resolved)
                return resolved
            # Anything else (e.g. the man page) is linked in the repository.
            return f"https://github.com/{repo}/blob/main/docs/{resolved}"
        if resolved not in pages:
            problems.append(f"{src}: link to a page that is not in the nav: {url}")
            return url
        return pages[resolved] + (f"#{anchor}" if anchor else "")

    def fix_line(line: str) -> str:
        line = LINK.sub(lambda m: f"{m[1]}[{m[2]}]({target(m[3], m[1] == '!')}{m[4] or ''})",
                        line)
        line = IMG_TAG.sub(lambda m: m[1] + target(m[2], True) + m[3], line)
        return line

    out: list[str] = []
    in_code = False
    dropped_title = False
    for line in text.splitlines():
        if FENCE.match(line):
            in_code = not in_code
            out.append(line)
            continue
        if in_code:
            out.append(line)
            continue
        if not dropped_title and line.startswith("# "):
            dropped_title = True
            continue
        if not dropped_title and line.strip():
            dropped_title = True  # the page has no leading title
        out.append(fix_line(line))

    while out and not out[0].strip():
        out.pop(0)
    return "\n".join(out).rstrip() + "\n"


def sidebar(sections, pages) -> str:
    lines = [MARKER, "", "**[Home](Home)**", ""]
    for title, items in sections:
        lines.append(f"**{title}**")
        lines.append("")
        for item_title, path in items:
            lines.append(f"- [{item_title}]({pages[path]})")
        lines.append("")
    return "\n".join(lines)


def footer(repo: str) -> str:
    return (f"{MARKER}\n\n"
            f"CactusPlot is free software under the GNU GPL version 2 or later · "
            f"[Repository](https://github.com/{repo}) · "
            f"[Releases](https://github.com/{repo}/releases) · "
            f"[Report a problem](https://github.com/{repo}/issues)  \n"
            f"This wiki is generated from the [`docs/` folder](https://github.com/{repo}/tree/main/docs). "
            f"To change a page, edit it there and open a pull request.\n")


def home_header(repo: str) -> str:
    return ('<p align="center"><img src="images/logo.png" width="96" alt="CactusPlot logo"/></p>\n\n'
            '<p align="center"><b>Beautiful, fast 2D scientific plotting &mdash; '
            'a modern successor to XMGrace.</b><br/>\n'
            'Windows · macOS · Linux (X11 and Wayland) · GPL-2.0-or-later · '
            f'<a href="https://github.com/{repo}/releases">Download</a></p>\n\n')


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("output", type=Path, help="directory to write the wiki into")
    parser.add_argument("--repo", default=DEFAULT_REPO,
                        help=f"GitHub repository, OWNER/NAME (default {DEFAULT_REPO})")
    args = parser.parse_args()
    out: Path = args.output
    out.mkdir(parents=True, exist_ok=True)

    sections, pages = read_nav()
    images: set[str] = {"images/logo.png"}
    problems: list[str] = []
    written: set[str] = set()

    for path, name in pages.items():
        source = DOCS / path
        if not source.is_file():
            problems.append(f"mkdocs.yml: nav entry {path} does not exist")
            continue
        body = convert(source.read_text(encoding="utf-8"), path, pages, args.repo, images,
                       problems)
        if name == "Home":
            body = home_header(args.repo) + body
        (out / f"{name}.md").write_text(f"{MARKER}\n\n{body}", encoding="utf-8")
        written.add(f"{name}.md")

    (out / "_Sidebar.md").write_text(sidebar(sections, pages), encoding="utf-8")
    (out / "_Footer.md").write_text(footer(args.repo), encoding="utf-8")
    written |= {"_Sidebar.md", "_Footer.md"}

    # Remove pages generated earlier that are gone from the nav.
    for old in out.glob("*.md"):
        if old.name not in written and old.read_text(encoding="utf-8").startswith(MARKER):
            old.unlink()
            print(f"removed {old.name}")

    image_dir = out / "images"
    if image_dir.is_dir():
        shutil.rmtree(image_dir)
    for image in sorted(images):
        dest = out / image
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(DOCS / image, dest)

    for problem in problems:
        print(f"error: {problem}", file=sys.stderr)
    if problems:
        sys.exit(1)
    print(f"wrote {len(written)} pages and {len(images)} images to {out}")


if __name__ == "__main__":
    main()
