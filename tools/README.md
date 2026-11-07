# Ferramentas para a biblioteca no PC

## Gerar capas com SongRec

O `cover_tool.py` reconhece cada MP3 usando o [SongRec](https://github.com/marin-m/SongRec),
baixa a capa retornada pelo Shazam e incorpora a imagem na tag ID3 `APIC`. O
`mp3ds` já lê essa tag, então a capa aparece no 3DS sem criar uma imagem única
para toda a categoria.

Instale o SongRec conforme a documentação do projeto e confirme que o comando
`songrec` está no `PATH`. Instale também o Mutagen:

```sh
python3 -m pip install mutagen
```

Primeiro faça uma simulação limitada:

```sh
python3 tools/cover_tool.py music/Biblioteca --dry-run --limit 5
```

O intervalo padrão entre consultas é de dois segundos. Para um teste rápido
local, ele pode ser reduzido com `--delay 0`; para uma coleção grande, é melhor
mantê-lo ou aumentá-lo para evitar limite temporário do serviço.

Cada reconhecimento também possui timeout. O padrão é de 30 segundos; uma
faixa que travar será registrada como falha e o lote continuará. Para uma
execução mais rápida, pode-se usar `--timeout 15 --request-interval 1`.

Depois aplique nas músicas:

```sh
python3 tools/cover_tool.py music/Biblioteca
```

Por padrão, arquivos que já têm uma capa incorporada são preservados. Para
substituí-las:

```sh
python3 tools/cover_tool.py music/Biblioteca --force
```

As imagens baixadas ficam em `music/Biblioteca/.cover-cache/` para evitar novos
downloads quando a ferramenta for executada novamente. Essa pasta pode ser
apagada depois do processamento. Músicas não reconhecidas aparecem no relatório
e não são alteradas.

O SongRec envia somente a impressão digital acústica ao serviço de
reconhecimento; o áudio original não é enviado, conforme a documentação do
projeto. O reconhecimento depende de rede e pode sofrer limitação de requisições.

## Playlists no PC

O `playlist_tool.py` usa um JSON simples como arquivo de trabalho no PC e exporta
`.m3u8`, o formato que o mp3ds já lê no 3DS. O JSON guarda caminhos relativos à
pasta de músicas, então não fica amarrado ao caminho absoluto do seu computador.

## Exemplo

```sh
python3 tools/playlist_tool.py --file minhas-playlists.json \
  init "$HOME/Music"

python3 tools/playlist_tool.py --file minhas-playlists.json \
  add "Favoritas" "$HOME/Music/Artista/Album/Musica.mp3"

python3 tools/playlist_tool.py --file minhas-playlists.json list

# exporta sdmc:/Music/Artista/... para cada playlist
python3 tools/playlist_tool.py --file minhas-playlists.json \
  export '*' /media/3DS/3ds/mp3ds/playlists
```

O resultado é uma playlist UTF-8 com `sdmc:/Music/...`, pronta para copiar para:

```text
sdmc:/3ds/mp3ds/playlists/
```

O `mp3ds` também encontra playlists diretamente em `sdmc:/Music/` ou em
`sdmc:/Music/Playlists/`, caso você queira manter as listas junto da biblioteca.

Para testar no PC usando caminhos relativos, use `--style relative`. O 3DS também
aceita playlists relativas quando elas estão no mesmo diretório das músicas, mas
para o layout padrão `sdmc:/Music` o estilo `sdmc` é o recomendado.

O JSON pode ser editado manualmente:

```json
{
  "version": 1,
  "music_root": "/home/voce/Music",
  "sd_root": "sdmc:/Music",
  "playlists": {
    "Favoritas": [
      "Artista/Album/Musica.mp3"
    ],
    "Treino": []
  }
}
```
