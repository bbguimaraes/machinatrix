CPPFLAGS += -I. -Isrc -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -g -Wall -Wextra -Wpedantic -Wconversion
LDLIBS += -lcurl -ltidy -lcjson
OUTPUT_OPTION += -MMD -MP
TESTS += tests/html tests/utils

headers = \
	src/config.h src/dlpo.h src/html.h src/utils.h src/wikt.h tests/common.h
sources = \
	src/dlpo.c src/html.c src/main.c src/matrix.c src/utils.c src/wikt.c \
	tests/html.c tests/utils.c

.PHONY: all check clean docs format tidy
all: machinatrix machinatrix_matrix
machinatrix: src/dlpo.o src/html.o src/main.o src/utils.o src/wikt.o
machinatrix_matrix: src/matrix.o src/utils.o
machinatrix machinatrix_matrix:
	$(LINK.c) $^ $(LDLIBS) -o $@

tests/html: src/html.o src/utils.o tests/common.o tests/html.o
tests/utils: src/utils.o tests/common.o tests/utils.o

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
		machinatrix machinatrix_matrix src/*.d src/*.o \
		$(TESTS) tests/*.d tests/*.o
	rm -rf docs/html docs/latex
-include $(wildcard *.d)
-include $(wildcard tests/*.d)
