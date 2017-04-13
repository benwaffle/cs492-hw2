CFLAGS = -D_POSIX_C_SOURCE=200809L -std=c99 -g -Wall -Wextra
LDLIBS = -lm

ifndef DEBUG
	CFLAGS += -DNDEBUG
endif

ifneq ($(shell uname -s), Darwin)
	LDLIBS += -lrt
endif

main: main.c

clean:
	$(RM) main

check: main
	./check.sh

.PHONY: clean check
