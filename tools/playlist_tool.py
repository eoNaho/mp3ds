#!/usr/bin/env python3
"""Organiza playlists no PC e exporta .m3u8 que o mp3ds entende.

Formato do arquivo de trabalho:
{
  "version": 1,
  "music_root": "/home/user/Music",
  "sd_root": "sdmc:/Music",
  "playlists": {
    "Corrida": ["Artist/Album/faixa.mp3", "outra.flac"]
  }
}

Os caminhos no JSON sao relativos a music_root. Assim o mesmo arquivo pode ser
usado em qualquer PC e exportado para outro cartao SD sem editar cada entrada.
"""
from __future__ import annotations

import argparse
import json
import os
import posixpath
import sys
from pathlib import Path


def read_json(path: Path) -> dict:
    if not path.exists():
        return {"version": 1, "music_root": "", "sd_root": "sdmc:/Music", "playlists": {}}
    with path.open("r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, dict) or not isinstance(data.get("playlists", {}), dict):
        raise ValueError("formato invalido: playlists.json deve conter um objeto playlists")
    data.setdefault("version", 1)
    data.setdefault("music_root", "")
    data.setdefault("sd_root", "sdmc:/Music")
    return data


def write_json(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_suffix(path.suffix + ".tmp")
    with tmp.open("w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")
    tmp.replace(path)


def norm_rel(value: str) -> str:
    value = value.replace("\\", "/").strip()
    if value.startswith("sdmc:/"):
        value = value[6:]
        if value.startswith("Music/"):
            value = value[6:]
    value = posixpath.normpath(value)
    return "" if value in (".", "..") or value.startswith("../") else value.lstrip("/")


def rel_path(raw: str, root: Path) -> str:
    p = Path(raw).expanduser()
    if not p.is_absolute():
        p = (root / p).resolve()
    try:
        return norm_rel(p.relative_to(root.resolve()).as_posix())
    except ValueError:
        raise ValueError(f"faixa fora de music_root: {raw}")


def export_m3u(data: dict, output: Path, name: str, style: str) -> int:
    playlists = data["playlists"]
    if name not in playlists:
        raise ValueError(f"playlist nao encontrada: {name}")
    entries = playlists[name]
    root = Path(data.get("music_root") or ".").expanduser().resolve()
    sd_root = data.get("sd_root", "sdmc:/Music").rstrip("/")
    output.parent.mkdir(parents=True, exist_ok=True)
    paths = []
    missing = 0
    for raw in entries:
        rel = norm_rel(str(raw))
        if not rel:
            continue
        if not (root / rel).is_file():
            missing += 1
        if style == "sdmc":
            paths.append(f"{sd_root}/{rel}")
        elif style == "relative":
            paths.append(rel)
        else:
            paths.append(str((root / rel).resolve()).replace(os.sep, "/"))
    with output.open("w", encoding="utf-8", newline="\n") as f:
        f.write("#EXTM3U\n")
        f.write(f"#PLAYLIST:{name}\n")
        f.writelines(p + "\n" for p in paths)
    return missing


def cmd_init(args: argparse.Namespace) -> int:
    data = read_json(args.file)
    data["music_root"] = str(Path(args.music_root).expanduser().resolve())
    data["sd_root"] = args.sd_root
    write_json(args.file, data)
    print(f"criado: {args.file}")
    return 0


def cmd_add(args: argparse.Namespace) -> int:
    data = read_json(args.file)
    root = Path(data.get("music_root") or ".").expanduser().resolve()
    if not data.get("music_root"):
        raise ValueError("defina music_root primeiro com 'init'")
    rel = rel_path(args.track, root)
    if not rel.lower().endswith((".mp3", ".flac")):
        raise ValueError("a faixa precisa ser .mp3 ou .flac")
    items = data["playlists"].setdefault(args.playlist, [])
    if rel not in items:
        items.append(rel)
    write_json(args.file, data)
    print(f"{args.playlist}: {rel}")
    return 0


def cmd_remove(args: argparse.Namespace) -> int:
    data = read_json(args.file)
    root = Path(data.get("music_root") or ".").expanduser().resolve()
    if args.playlist not in data["playlists"]:
        raise ValueError(f"playlist nao encontrada: {args.playlist}")
    rel = norm_rel(args.track)
    if not Path(args.track).is_absolute():
        try:
            rel = rel_path(args.track, root)
        except ValueError:
            pass
    old = data["playlists"][args.playlist]
    data["playlists"][args.playlist] = [x for x in old if norm_rel(str(x)) != rel]
    write_json(args.file, data)
    print(f"removidas: {len(old) - len(data['playlists'][args.playlist])}")
    return 0


def cmd_export(args: argparse.Namespace) -> int:
    data = read_json(args.file)
    names = list(data["playlists"]) if args.playlist == "*" else [args.playlist]
    outdir = Path(args.output)
    for name in names:
        missing = export_m3u(data, outdir / (name + ".m3u8"), name, args.style)
        print(f"exportado: {name} ({len(data['playlists'][name])} faixas)", end="")
        if missing:
            print(f" — {missing} arquivo(s) ausente(s)")
        else:
            print()
    return 0


def cmd_list(args: argparse.Namespace) -> int:
    data = read_json(args.file)
    for name, tracks in data["playlists"].items():
        print(f"{name}: {len(tracks)} faixa(s)")
    return 0


def parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(description="organiza playlists do mp3ds no PC")
    p.add_argument("--file", type=Path, default=Path("playlists.json"), help="arquivo JSON de trabalho")
    sub = p.add_subparsers(dest="command", required=True)
    x = sub.add_parser("init", help="cria/configura o projeto")
    x.add_argument("music_root")
    x.add_argument("--sd-root", default="sdmc:/Music")
    x.set_defaults(func=cmd_init)
    x = sub.add_parser("add", help="adiciona uma faixa")
    x.add_argument("playlist")
    x.add_argument("track")
    x.set_defaults(func=cmd_add)
    x = sub.add_parser("remove", help="remove uma faixa")
    x.add_argument("playlist")
    x.add_argument("track")
    x.set_defaults(func=cmd_remove)
    x = sub.add_parser("list", help="lista playlists")
    x.set_defaults(func=cmd_list)
    x = sub.add_parser("export", help="exporta para m3u8")
    x.add_argument("playlist", help="nome ou * para todas")
    x.add_argument("output", help="pasta de saida, normalmente sdmc:/3ds/mp3ds/playlists")
    x.add_argument("--style", choices=("sdmc", "relative", "absolute"), default="sdmc")
    x.set_defaults(func=cmd_export)
    return p


def main() -> int:
    try:
        args = parser().parse_args()
        return args.func(args)
    except (OSError, ValueError, json.JSONDecodeError) as e:
        print(f"erro: {e}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
