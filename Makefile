CFLAGS += -std=c99 -Wall -pedantic -Wextra -Wshadow -Wconversion -O

all: fancontroller_linux ec_debug

ec_debug: ec_debug.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<


fancontroller_linux: fancontroller_linux.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $<

clean:
	$(RM) fancontroller_linux
