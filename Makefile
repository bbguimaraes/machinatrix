CFLAGS += -g -Wall -pedantic
CFLAGS += -isystem deps/include/
LDFLAGS += -L deps/lib/
LDLIBS += -lcurl -ltidy
OUTPUT_OPTION += -MMD -MP
machinatrix: config.o dlpo.o html.o main.o util.o
	$(LINK.c) $^ $(LDLIBS) -o $@
clean:
	rm -f machinatrix *.d *.o
-include $(wildcard *.d)
