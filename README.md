# mp3ds — Player leve de MP3 para Nintendo 3DS

Player homebrew focado em MP3, com biblioteca lida do cartão SD, capas com cache, playlists `.m3u`
e visualizador simples.

## Estado do projeto
| Parte | Estado |
|---|---|
| ID3 (v2.2/2.3/2.4 + v1), duração, decoder MP3 (seek Xing TOC), visualizador, fila, playlists, biblioteca/índice, cache de capas, busca com filtro, álbuns, retomada da última faixa | **Foco atual: MP3 e interface fluida** |
| Áudio via `ndsp` (equalizador por IIR do DSP), capas na GPU, UI `citro2d`, scan da biblioteca em thread | **Compilado para 3DS, ainda não testado no hardware** |

O foco atual é reprodução MP3 e uma interface fluida no 3DS. O decoder FLAC nativo
continua preservado no código, mas fica desativado por padrão (`ENABLE_FLAC 0` em
`source/config.h`) até receber uma otimização específica para o Old 3DS.

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
- Dados do app em `sdmc:/3ds/mp3ds/` (`library.idx`, `covers/`, `playlists/*.m3u`, `resume.txt`). Playlists também são encontradas em `sdmc:/Music/*.m3u` e `sdmc:/Music/Playlists/*.m3u` ou `.m3u8`.
- Depois do primeiro scan, o índice salvo é reutilizado nas próximas aberturas, sem analisar todas as músicas novamente. Se os arquivos forem movidos, o app faz uma sincronização automática uma vez para atualizar os caminhos; para procurar músicas novas ou removidas, use `Playlists → Reescanear biblioteca`.
- Durante a reprodução, fechar a tampa mantém a música tocando; ao pausar ou parar, a suspensão volta a ser permitida.

A fila é salva em `session.txt`, incluindo a ordem de reprodução, a posição atual,
shuffle e repeat. Ao reabrir, pressione B ou play para continuar; a reprodução
começa quando você pedir. Faixas removidas do SD são ignoradas na restauração.

Durante o uso, sessão, posição e histórico são gravados por um worker, sem
escritas no SD na thread da interface. Ao sair, o app aguarda o último salvamento.
As capas são convertidas diretamente para a textura, e a troca ocorre depois do
frame anterior terminar, evitando esperas extras de transferência da GPU.

O índice v3 aceita caminhos de até 1023 bytes, títulos de até 383 bytes e
artista/álbum/album artist de até 255 bytes, sempre sem cortar caracteres UTF-8.
O scan informa caminhos ignorados por tamanho ou profundidade. Coletâneas usam
album artist (TPE2), discos usam TPOS, e a capa frontal APIC tipo 3 tem preferência.
A busca mantém CJK e letras como č, ř e ł, com campos normalizados em memória;
caminhos são consultados por hash para restaurar filas grandes.

Capas novas usam RGB565 no cache (32 KiB mais cabeçalho); caches RGBA8 antigos
continuam legíveis. O carregamento tenta as faixas do álbum antes de criar um
marcador sem capa. “Reescanear” limpa esses marcadores e o cache em memória.

A thread de áudio pré-abre a próxima faixa usando offset/TOC do índice, com
validação de tamanho/mtime. Para taxas iguais, enfileira a próxima sem drenar o
DSP. Tags LAME válidas removem delay/padding; arquivos sem essas tags conservam
seu silêncio de codificação. Taxas diferentes ainda exigem reconfigurar o DSP.
O teste com DSP simulado verifica continuidade e sincronização da interface;
latência e consumo reais ainda precisam ser medidos em Old/New 3DS.

O auto-save periódico grava somente `resume.txt`; sessão/fila são gravadas quando
mudam, e o histórico ao pausar/sair. Com a tampa fechada ou as telas desligadas,
o desenho é suspenso e o áudio continua.

ReplayGain continua opcional. A troca do JPEG para libjpeg-turbo com escala DCT
fica para uma comparação de tempo/RAM no console antes de adicionar a dependência.

O estado de retomada (`resume.txt`) guarda a última faixa e posição: ao abrir de novo
e tocar a mesma faixa, ela começa de onde parou. O índice também guarda `added`, `plays`
e `last_played`; índices antigos são migrados automaticamente para o formato v3. A migracao rele as tags uma vez e preserva o historico.

## Controles
| | |
|---|---|
| ↑/↓ ou Circle Pad | mover seleção | 
| A | tocar selecionado |
| A + X / A + Y (Músicas) | adicionar a selecionada para tocar a seguir / ao fim da fila |
| + na borda da música | abrir ações da fila; B ou toque fora cancela |
| B | pausar/continuar (durante o scan: cancela) |
| ←/→ | -10 s / +10 s |
| L / R | anterior / próxima faixa |
| A + L / A + R | aba anterior / próxima (Músicas, Fila, Álbuns, Playlists) |
| X / Y | shuffle / repeat |
| SELECT | alternar Músicas ↔ Fila ↔ Álbuns ↔ Playlists |
| START | sair |
| Cabeçalho da aba Músicas | lupa: buscar; nome da ordenação: alternar Artista/Álbum/Título/Adição/Recentes |
| Lista de Músicas | mostra o contador de reproduções entre colchetes |
| Fila: A / toque duplo | tocar a partir do slot escolhido |
| Fila: X ou ✕ na borda | remover da fila |
| Fila: ←/→ | mover ±1 posição (a faixa tocando acompanha) |
| 🔍 (cabeçalho) | abre Busca e filtros: A edita o texto, X alterna Todos/Título/Artista/Álbum/Pasta, Y alterna Todas/Nunca tocadas/Já tocadas; B mostra os resultados |
| 🌙 ou qualquer botão/tecla | desliga/religa as telas |
| EQ (rodapé) | abre painel com 6 presets (Desligado / Sub / Graves / Voz / Brilho / Suave); toque um preset (voltinhas são o perfil de frequência) |
| Toque na lista | um toque seleciona; dois toques na mesma linha em até 450 ms abrem/tocam |
| Anterior / próxima na tela | dois toques em até 450 ms; L / R continuam imediatos |
| Arrastar | rolar a lista; a seleção acompanha a janela e o toque duplo pendente é cancelado |

Os controles de toque seguem as áreas dos ícones. Gestos toleram até 10 px de
movimento; um toque usa a última posição válida antes de soltar o dedo. Arrastar
no painel de busca ou no popup da fila não aciona botões nem rola a lista ao fundo.

A busca combina todas as palavras digitadas, em qualquer ordem, ignorando caixa e
acentos: `cronicas entender` encontra uma faixa com “Crônicas” no artista e
“Entender” no título. O filtro de campo restringe todas as palavras ao campo
escolhido; Pasta procura no diretório, sem incluir o nome do arquivo. Os filtros
podem ser usados sem texto, inclusive para listar só músicas nunca tocadas.
O painel mostra a contagem de resultados e permite editar o texto ou limpar todos
os filtros. Tocar um resultado monta a fila na mesma ordem visível da lista.

## Testes no PC
Requer GCC, Python 3 e os executáveis `flac` e `lame` no PATH; devkitARM é necessário somente para compilar o aplicativo.

```
pip install lameenc mutagen pillow
make test        # gera MP3 sintéticos com tags/capas e roda tudo com sanitizers
```

## Organizar playlists no PC

O diretório `tools/` contém um organizador sem dependências externas:
`tools/playlist_tool.py`. Ele mantém um `playlists.json` editável no PC e exporta
`.m3u8` compatíveis com o 3DS. Os caminhos ficam relativos à pasta de músicas,
portanto o arquivo pode ser movido entre computadores.

Para reconhecer músicas e incorporar capas ID3 usando o SongRec, veja
[`tools/README.md`](tools/README.md) e execute `tools/cover_tool.py` no PC. A
ferramenta não substitui capas existentes por padrão e não é executada no 3DS.

```sh
python3 tools/playlist_tool.py --file minhas-playlists.json init "$HOME/Music"
python3 tools/playlist_tool.py --file minhas-playlists.json add \
  "Favoritas" "$HOME/Music/Artista/Album/Musica.mp3"
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
source/audio/            player (ndsp+thread, EQ IIR), decoder MP3, viz (Goertzel)
source/lib/              id3, mp3info, library (índice, busca, álbuns), playlist, queue, cover, resume   <- portáteis
source/ui/               ui (citro2d, teclado de busca), covers (texturas + thread)
source/third_party/      minimp3, stb_image
tools/                   organizador de playlists no PC (JSON → M3U8)
tests/                   testes no PC
```

## Bibliotecas de terceiros

- [minimp3](https://github.com/lieff/minimp3), de lieff — CC0.
- [stb_image](https://github.com/nothings/stb), de Sean Barrett — domínio público ou MIT.
