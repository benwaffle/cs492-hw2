CFLAGS += -pthread -std=c99 -g -Wall -Wextra
LDLIBS += -lm

main: main.c

clean:
	$(RM) main

.PHONY: clean
