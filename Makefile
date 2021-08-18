CPPFLAGS += -I. -D_POSIX_C_SOURCE=200809L
CFLAGS += -std=c11 -O2 -Wall -Wextra -Wpedantic -Wconversion
LDLIBS += -lcurl -ltidy -lcjson
OUTPUT_OPTION = -MMD -MP -o $@
TESTS += tests/hash tests/html tests/utils

headers = \
	config.h dlpo.h html.h utils.h wikt.h tests/common.h
sources = \
	dlpo.c html.c main.c matrix.c utils.c wikt.c \
	tests/html.c tests/utils.c

.PHONY: all check clean docs tidy
all: machinatrix machinatrix_matrix numeraria
machinatrix: \
	dlpo.o \
	hash.o \
	html.o \
	log.o \
	main.o \
	numeraria_lib.o \
	socket.o \
	utils.o \
	wikt.o
machinatrix_matrix: log.o matrix.o utils.o
numeraria: log.o numeraria.c socket.o utils.o -lsqlite3
machinatrix machinatrix_matrix numeraria:
	$(LINK.c) $^ $(LDLIBS) -o $@

tests/hash: tests/hash.o
tests/html: html.o log.o utils.o tests/common.o tests/html.o
tests/utils: log.o utils.o tests/common.o tests/utils.o

docs:
	doxygen
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
		machinatrix machinatrix_matrix numeraria *.d *.o \
		$(TESTS) tests/*.d tests/*.o
	rm -rf docs/html docs/latex
-include $(wildcard *.d)
-include $(wildcard tests/*.d)
