CC=gcc
CFILES=htmlify.c
OFILES=htmlify.o
LIBS=-lmisclib
WFLAGS=-Wall -Werror
GFLAGS=-g
ALLFLAGS=$(GFLAGS) $(WFLAGS)

SOURCE_FILES := \
	buffer.c \
	htmlify.c

OBJECT_FILES := ${SOURCE_FILES:.c=.o}

htmlify: $(OBJECT_FILES)
	$(CC) $(ALLFLAGS) -o htmlify $(OBJECT_FILES) $(LIBS)

$(OBJECT_FILES): $(SOURCE_FILES)
	$(CC) $(ALLFLAGS) -c $^

clean:
	rm *.o
