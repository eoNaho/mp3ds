# Playlists no PC

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
