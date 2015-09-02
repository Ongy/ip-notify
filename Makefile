CFLAGS+=-Wall -Wextra -pedantic -fno-strict-aliasing -g -Iinclude
TARGET=ip-notify


OBJECTS=src/ip-notify.o

all: $(TARGET) $(TEST)

clean:
	-rm -f $(TARGET)
	-rm -f $(OBJECTS)


fresh: clean all

debug: CFLAGS += -ggdb -DDEBUG
debug: $(TARGET)

.PHONY: fresh debug all clean

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)


