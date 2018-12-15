CFLAGS += -g -Wall -pedantic
OUTPUT_OPTION += -MMD -MP
machinatrix: config.o main.o util.o
	$(LINK.c) $^ $(LDLIBS) -o $@
clean:
	rm -f machinatrix *.d *.o
-include $(wildcard *.d)
