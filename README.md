# mp3ds — Player leve de MP3/FLAC para Nintendo 3DS

Player homebrew focado em MP3 e FLAC, com biblioteca lida do cartão SD, capas com cache, playlists `.m3u`
e visualizador simples.

## Estado do projeto
| Parte | Estado |
|---|---|
| ID3 (v2.2/2.3/2.4 + v1), FLAC (Vorbis Comments, mono/estéreo, 8/16/24-bit, seek), duração, decoders MP3/FLAC, visualizador, fila, playlists, biblioteca/índice, cache de capas, busca com filtro, álbuns, retomada da última faixa | **Testado no PC** (`make test`, com ASan/UBSan) |
| Áudio via `ndsp` (equalizador por IIR do DSP), capas na GPU, UI `citro2d`, scan da biblioteca em thread | **Compilado para 3DS, ainda não testado no hardware** |

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
- MP3/FLAC em `sdmc:/Music` (mude `MUSIC_ROOT` em `source/config.h`).
- Dados do app em `sdmc:/3ds/mp3ds/` (`library.idx`, `covers/`, `playlists/*.m3u`, `resume.txt`).
- Durante a reprodução, fechar a tampa mantém a música tocando; ao pausar ou parar, a suspensão volta a ser permitida.

O estado de retomada (`resume.txt`) guarda a última faixa e posição: ao abrir de novo
e tocar a mesma faixa, ela começa de onde parou. O índice também guarda `added`, `plays`
e `last_played`; índices antigos são migrados automaticamente para o formato v2.

## Controles
| | |
|---|---|
| ↑/↓ ou Circle Pad | mover seleção | 
| A | tocar selecionado |
| B | pausar/continuar (durante o scan: cancela) |
| ←/→ | -10 s / +10 s |
| L / R | anterior / próxima |
| X / Y | shuffle / repeat |
| SELECT | alternar Músicas ↔ Fila ↔ Álbuns ↔ Playlists |
| START | sair |
| Cabeçalho da aba Músicas | lupa: buscar; nome da ordenação: alternar Artista/Álbum/Título/Adição/Recentes |
| Lista de Músicas | mostra o contador de reproduções entre colchetes |
| Fila: A / toque | tocar a partir do slot escolhido |
| Fila: X ou ✕ na borda | remover da fila |
| Fila: ←/→ | mover ±1 posição (a faixa tocando acompanha) |
| 🔍 (cabeçalho) | abrir teclado do 3DS e filtrar a lista por título/artista/álbum; ✕ limpa o filtro (a fila passa a ser o resultado da busca) |
| 🌙 ou qualquer botão/tecla | desliga/religa as telas |
| EQ (rodapé) | abre painel com 6 presets (Desligado / Sub / Graves / Voz / Brilho / Suave); toque um preset (voltinhas são o perfil de frequência) |
| Toque | tocar na lista, arrastar para rolar, botões embaixo |

## Testes no PC
```
pip install lameenc mutagen pillow
# também é necessário o executável flac no PATH
make test        # gera MP3/FLAC sintéticos com tags/capas e roda tudo com sanitizers
```

## Organizar playlists no PC

O diretório `tools/` contém um organizador sem dependências externas:
`tools/playlist_tool.py`. Ele mantém um `playlists.json` editável no PC e exporta
`.m3u8` compatíveis com o 3DS. Os caminhos ficam relativos à pasta de músicas,
portanto o arquivo pode ser movido entre computadores.

```sh
python3 tools/playlist_tool.py --file minhas-playlists.json init "$HOME/Music"
python3 tools/playlist_tool.py --file minhas-playlists.json add \
  "Favoritas" "$HOME/Music/Artista/Album/Musica.flac"
python3 tools/playlist_tool.py --file minhas-playlists.json list
python3 tools/playlist_tool.py --file minhas-playlists.json export '*' \
  /media/3DS/3ds/mp3ds/playlists
```

O comando `export` gera entradas como `sdmc:/Music/Artista/Album/Musica.mp3`.
Copie os `.m3u8` gerados para `sdmc:/3ds/mp3ds/playlists/` e reescaneie as
playlists no app. Mais exemplos estão em [`tools/README.md`](tools/README.md).

## Estrutura
```
source/main.c            loop principal (redesenha só quando precisa)
source/app.c             estado: fila, playlists, álbuns, filtro, retomada, scan em thread
source/audio/            player (ndsp+thread, EQ IIR), decoders MP3/FLAC, viz (Goertzel)
source/lib/              id3, mp3info, flacinfo, library (índice, busca, álbuns), playlist, queue, cover, resume   <- portáteis
source/ui/               ui (citro2d, teclado de busca), covers (texturas + thread)
source/third_party/      minimp3, stb_image
tools/                   organizador de playlists no PC (JSON → M3U8)
tests/                   testes no PC
```

## Bibliotecas de terceiros

- [minimp3](https://github.com/lieff/minimp3), de lieff — CC0.
- [stb_image](https://github.com/nothings/stb), de Sean Barrett — domínio público ou MIT.
