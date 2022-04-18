CHECK           ?= none
COMPILER	    ?= clang++
OPTIMIZATION    ?= -O0 -g
STANDARD	    ?= c++20
WARNINGS	    ?= -Wall -Wextra
CFLAGS          := -std=$(STANDARD) $(OPTIMIZATION) $(WARNINGS) -Iinclude
OUTPUT          ?= algiz
LDFLAGS         ?= -pthread
TESTARGS        ?= -i 127.0.0.1 -p 8080 -r ./www

CLOC_OPTIONS    := --exclude-dir=.vscode
SOURCES         := $(shell find src/**/*.cpp src/*.cpp)
OBJECTS         := $(SOURCES:.cpp=.o)

ifeq ($(CHECK), asan)
	COMPILER := $(COMPILER) -fsanitize=address -fno-common
else ifeq ($(CHECK), msan)
	COMPILER := $(COMPILER) -fsanitize=memory  -fno-common
endif

.PHONY: all clean test count countbf

all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(COMPILER) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(COMPILER) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OUTPUT) src/*.o src/**/*.o PVS-Studio.log report.tasks strace_out

test: $(OUTPUT)
	./$< $(TESTARGS)

grind: $(OUTPUT)
	valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --show-reachable=no ./$< $(TESTARGS)

count:
	cloc . parser $(CLOC_OPTIONS)

countbf:
	cloc --by-file . parser $(CLOC_OPTIONS)

DEPFILE  = .dep
DEPTOKEN = "\# MAKEDEPENDS"
DEPFLAGS = -f $(DEPFILE) -s $(DEPTOKEN)

depend:
	@ echo $(DEPTOKEN) > $(DEPFILE)
	makedepend $(DEPFLAGS) -- $(COMPILER) $(CFLAGS) -- $(SOURCES) 2>/dev/null
	@ rm $(DEPFILE).bak

sinclude $(DEPFILE)
