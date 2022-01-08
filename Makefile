
CFLAGS_CURL = $(shell curl-config --cflags)
CFLAGS_ARCHIVE = $(shell pkg-config --cflags libarchive)

LDFLAGS_CURL = $(shell curl-config --libs)
LDFLAGS_ARCHIVE= $(shell pkg-config --libs libarchive)

download: download.c
	gcc -O0 -Wall $(CFLAGS_CURL) $(CFLAGS_ARCHIVE) download.c $(LDFLAGS_CURL) $(LDFLAGS_ARCHIVE) -o download -ggdb
