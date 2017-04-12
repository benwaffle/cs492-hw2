CFLAGS += -D_POSIX_C_SOURCE=200809L -std=c99 -g -Wall -Wextra
LDLIBS += -lm

ifneq ($(shell uname -s), Darwin)
	LDLIBS += -lrt
endif

main: main.c

clean:
	$(RM) main

.PHONY: clean
