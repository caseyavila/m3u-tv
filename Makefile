CC ?= gcc
CFLAGS += -g -Wall -pedantic `pkg-config --cflags mpv gtk+-3.0 epoxy`
LDLIBS += `pkg-config --libs mpv gtk+-3.0 epoxy` -lm

SRC = main.c \
	  m3u-tv-player.c

OBJ = $(SRC:.c=.o)

m3u-tv: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(OBJ) m3u-tv

.PHONY: clean
