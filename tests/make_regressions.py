"""Fixtures para metadados, caminhos UTF-8 e audio LAME, fora da biblioteca base."""
from pathlib import Path
import io, shutil, struct, subprocess, wave, math
from PIL import Image

root = Path('fixtures/regression')
root.mkdir(parents=True, exist_ok=True)
audio = Path('fixtures/music/Sem Tag/sem_tag.mp3').read_bytes()
stereo = Path('fixtures/music/Album A/02 - Segunda.mp3').read_bytes()
# Remove a tag ID3v2 para montar frames em ordem deliberada.
if stereo[:3] == b'ID3':
    size = sum((stereo[6+i] & 127) << (21-7*i) for i in range(4))
    stereo = stereo[10+size:]
def sync(n): return bytes((n >> shift) & 127 for shift in (21,14,7,0))
def frame(key, data): return key.encode() + sync(len(data)) + b'\0\0' + data
def text(key, value): return frame(key, b'\x03' + value.encode() + b'\0')
def image(color):
    out = io.BytesIO(); Image.new('RGB', (80, 80), color).save(out, 'JPEG'); return out.getvalue()
def pic(kind, color): return frame('APIC', b'\x03image/jpeg\0' + bytes([kind]) + b'\0' + image(color))
def tagged(path, tags):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(b'ID3\x04\0\0' + sync(len(tags)) + tags + stereo)
common = text('TALB', '日本のコンピレーション') + text('TPE2', 'Various Artists') + text('TRCK','1/20')
tagged(root/'compilation/one.mp3', common + text('TPE1','宇多田ヒカル') + text('TPOS','1/2') + text('TIT2','日本語'*80))
tagged(root/'compilation/two.mp3', common + text('TPE1','別の歌手') + text('TPOS','2/2') + text('TIT2','Český Řeka Łódź') + pic(4,'red') + pic(3,'blue'))
tagged(root/'utf8.mp3', text('TIT2','宇多田ヒカル'*60) + text('TPE1','歌手'*70) + text('TALB','アルバム'*50))
long = root/'paths'
for i in range(8): long = long / ('日本語'*15 + str(i))
long.mkdir(parents=True, exist_ok=True)
(long/'song.mp3').write_bytes(audio)
# O prefixo e valido ate 1023 bytes; o proximo diretorio deve contar como ignorado.
missing = root/'missing'
missing.mkdir(exist_ok=True)
(missing/'song.mp3').write_bytes(audio)
# Um WAV com quantidade exata de amostras para validar delay/padding LAME.
with wave.open(str(root/'gapless.wav'), 'wb') as w:
    w.setnchannels(2); w.setsampwidth(2); w.setframerate(44100)
    w.writeframes(b''.join(struct.pack('<hh', int(9000*math.sin(i*0.1)), int(9000*math.sin(i*0.1))) for i in range(44100)))
subprocess.run(['lame','--silent', str(root/'gapless.wav'), str(root/'gapless.mp3')], check=True)
shutil.copyfile(root/'gapless.mp3', root/'gapless2.mp3')
