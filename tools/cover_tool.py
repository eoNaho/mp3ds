#!/usr/bin/env python3
"""Reconhece MP3s com SongRec e adiciona as capas nas tags ID3.

Uso típico:
    python3 tools/cover_tool.py music/Biblioteca --dry-run
    python3 tools/cover_tool.py music/Biblioteca

O SongRec faz o reconhecimento usando o áudio. A capa é baixada da resposta
JSON do Shazam e gravada como APIC no MP3, formato que o mp3ds já entende.
Por segurança, arquivos que já têm capa são ignorados; use --force para
substituí-las.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import mimetypes
from pathlib import Path
import subprocess
import sys
import time
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.request import Request, urlopen


MAX_IMAGE_BYTES = 8 * 1024 * 1024
DEFAULT_USER_AGENT = "mp3ds-cover-tool/1.0 (+https://github.com/)"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Reconhece MP3s com SongRec e incorpora as capas nas tags ID3."
    )
    parser.add_argument("root", type=Path, help="pasta com os MP3s")
    parser.add_argument(
        "--songrec",
        default="songrec",
        help="executável do SongRec (padrão: songrec)",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="substitui capas APIC que já existirem",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="reconhece e baixa, mas não altera os MP3s",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="processa no máximo N arquivos (0 = todos)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=30.0,
        help="timeout para baixar cada capa em segundos (padrão: 30)",
    )
    parser.add_argument(
        "--delay",
        type=float,
        default=2.0,
        help="espera entre reconhecimentos para reduzir o risco de limite (padrão: 2)",
    )
    parser.add_argument(
        "--request-interval",
        type=int,
        default=1,
        help="intervalo solicitado ao SongRec (padrão: 1 segundo)",
    )
    parser.add_argument(
        "--cache-dir",
        type=Path,
        default=None,
        help="cache das capas baixadas (padrão: <root>/.cover-cache)",
    )
    return parser.parse_args()


def mp3_files(root: Path) -> list[Path]:
    return sorted(
        (path for path in root.rglob("*") if path.is_file() and path.suffix.lower() == ".mp3"),
        key=lambda path: str(path).casefold(),
    )


def has_embedded_cover(path: Path) -> bool:
    from mutagen.id3 import ID3, ID3NoHeaderError

    try:
        tags = ID3(str(path))
    except ID3NoHeaderError:
        return False
    return bool(tags.getall("APIC"))


def run_songrec(
    executable: str, path: Path, timeout: float, request_interval: int
) -> dict[str, Any] | None:
    """Executa SongRec e retorna a resposta JSON, ou None quando sem match."""
    try:
        result = subprocess.run(
            [
                executable,
                "recognize",
                "--json",
                "--request-interval",
                str(request_interval),
                str(path),
            ],
            check=False,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as exc:
        raise RuntimeError(f"SongRec excedeu o timeout de {timeout:g}s") from exc
    except OSError as exc:
        raise RuntimeError(
            f"não consegui executar '{executable}': {exc}. "
            "Instale o SongRec ou use --songrec /caminho/songrec."
        ) from exc

    output = result.stdout.strip()
    if not output:
        detail = result.stderr.strip()
        if result.returncode != 0 and "no match" not in detail.lower():
            raise RuntimeError(
                f"SongRec terminou com código {result.returncode} para {path}"
                + (f": {detail}" if detail else "")
            )
        return None

    try:
        value = json.loads(output)
    except json.JSONDecodeError as exc:
        # O SongRec deve imprimir JSON em stdout. Mostrar stderr ajuda a
        # diagnosticar versões antigas ou instalação via Flatpak.
        detail = result.stderr.strip()
        raise RuntimeError(
            f"JSON inválido retornado pelo SongRec para {path}: {exc}"
            + (f"\nstderr: {detail}" if detail else "")
        ) from exc

    if not isinstance(value, dict) or not isinstance(value.get("track"), dict):
        return None
    return value


def track_info(response: dict[str, Any]) -> tuple[str, str, str | None, str] | None:
    track = response.get("track")
    if not isinstance(track, dict):
        return None

    title = track.get("title")
    artist = track.get("subtitle")
    key = track.get("key", "?")
    if not isinstance(title, str) or not isinstance(artist, str):
        return None

    images = track.get("images")
    url: str | None = None
    if isinstance(images, dict):
        for candidate in (images.get("coverarthq"), images.get("coverart")):
            if isinstance(candidate, str) and candidate.startswith(("https://", "http://")):
                url = candidate
                break
    if url is None:
        share = track.get("share")
        if isinstance(share, dict) and isinstance(share.get("image"), str):
            candidate = share["image"]
            if candidate.startswith(("https://", "http://")):
                url = candidate
    return title, artist, url, str(key)


def image_extension(data: bytes, content_type: str | None = None) -> str:
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        return ".png"
    if data.startswith(b"\xff\xd8\xff"):
        return ".jpg"
    if data.startswith((b"GIF87a", b"GIF89a")):
        return ".gif"
    if len(data) >= 12 and data[:4] == b"RIFF" and data[8:12] == b"WEBP":
        return ".webp"
    if content_type:
        ext = mimetypes.guess_extension(content_type.split(";", 1)[0].strip())
        if ext:
            return ext
    raise ValueError("resposta não parece ser uma imagem suportada")


def download_image(url: str, cache_dir: Path, timeout: float) -> bytes:
    cache_dir.mkdir(parents=True, exist_ok=True)
    stem = hashlib.sha256(url.encode("utf-8")).hexdigest()
    for cached in cache_dir.glob(stem + ".*"):
        data = cached.read_bytes()
        if data:
            return data

    request = Request(url, headers={"User-Agent": DEFAULT_USER_AGENT})
    try:
        with urlopen(request, timeout=timeout) as response:
            content_type = response.headers.get("Content-Type")
            length = response.headers.get("Content-Length")
            if length and int(length) > MAX_IMAGE_BYTES:
                raise ValueError(f"imagem maior que {MAX_IMAGE_BYTES // (1024 * 1024)} MiB")
            chunks: list[bytes] = []
            total = 0
            while True:
                chunk = response.read(64 * 1024)
                if not chunk:
                    break
                total += len(chunk)
                if total > MAX_IMAGE_BYTES:
                    raise ValueError(f"imagem maior que {MAX_IMAGE_BYTES // (1024 * 1024)} MiB")
                chunks.append(chunk)
    except (HTTPError, URLError, TimeoutError) as exc:
        raise RuntimeError(f"falha ao baixar capa: {exc}") from exc

    data = b"".join(chunks)
    ext = image_extension(data, content_type)
    cache_path = cache_dir / (stem + ext)
    cache_path.write_bytes(data)
    return data


def embed_cover(path: Path, data: bytes) -> None:
    from mutagen.id3 import APIC, ID3, ID3NoHeaderError

    try:
        tags = ID3(str(path))
    except ID3NoHeaderError:
        tags = ID3()

    tags.delall("APIC")
    mime = {
        ".jpg": "image/jpeg",
        ".jpeg": "image/jpeg",
        ".png": "image/png",
        ".gif": "image/gif",
        ".webp": "image/webp",
    }
    ext = image_extension(data)
    tags.add(APIC(encoding=3, mime=mime.get(ext, "image/jpeg"), type=3, desc="Cover", data=data))

    # O mp3ds suporta ID3v2.2/2.3/2.4. Mutagen mantém as tags existentes e
    # grava ID3v2.4 quando um arquivo ainda não tinha cabeçalho ID3.
    tags.save(str(path))


def main() -> int:
    args = parse_args()
    root = args.root.expanduser().resolve()
    if not root.is_dir():
        print(f"erro: pasta não encontrada: {root}", file=sys.stderr)
        return 2
    if args.limit < 0:
        print("erro: --limit não pode ser negativo", file=sys.stderr)
        return 2
    if args.delay < 0 or args.timeout <= 0 or args.request_interval < 1:
        print("erro: --delay >= 0, --timeout > 0 e --request-interval >= 1", file=sys.stderr)
        return 2

    try:
        import mutagen  # noqa: F401
    except ImportError:
        print("erro: instale a dependência com: python3 -m pip install mutagen", file=sys.stderr)
        return 2

    files = mp3_files(root)
    if args.limit:
        files = files[: args.limit]
    cache_dir = (args.cache_dir or root / ".cover-cache").expanduser().resolve()

    stats = {"done": 0, "skipped": 0, "no_match": 0, "no_cover": 0, "failed": 0}
    for index, path in enumerate(files, 1):
        relative = path.relative_to(root)
        if not args.force and has_embedded_cover(path):
            stats["skipped"] += 1
            print(f"[{index}/{len(files)}] ignorado (já tem capa): {relative}")
            continue

        try:
            response = run_songrec(args.songrec, path, args.timeout, args.request_interval)
            if response is None:
                stats["no_match"] += 1
                print(f"[{index}/{len(files)}] sem reconhecimento: {relative}")
                continue
            info = track_info(response)
            if info is None:
                stats["no_match"] += 1
                print(f"[{index}/{len(files)}] resposta sem faixa: {relative}")
                continue
            title, artist, url, key = info
            if url is None:
                stats["no_cover"] += 1
                print(f"[{index}/{len(files)}] sem URL de capa: {artist} - {title} ({relative})")
                continue
            data = download_image(url, cache_dir, args.timeout)
            if not args.dry_run:
                embed_cover(path, data)
            stats["done"] += 1
            action = "seria atualizada" if args.dry_run else "atualizada"
            print(f"[{index}/{len(files)}] {action}: {artist} - {title} [Shazam {key}] ({relative})")
        except (RuntimeError, ValueError, OSError) as exc:
            stats["failed"] += 1
            print(f"[{index}/{len(files)}] falhou: {relative}: {exc}", file=sys.stderr)
        if args.delay and index < len(files):
            time.sleep(args.delay)

    print(
        "Resumo: "
        f"{stats['done']} atualizadas, "
        f"{stats['skipped']} já tinham capa, "
        f"{stats['no_match']} sem reconhecimento, "
        f"{stats['no_cover']} sem capa, "
        f"{stats['failed']} falharam."
    )
    return 1 if stats["failed"] else 0


if __name__ == "__main__":
    raise SystemExit(main())
