CPPFLAGS += -I. -isystem deps/include/
CFLAGS += -g -Wall -pedantic
LDFLAGS += -L deps/lib/
LDLIBS += -lcurl -ltidy -lcjson
OUTPUT_OPTION += -MMD -MP
TESTS += tests/html tests/utils

.PHONY: all
all: machinatrix machinatrix_matrix
machinatrix: config.o dlpo.o html.o main.o utils.o wikt.o
machinatrix_matrix: config.o matrix.o utils.o
machinatrix machinatrix_matrix:
	$(LINK.c) $^ $(LDLIBS) -o $@

tests/html: html.o utils.o tests/html.o
tests/utils: utils.o tests/utils.o

check: $(TESTS)
	for x in $(TESTS); do { echo "$$x" && ./"$$x"; } || exit; done
clean:
	rm -f \
		machinatrix machinatrix_matrix *.d *.o \
		$(TESTS) tests/*.d tests/*.o
-include $(wildcard *.d)
-include $(wildcard tests/*.d)
