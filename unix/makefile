#
# Foment
#

CXX ?= g++
BUILD_CXX ?= g++

TEST_OPTIONS = --check-heap
TEST_BUILD = debug

# FOMENT_LITTLE_ENDIAN is defined in foment.hpp as 1,
# EXCEPT if already defined. So if FOMENT_BIG_ENDIAN was
# set when calling make, we define FOMENT_LITTLE_ENDIAN=0
ifdef FOMENT_BIG_ENDIAN
	CFLAGS += -DFOMENT_LITTLE_ENDIAN=0
endif

CFLAGS += -c -Wall -DFOMENT_UNIX -std=c++2a

CCDEBUG   := -ggdb -DFOMENT_DEBUG $(CFLAGS)
CCRELEASE := $(CFLAGS)
CCPROFILE := -pg $(CCRELEASE)

.PHONY: all
all: debug release debug/foment release/foment

.PHONY: prof
prof: profile profile/foment

.PHONY: install
install: all
	sudo cp debug/foment /usr/local/bin

.PHONY: clean
clean: debug release profile
	-rm debug/*
	-rm release/*
	-rm profile/*

.PHONY: test
test: all foment-test stress-test chibi-test process-test

.PHONY: test-all
test-all: all
	$(MAKE) test TEST_BUILD=debug TEST_OPTIONS=--no-collector
	$(MAKE) test TEST_BUILD=release TEST_OPTIONS=--no-collector
	$(MAKE) test TEST_BUILD=debug TEST_OPTIONS=--mark-sweep
	$(MAKE) test TEST_BUILD=release TEST_OPTIONS=--mark-sweep
	$(MAKE) test TEST_BUILD=debug "TEST_OPTIONS=--no-collector --check-heap"
	$(MAKE) test TEST_BUILD=release "TEST_OPTIONS=--no-collector --check-heap"
	$(MAKE) test TEST_BUILD=debug "TEST_OPTIONS=--mark-sweep --check-heap"
	$(MAKE) test TEST_BUILD=release "TEST_OPTIONS=--mark-sweep --check-heap"

.PHONY: foment-test
foment-test: all
	cd ../test ; ../unix/$(TEST_BUILD)/foment $(TEST_OPTIONS) runtests.scm r7rs.scm r7rs-eval.scm foment.scm eccentric.scm r5rs_pitfall.scm unicode.scm srfi.scm
	cd ../test ; ../unix/$(TEST_BUILD)/foment $(TEST_OPTIONS) r7rs.scm

.PHONY: stress-test
stress-test: all
	cd ../test ; ../unix/$(TEST_BUILD)/foment $(TEST_OPTIONS) runtests.scm stress.scm r7rs.scm

.PHONY: chibi-test
chibi-test: all
	cd ../test ; ../unix/$(TEST_BUILD)/foment $(TEST_OPTIONS) r7rs-tests.scm

.PHONY: process-test
process-test: all debug/stdread debug/stdwrite debug/exitcode debug/hang
	export PATH=../unix/debug:"$$PATH" ; cd ../test ; ../unix/$(TEST_BUILD)/foment $(TEST_OPTIONS) runtests.scm process.scm

debug:
	-mkdir debug

release:
	-mkdir release

profile:
	-mkdir profile

debug/base.cpp: debug/txt2cpp makefile ../src/base.scm ../src/srfi-106.scm ../src/srfi-60.scm\
		../src/srfi-1.scm ../src/srfi-128.scm ../src/srfi-125.scm ../src/srfi-133.scm\
		../src/srfi-14.scm
	debug/txt2cpp debug/base.cpp ../src/base.scm ../src/srfi-106.scm ../src/srfi-60.scm\
		../src/srfi-1.scm ../src/srfi-128.scm ../src/srfi-125.scm ../src/srfi-133.scm\
		../src/srfi-14.scm

debug/foment: debug/foment.o debug/gc.o debug/syncthrd.o debug/compile.o debug/io.o\
		debug/synrules.o debug/synpass.o debug/midpass.o debug/genpass.o\
		debug/pairs.o debug/unicode.o debug/chars.o debug/strings.o debug/vectors.o\
		debug/library.o debug/execute.o debug/numbers.o debug/write.o\
		debug/read.o debug/filesys.o debug/compare.o debug/main.o debug/hashtbl.o\
		debug/bignums.o debug/charset.o debug/process.o debug/base.o
	$(CXX)  $(LDFLAGS) -o debug/foment $^ -lpthread

release/foment: release/foment.o release/gc.o release/syncthrd.o release/compile.o release/io.o\
		release/synrules.o release/synpass.o release/midpass.o release/genpass.o\
		release/pairs.o release/unicode.o release/chars.o release/strings.o\
		release/vectors.o release/library.o release/execute.o release/numbers.o\
		release/write.o release/read.o release/filesys.o\
		release/compare.o release/main.o release/hashtbl.o release/bignums.o release/charset.o\
		release/process.o release/base.o
	$(CXX) $(LDFLAGS) -o release/foment $^ -lpthread

profile/foment: profile/foment.o profile/gc.o profile/syncthrd.o profile/compile.o profile/io.o\
		profile/synrules.o profile/synpass.o profile/midpass.o profile/genpass.o\
		profile/pairs.o profile/unicode.o profile/chars.o profile/strings.o\
		profile/vectors.o profile/library.o profile/execute.o profile/numbers.o\
		profile/write.o profile/read.o profile/filesys.o\
		profile/compare.o profile/main.o profile/hashtbl.o profile/bignums.o profile/charset.o\
		profile/process.o profile/base.o
	$(CXX)  $(LDFLAGS) -pg -o profile/foment $^ -lpthread

debug/foment.o: ../src/foment.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/unicode.hpp
debug/gc.o: ../src/gc.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp
debug/syncthrd.o: ../src/syncthrd.cpp ../src/foment.hpp ../src/execute.hpp ../src/syncthrd.hpp
debug/compile.o: ../src/compile.cpp ../src/foment.hpp ../src/compile.hpp
debug/synrules.o: ../src/synrules.cpp ../src/foment.hpp ../src/compile.hpp
debug/synpass.o: ../src/synpass.cpp ../src/foment.hpp ../src/compile.hpp
debug/midpass.o: ../src/midpass.cpp ../src/foment.hpp ../src/compile.hpp
debug/genpass.o: ../src/genpass.cpp ../src/foment.hpp ../src/compile.hpp ../src/execute.hpp
debug/pairs.o: ../src/pairs.cpp ../src/foment.hpp
debug/unicode.o: ../src/unicode.cpp ../src/foment.hpp ../src/unicode.hpp ../src/unicase.hpp
debug/chars.o: ../src/chars.cpp ../src/foment.hpp ../src/unicode.hpp
debug/strings.o: ../src/strings.cpp ../src/foment.hpp ../src/unicode.hpp
debug/vectors.o: ../src/vectors.cpp ../src/foment.hpp ../src/unicode.hpp
debug/library.o: ../src/library.cpp ../src/foment.hpp ../src/compile.hpp
debug/execute.o: ../src/execute.cpp ../src/foment.hpp ../src/execute.hpp ../src/syncthrd.hpp
debug/numbers.o: ../src/numbers.cpp ../src/foment.hpp ../src/unicode.hpp ../src/bignums.hpp
debug/bignums.o: ../src/bignums.cpp ../src/foment.hpp ../src/unicode.hpp ../src/bignums.hpp
debug/charset.o: ../src/charset.cpp ../src/foment.hpp ../src/unicode.hpp ../src/unicrng.hpp
debug/process.o: ../src/process.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
debug/io.o: ../src/io.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
debug/write.o: ../src/write.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/compile.hpp
debug/read.o: ../src/read.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
debug/filesys.o: ../src/filesys.cpp ../src/foment.hpp ../src/unicode.hpp
debug/hashtbl.o: ../src/hashtbl.cpp ../src/foment.hpp ../src/syncthrd.hpp
debug/compare.o: ../src/compare.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/unicode.hpp
debug/main.o: ../src/main.cpp ../src/foment.hpp
debug/base.o: debug/base.cpp

release/foment.o: ../src/foment.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/unicode.hpp
release/gc.o: ../src/gc.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp
release/syncthrd.o: ../src/syncthrd.cpp ../src/foment.hpp ../src/execute.hpp ../src/syncthrd.hpp
release/compile.o: ../src/compile.cpp ../src/foment.hpp ../src/compile.hpp
release/synrules.o: ../src/synrules.cpp ../src/foment.hpp ../src/compile.hpp
release/synpass.o: ../src/synpass.cpp ../src/foment.hpp ../src/compile.hpp
release/midpass.o: ../src/midpass.cpp ../src/foment.hpp ../src/compile.hpp
release/genpass.o: ../src/genpass.cpp ../src/foment.hpp ../src/compile.hpp ../src/execute.hpp
release/pairs.o: ../src/pairs.cpp ../src/foment.hpp
release/unicode.o: ../src/unicode.cpp ../src/foment.hpp ../src/unicode.hpp ../src/unicase.hpp
release/chars.o: ../src/chars.cpp ../src/foment.hpp ../src/unicode.hpp
release/strings.o: ../src/strings.cpp ../src/foment.hpp ../src/unicode.hpp
release/vectors.o: ../src/vectors.cpp ../src/foment.hpp ../src/unicode.hpp
release/library.o: ../src/library.cpp ../src/foment.hpp ../src/compile.hpp
release/execute.o: ../src/execute.cpp ../src/foment.hpp ../src/execute.hpp ../src/syncthrd.hpp
release/numbers.o: ../src/numbers.cpp ../src/foment.hpp ../src/unicode.hpp ../src/bignums.hpp
release/bignums.o: ../src/bignums.cpp ../src/foment.hpp ../src/unicode.hpp ../src/bignums.hpp
release/charset.o: ../src/charset.cpp ../src/foment.hpp ../src/unicode.hpp ../src/unicrng.hpp
release/process.o: ../src/process.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
release/io.o: ../src/io.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
release/write.o: ../src/write.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/compile.hpp
release/read.o: ../src/read.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
release/filesys.o: ../src/filesys.cpp ../src/foment.hpp ../src/unicode.hpp
release/hashtbl.o: ../src/hashtbl.cpp ../src/foment.hpp ../src/syncthrd.hpp
release/compare.o: ../src/compare.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/unicode.hpp
release/main.o: ../src/main.cpp ../src/foment.hpp
release/base.o: debug/base.cpp

debug/%.o: %.cpp
	$(CXX) $(CCDEBUG) -I ../src -o $@ $<

debug/%.o: ../src/%.cpp
	$(CXX) $(CCDEBUG) -I ../src -o $@ $<

debug/%.o: debug/%.cpp
	$(CXX) $(CCDEBUG) -I ../src -o $@ $<

release/%.o: %.cpp
	$(CXX) $(CCRELEASE) -I ../src -o $@ $<

release/%.o: ../src/%.cpp
	$(CXX) $(CCRELEASE) -I ../src -o $@ $<

release/%.o: debug/%.cpp
	$(CXX) $(CCRELEASE) -I ../src -o $@ $<

profile/%.o: %.cpp
	$(CXX) $(CCPROFILE) -I ../src -o $@ $<

profile/%.o: ../src/%.cpp
	$(CXX) $(CCPROFILE) -I ../src -o $@ $<

profile/%.o: debug/%.cpp
	$(CXX) $(CCPROFILE) -I ../src -o $@ $<

debug/%: ../test/%.cpp
	$(BUILD_CXX) $(CCDEBUG) -o debug/$*.o $<
	$(BUILD_CXX) debug/$*.o -o $@

debug/txt2cpp: ../src/txt2cpp.cpp
	$(BUILD_CXX) $(CCDEBUG) ../src/txt2cpp.cpp -o debug/txt2cpp.o
	$(BUILD_CXX) debug/txt2cpp.o -o debug/txt2cpp

