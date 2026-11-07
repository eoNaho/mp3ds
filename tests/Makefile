# Testes no PC (gcc nativo). Requer: python3 + flac + pip install lameenc mutagen pillow
SRC := ../source
CFILES := test_host.c $(SRC)/lib/util.c $(SRC)/lib/id3.c $(SRC)/lib/mp3info.c $(SRC)/lib/flacinfo.c $(SRC)/lib/library.c $(SRC)/lib/queue.c \
          $(SRC)/lib/playlist.c $(SRC)/lib/resume.c $(SRC)/lib/session.c $(SRC)/lib/save_state.c $(SRC)/lib/texture.c $(SRC)/audio/commands.c $(SRC)/lib/cover.c $(SRC)/audio/flac.c $(SRC)/audio/decoder.c $(SRC)/audio/viz.c $(SRC)/third_party/third_party_impl.c
INC := -I$(SRC) -I$(SRC)/lib -I$(SRC)/audio -I$(SRC)/third_party

all: run
fixtures/music:
	python3 make_fixtures.py fixtures/music
test_host: $(CFILES) fixtures/music
	gcc -g -O1 -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined $(INC) $(CFILES) -o $@ -lm
PERSIST_SRC := test_persistence.c $(SRC)/persistence.c $(SRC)/lib/save_state.c $(SRC)/lib/session.c $(SRC)/lib/resume.c $(SRC)/lib/queue.c $(SRC)/lib/library.c $(SRC)/lib/util.c $(SRC)/lib/id3.c $(SRC)/lib/mp3info.c $(SRC)/lib/flacinfo.c
test_persistence: $(PERSIST_SRC) stubs/3ds.h
	gcc -g -O1 -Wall -Wextra -fsanitize=address,undefined -Istubs $(INC) $(PERSIST_SRC) -Wl,--wrap=save_state_write -pthread -o $@
test_tap: test_tap.c $(SRC)/lib/tap_confirm.h
	gcc -g -O1 -Wall -Wextra -fsanitize=address,undefined $(INC) $< -o $@
run: test_host test_persistence test_tap
	./test_host
	./test_persistence
	./test_tap
clean:
	rm -rf test_host test_persistence test_tap fixtures
.PHONY: all run clean
