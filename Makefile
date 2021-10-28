BINARY=dc

SRC=src
BIN=bin
INCLUDE=$(SRC)/include

VERSIONS ?= original vuln-1 vuln-2 vuln-3 vuln-4 vuln-5

VBINARIES=$(VERSIONS:%=%/$(BINARY))
BIN_TARGETS=$(VBINARIES) $(VBINARIES:%=%-san) $(VBINARIES:%=%-fuzz) $(VBINARIES:%=%-cov)

BIN_DIRS=$(BIN) $(VERSIONS:%=$(BIN)/%)

TARGETS=$(BIN_TARGETS:%=$(BIN)/%)

HEADERS=$(wildcard $(INCLUDE)/*.h)

# allow the user to provide additional CFLAGS by doing e.g. CFLAGS=blah make
CFLAGS += -W -Wall -Wpedantic -Wno-language-extension-token -g -I$(INCLUDE)

# allow the user to override what clang we use by doing e.g. CLANG=blah make
CLANG ?= clang-6.0
CC=$(CLANG)

SAN_FLAGS ?= -fsanitize=address -fno-omit-frame-pointer -DDC_FUZZ
FUZZ_FLAGS ?= -DDC_LIBFUZZER -fsanitize=fuzzer,address -fno-omit-frame-pointer -DDC_FUZZ
NO_STRICT_OVERFLOW_CFLAGS ?= -fwrapv -fno-strict-overflow -Wstrict-overflow
COV_FLAGS ?= -fprofile-instr-generate -fcoverage-mapping -DDC_FUZZ

default: $(BIN_DIRS) $(TARGETS)

$(BIN_DIRS):
	mkdir -p $@

.PHONY: default


$(BIN)/%: $(SRC)/%.c $(HEADERS)
	$(CC) $< $(CFLAGS) $(LDFLAGS) $(NO_STRICT_OVERFLOW_CFLAGS) -o $@ 


$(BIN)/%-san: $(SRC)/%.c $(HEADERS)
	$(CLANG) $< $(CFLAGS) $(LDFLAGS) $(SAN_FLAGS) $(NO_STRICT_OVERFLOW_CFLAGS) -o $@


# build a self-fuzzing binary with libFuzzer
# needs a recent clang version (e.g. clang-6.0)
# then to run it:
# ./dc-fuzz ~/tmp/corpus/  -timeout=5 -only_ascii=1 -dict=libfuzzer-dict -max_total_time=1200 -print_final_stats=1
$(BIN)/%-fuzz: $(SRC)/%.c $(HEADERS)
	$(CLANG) $< $(CFLAGS) $(LDFLAGS) $(FUZZ_FLAGS) $(NO_STRICT_OVERFLOW_CFLAGS) -o $@

$(BIN)/%-cov: $(SRC)/%.c $(HEADERS)
	$(CLANG) $< $(CFLAGS) $(LDFLAGS) $(COV_FLAGS) $(NO_STRICT_OVERFLOW_CFLAGS) -o $@

clean:
	rm -rf bin/* *.profraw *.profdata

