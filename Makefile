CPPFLAGS += -isystem deps/include/
CFLAGS += -g -Wall -pedantic
LDFLAGS += -L deps/lib/
LDLIBS += -lcurl -ltidy -lcjson
OUTPUT_OPTION += -MMD -MP
.PHONY: all
all: machinatrix machinatrix_matrix
machinatrix: config.o dlpo.o html.o main.o utils.o wikt.o
machinatrix_matrix: config.o matrix.o utils.o
machinatrix machinatrix_matrix:
	$(LINK.c) $^ $(LDLIBS) -o $@
clean:
	rm -f machinatrix machinatrix_matrix *.d *.o
-include $(wildcard *.d)
