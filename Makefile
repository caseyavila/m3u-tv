CC ?= gcc
CFLAGS += -Wall -pedantic `pkg-config --cflags mpv gtk+-3.0 epoxy`
LDLIBS += `pkg-config --libs mpv gtk+-3.0 epoxy`

SRC = main.c

OBJ = $(SRC:.c=.o)

m3u-tv: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f $(OBJ) m3u-tv

.PHONY: clean
