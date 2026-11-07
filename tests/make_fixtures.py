#!/usr/bin/env python3
"""Gera MP3s de teste (seno 1 kHz) com tags ID3 variadas e capas, em tests/fixtures/music."""
import io, math, os, struct, sys
import lameenc
from mutagen.id3 import ID3, TIT2, TPE1, TALB, TRCK, APIC, ID3NoHeaderError
from PIL import Image

out = sys.argv[1] if len(sys.argv) > 1 else "fixtures/music"
os.makedirs(out + "/Album A", exist_ok=True)
os.makedirs(out + "/Sem Tag", exist_ok=True)

def sine_mp3(seconds=2.0, hz=44100, freq=1000, bitrate=128, channels=2):
    n = int(seconds * hz)
    pcm = bytearray()
    for i in range(n):
        v = int(12000 * math.sin(2 * math.pi * freq * i / hz))
        pcm += struct.pack("<h", v) * channels
    enc = lameenc.Encoder()
    enc.set_bit_rate(bitrate); enc.set_in_sample_rate(hz); enc.set_channels(channels); enc.set_quality(7)
    return bytes(enc.encode(bytes(pcm)) + enc.flush())

def cover_jpeg(rgb=(200, 30, 30), size=600):
    img = Image.new("RGB", (size, size), rgb)
    for y in range(size):                      # gradiente leve para nao ser chapado
        for x in range(0, size, 8):
            img.putpixel((x, y), (min(255, rgb[0] + y // 10), rgb[1], rgb[2]))
    b = io.BytesIO(); img.save(b, "JPEG", quality=85); return b.getvalue()

def write(path, data): open(path, "wb").write(data)

# 1) com tags UTF-8 (ID3v2.4), capa embutida vermelha
p = out + "/Album A/01 - Cancao Teste.mp3"; write(p, sine_mp3())
t = ID3()
t.add(TIT2(encoding=3, text="Canção Teste")); t.add(TPE1(encoding=3, text="Artista Ñandú"))
t.add(TALB(encoding=3, text="Álbum A")); t.add(TRCK(encoding=3, text="3/12"))
t.add(APIC(encoding=3, mime="image/jpeg", type=3, desc="Capa", data=cover_jpeg()))
t.save(p, v2_version=4)

# 2) tags UTF-16 (ID3v2.3), mesmo album, sem capa embutida (cai no folder.jpg)
p = out + "/Album A/02 - Segunda.mp3"; write(p, sine_mp3(1.5, freq=440))
t = ID3()
t.add(TIT2(encoding=1, text="Segunda Faixa")); t.add(TPE1(encoding=1, text="Artista Ñandú"))
t.add(TALB(encoding=1, text="Álbum A")); t.add(TRCK(encoding=1, text="4"))
t.save(p, v2_version=3)
write(out + "/Album A/folder.jpg", cover_jpeg((20, 60, 200)))

# 3) sem nenhuma tag, mono 22,05 kHz, sem capa
p = out + "/Sem Tag/sem_tag.mp3"; write(p, sine_mp3(1.0, hz=22050, channels=1, bitrate=64))

# 4) so ID3v1 no fim do arquivo
p = out + "/v1only.mp3"; d = sine_mp3(1.0)
tag = b"TAG" + b"Titulo V1".ljust(30, b"\0") + b"Artista V1".ljust(30, b"\0") + b"Album V1".ljust(30, b"\0") + b"2020" + b"\0" * 28 + b"\0" + bytes([7]) + bytes([255])
write(p, d + tag)

# 5) lixo: nao-MP3 com extensao .mp3 e um .txt
write(out + "/quebrado.mp3", b"isto nao e um mp3" * 100)
write(out + "/notas.txt", b"ignorar")
print("fixtures ok em", out)
