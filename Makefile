CHECK           ?= none
COMPILER        ?= clang++
OPTIMIZATION    ?= -O0 -g
STANDARD        ?= c++20
WARNINGS        ?= -Wall -Wextra
INCLUDES        := -Iinclude -Ijson/include -Iinja/single_include/inja $(shell pkg-config --cflags openssl)
CFLAGS          := -std=$(STANDARD) $(OPTIMIZATION) $(WARNINGS) $(INCLUDES)
OUTPUT          ?= algiz
LDFLAGS         ?= -pthread $(shell pkg-config --libs openssl) -ldl

CLOC_OPTIONS    := --exclude-dir=.vscode,json,www,inja --not-match-f='^algiz.json$$'
SOURCES         := $(shell find -L src -name '*.cpp' | sed -nE '/^src\/plugins\//!p')
OBJECTS         := $(SOURCES:.cpp=.o)

SHARED_EXT      := so
SHARED_FLAG	    := -shared
LIBPATHVAR      := LD_LIBRARY_PATH
ifeq ($(shell uname -s), Darwin)
	SHARED_EXT  := dylib
	SHARED_FLAG := -dynamiclib
	LIBPATHVAR  := DYLD_LIBRARY_PATH
endif

SOURCES_PL      := $(shell find -L src/plugins -name '*.cpp')
OBJECTS_PL      := $(addsuffix .$(SHARED_EXT),$(addprefix plugin/,$(shell ls src/plugins)))

ifeq ($(CHECK), asan)
	COMPILER := $(COMPILER) -fsanitize=address -fno-common
else ifeq ($(CHECK), msan)
	COMPILER := $(COMPILER) -fsanitize=memory  -fno-common
endif

.PHONY: all plugins clean test count countbf list unplug

all: $(OUTPUT) plugins

sinclude $(shell find src -name 'targets.mk')

define PLUGINRULE
plugin/$P.$(SHARED_EXT): $(patsubst %.cpp,%.o,$(addprefix src/plugins/$P/,$(filter %.cpp,$(shell ls "src/plugins/$P"))))
	$(COMPILER) $(SHARED_FLAG) $$+ -o $$@ $(LDFLAGS) -Wl,-undefined,dynamic_lookup
endef

$(foreach P,$(notdir $(shell find src/plugins -mindepth 1 -maxdepth 1 -type d '!' -exec test -e "{}/targets.mk" ';' -print)),$(eval $(PLUGINRULE)))

plugin:
	@ mkdir -p plugin

plugins: plugin $(OBJECTS_PL)

$(OUTPUT): $(OBJECTS)
	$(COMPILER) -rdynamic -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(COMPILER) $(CFLAGS) -c $< -o $@

src/plugins/%.o: src/plugins/%.cpp
	$(COMPILER) $(CFLAGS) -fPIC -c $< -o $@

clean:
	rm -f $(strip $(OUTPUT) $(shell find src -name '*.o') PVS-Studio.log report.tasks strace_out $(shell find plugin -name '*.$(SHARED_EXT)'))

unplug:
	rm -f $(shell find plugin -name '*.$(SHARED_EXT)')

test: $(OUTPUT) plugins
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
	makedepend $(DEPFLAGS) -- $(COMPILER) $(CFLAGS) -- $(SOURCES) $(SOURCES_PL) 2>/dev/null
	@ rm $(DEPFILE).bak

sinclude $(DEPFILE)
