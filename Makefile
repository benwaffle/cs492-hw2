CFLAGS += -D_DEFAULT_SOURCE -std=c99 -g -Wall -Wextra
LDLIBS += -lm

main: main.c

clean:
	$(RM) main

.PHONY: clean
