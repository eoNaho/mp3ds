# Testes no PC (gcc nativo). Requer: python3 + flac + lame + pip install lameenc mutagen pillow
SRC := ../source
PYTHON ?= python3
CFILES := test_host.c $(SRC)/lib/util.c $(SRC)/lib/id3.c $(SRC)/lib/mp3info.c $(SRC)/lib/flacinfo.c $(SRC)/lib/library.c $(SRC)/lib/queue.c \
          $(SRC)/lib/playlist.c $(SRC)/lib/resume.c $(SRC)/lib/session.c $(SRC)/lib/save_state.c $(SRC)/lib/texture.c $(SRC)/lib/touch.c $(SRC)/audio/commands.c $(SRC)/audio/stream.c $(SRC)/lib/cover.c $(SRC)/audio/flac.c $(SRC)/audio/decoder.c $(SRC)/audio/viz.c $(SRC)/third_party/third_party_impl.c
INC := -I$(SRC) -I$(SRC)/lib -I$(SRC)/audio -I$(SRC)/third_party

all: run
fixtures/.stamp: make_fixtures.py make_regressions.py
	$(PYTHON) make_fixtures.py fixtures/music
	$(PYTHON) make_regressions.py
	touch $@
test_host: $(CFILES) test_regressions.inc $(wildcard $(SRC)/*.h $(SRC)/lib/*.h $(SRC)/audio/*.h) fixtures/.stamp
	gcc -g -O1 -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined $(INC) $(CFILES) -o $@ -lm
PERSIST_SRC := test_persistence.c $(SRC)/persistence.c $(SRC)/lib/save_state.c $(SRC)/lib/session.c $(SRC)/lib/resume.c $(SRC)/lib/queue.c $(SRC)/lib/library.c $(SRC)/lib/util.c $(SRC)/lib/id3.c $(SRC)/lib/mp3info.c $(SRC)/lib/flacinfo.c
test_persistence: $(PERSIST_SRC) stubs/3ds.h $(wildcard $(SRC)/*.h $(SRC)/lib/*.h)
	gcc -g -O1 -Wall -Wextra -fsanitize=address,undefined -Istubs $(INC) $(PERSIST_SRC) -Wl,--wrap=save_state_write -pthread -o $@
test_tap: test_tap.c $(SRC)/lib/tap_confirm.h
	gcc -g -O1 -Wall -Wextra -fsanitize=address,undefined $(INC) $< -o $@
PLAYER_SRC := test_player.c $(SRC)/lib/flacinfo.c $(SRC)/audio/player.c $(SRC)/audio/stream.c $(SRC)/audio/commands.c $(SRC)/audio/decoder.c $(SRC)/audio/flac.c $(SRC)/audio/viz.c $(SRC)/lib/id3.c $(SRC)/lib/mp3info.c $(SRC)/lib/util.c $(SRC)/third_party/third_party_impl.c
test_player: $(PLAYER_SRC) stubs/3ds.h $(wildcard $(SRC)/audio/*.h $(SRC)/lib/*.h) fixtures/.stamp
	gcc -g -O1 -Wall -Wextra -fsanitize=address,undefined -Istubs $(INC) $(PLAYER_SRC) -pthread -lm -o $@
run: test_host test_persistence test_tap test_player
	./test_host
	./test_persistence
	./test_tap
	./test_player
clean:
	rm -rf test_host test_persistence test_tap test_player fixtures
.PHONY: all run clean
