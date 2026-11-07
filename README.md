# mp3ds — MP3 player leve para Nintendo 3DS

Player homebrew focado em MP3, com biblioteca lida do cartão SD, capas com cache, playlists `.m3u`
e visualizador simples.

## Estado do projeto
| Parte | Estado |
|---|---|
| ID3 (v2.2/2.3/2.4 + v1), duração, decoder MP3, visualizador, fila, playlists, biblioteca/índice, cache de capas | **Testado no PC** (`make test`, com ASan/UBSan) |
| Áudio via `ndsp`, capas na GPU, UI `citro2d`, loop principal | **Compilado para 3DS, ainda não testado no hardware** |

## Compilar
1. Instale o devkitPro (Arch: AUR `devkitpro-pacman`, depois `sudo dkp-pacman -S 3ds-dev`).
2. `export DEVKITPRO=/opt/devkitpro DEVKITARM=/opt/devkitpro/devkitARM`
3. `make` → gera `mp3ds.3dsx` (e `.smdh`).
4. Copie `mp3ds.3dsx` para `sdmc:/3ds/` e abra pelo Homebrew Launcher.

Para gerar o arquivo instalável, instale também `makerom` e `bannertool` e execute
`make cia`. O arquivo `mp3ds.cia` pode ser copiado para o cartão SD e instalado
com FBI em um 3DS com desbloqueio compatível. O identificador do título está em
`cia/mp3ds.rsf`.

Sem instalar nada: dê push no GitHub; o workflow `.github/workflows/build.yml` compila num container `devkitpro/devkitarm`.

## Requisitos no console
- `dspfirm.cdc` dumpado (app **DSP1**), senão o `ndsp` não inicia — o app avisa na tela.
- MP3 em `sdmc:/Music` (mude `MUSIC_ROOT` em `source/config.h`).
- Dados do app em `sdmc:/3ds/mp3ds/` (`library.idx`, `covers/`, `playlists/*.m3u`).

## Controles
| | |
|---|---|
| ↑/↓ ou Circle Pad | mover seleção | 
| A | tocar selecionado | 
| B | pausar/continuar |
| ←/→ | -10 s / +10 s |
| L / R | anterior / próxima |
| X / Y | shuffle / repeat |
| SELECT | alternar Músicas ↔ Playlists |
| START | sair |
| Toque | tocar na lista, arrastar para rolar, botões embaixo, 🌙 desliga as telas (qualquer toque acorda) |

## Testes no PC
```
pip install lameenc mutagen pillow
make test        # gera MP3s sintéticos com tags/capas e roda tudo com sanitizers
```

## Estrutura
```
source/main.c            loop principal (redesenha só quando precisa)
source/app.c             estado: fila, playlists, ações
source/audio/            player (ndsp+thread), decoder (minimp3), viz (Goertzel)
source/lib/              id3, mp3info, library (índice), playlist, queue, cover   <- portáteis
source/ui/               ui (citro2d), covers (texturas + thread)
source/third_party/      minimp3, stb_image
tests/                   testes no PC
```

## Bibliotecas de terceiros

- [minimp3](https://github.com/lieff/minimp3), de lieff — CC0.
- [stb_image](https://github.com/nothings/stb), de Sean Barrett — domínio público ou MIT.
