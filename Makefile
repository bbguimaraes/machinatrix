CPPFLAGS += -I. -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -g -Wall -pedantic
LDLIBS += -lcurl -ltidy -lcjson
OUTPUT_OPTION += -MMD -MP
TESTS += tests/html tests/utils

headers = \
	config.h dlpo.h html.h utils.h wikt.h tests/common.h
sources = \
	dlpo.c html.c main.c matrix.c utils.c wikt.c \
	tests/html.c tests/utils.c

.PHONY: all check clean docs format tidy
all: machinatrix machinatrix_matrix
machinatrix: dlpo.o html.o main.o utils.o wikt.o
machinatrix_matrix: matrix.o utils.o
machinatrix machinatrix_matrix:
	$(LINK.c) $^ $(LDLIBS) -o $@

tests/html: html.o utils.o tests/html.o
tests/utils: utils.o tests/utils.o

docs:
	doxygen
format:
	for x in $(headers) $(sources); do clang-format -i "$$x"; done
tidy:
	echo $(sources) \
		| xargs -n 1 \
		| xargs -I {} --max-procs $$(nproc) \
			clang-tidy {} -- $(CPPFLAGS) $(CFLAGS)
test_flags = -fsanitize=address,undefined -fstack-protector
$(TESTS): CFLAGS := $(CFLAGS) $(test_flags)
$(TESTS): LDFLAGS := $(LDFLAGS) $(test_flags)
check: $(TESTS)
	for x in $(TESTS); do { echo "$$x" && ./"$$x"; } || exit; done
clean:
	rm -f \
		machinatrix machinatrix_matrix *.d *.o \
		$(TESTS) tests/*.d tests/*.o
	rm -rf docs/html docs/latex
-include $(wildcard *.d)
-include $(wildcard tests/*.d)
