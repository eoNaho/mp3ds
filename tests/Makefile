# Testes no PC (gcc nativo). Requer: python3 + pip install lameenc mutagen pillow
SRC := ../source
CFILES := test_host.c $(SRC)/lib/util.c $(SRC)/lib/id3.c $(SRC)/lib/mp3info.c $(SRC)/lib/library.c $(SRC)/lib/queue.c \
          $(SRC)/lib/playlist.c $(SRC)/lib/resume.c $(SRC)/lib/cover.c $(SRC)/audio/decoder.c $(SRC)/audio/viz.c $(SRC)/third_party/third_party_impl.c
INC := -I$(SRC) -I$(SRC)/lib -I$(SRC)/audio -I$(SRC)/third_party

all: run
fixtures/music:
	python3 make_fixtures.py fixtures/music
test_host: $(CFILES) fixtures/music
	gcc -g -O1 -Wall -Wextra -Wno-unused-parameter -fsanitize=address,undefined $(INC) $(CFILES) -o $@ -lm
run: test_host
	./test_host
clean:
	rm -rf test_host fixtures
.PHONY: all run clean
