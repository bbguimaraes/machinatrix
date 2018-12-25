CFLAGS += -g -Wall -pedantic
CFLAGS += -isystem deps/include/
LDFLAGS += -L deps/lib/
LDLIBS += -lcurl -ltidy -lcjson
OUTPUT_OPTION += -MMD -MP
.PHONY: all
all: machinatrix machinatrix_matrix
machinatrix: config.o dlpo.o html.o main.o util.o wikt.o
machinatrix_matrix: config.o matrix.o util.o
machinatrix machinatrix_matrix:
	$(LINK.c) $^ $(LDLIBS) -o $@
clean:
	rm -f machinatrix machinatrix_matrix *.d *.o
-include $(wildcard *.d)
