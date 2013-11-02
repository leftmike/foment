#
# Foment
#

CCDEBUG = -c -ggdb -Wall -DFOMENT_DEBUG -DFOMENT_UNIX
CCRELEASE = -c -Wall -DFOMENT_UNIX

all: debug release debug/foment release/foment

clean: debug release
	-rm debug/*
	-rm release/*

test: all
	cd ../test ; ../unix/debug/foment runtests.scm r7rs.scm foment.scm eccentric.scm r5rs_pitfall.scm unicode.scm
	cd ../test ; ../unix/release/foment runtests.scm r7rs.scm foment.scm eccentric.scm r5rs_pitfall.scm unicode.scm

stress: all
	cd ../test ; ../unix/debug/foment runtests.scm stress.scm r7rs.scm
	cd ../test ; ../unix/release/foment runtests.scm stress.scm r7rs.scm

debug:
	-mkdir debug

release:
	-mkdir release

debug/base.cpp: debug/txt2cpp ../src/base.scm makefile
	debug/txt2cpp ../src/base.scm debug/base.cpp BaseCode

debug/foment: debug/foment.o debug/gc.o debug/syncthrd.o debug/compile.o debug/io.o\
		debug/synrules.o debug/synpass.o debug/midpass.o debug/genpass.o\
		debug/pairs.o debug/unicode.o debug/chars.o debug/strings.o debug/vectors.o\
		debug/library.o debug/execute.o debug/numbers.o	debug/write.o debug/read.o\
		debug/main.o debug/base.o
	g++ -lpthread -o debug/foment $^

release/foment: release/foment.o release/gc.o release/syncthrd.o release/compile.o release/io.o\
		release/synrules.o release/synpass.o release/midpass.o release/genpass.o\
		release/pairs.o release/unicode.o release/chars.o release/strings.o\
		release/vectors.o release/library.o release/execute.o release/numbers.o\
		release/write.o release/read.o release/main.o release/base.o
	g++ -lpthread -o release/foment $^

debug/foment.o: ../src/foment.cpp ../src/foment.hpp
debug/gc.o: ../src/gc.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp
debug/syncthrd.o: ../src/syncthrd.cpp ../src/foment.hpp ../src/execute.hpp ../src/syncthrd.hpp
debug/compile.o: ../src/compile.cpp ../src/foment.hpp ../src/compile.hpp
debug/synrules.o: ../src/synrules.cpp ../src/foment.hpp ../src/compile.hpp
debug/synpass.o: ../src/synpass.cpp ../src/foment.hpp ../src/compile.hpp
debug/midpass.o: ../src/midpass.cpp ../src/foment.hpp ../src/compile.hpp
debug/genpass.o: ../src/genpass.cpp ../src/foment.hpp ../src/compile.hpp ../src/execute.hpp
debug/pairs.o: ../src/pairs.cpp ../src/foment.hpp
debug/unicode.o: ../src/unicode.cpp ../src/foment.hpp ../src/unicode.hpp ../src/unidata.hpp
debug/chars.o: ../src/chars.cpp ../src/foment.hpp ../src/unicode.hpp
debug/strings.o: ../src/strings.cpp ../src/foment.hpp ../src/unicode.hpp
debug/vectors.o: ../src/vectors.cpp ../src/foment.hpp ../src/unicode.hpp
debug/library.o: ../src/library.cpp ../src/foment.hpp ../src/compile.hpp
debug/execute.o: ../src/execute.cpp ../src/foment.hpp ../src/execute.hpp ../src/syncthrd.hpp
debug/numbers.o: ../src/numbers.cpp ../src/foment.hpp
debug/io.o: ../src/io.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
debug/write.o: ../src/write.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/compile.hpp
debug/read.o: ../src/read.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
debug/main.o: ../src/main.cpp ../src/foment.hpp
debug/base.o: debug/base.cpp

release/foment.o: ../src/foment.cpp ../src/foment.hpp
release/gc.o: ../src/gc.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp
release/syncthrd.o: ../src/syncthrd.cpp ../src/foment.hpp ../src/execute.hpp ../src/syncthrd.hpp
release/compile.o: ../src/compile.cpp ../src/foment.hpp ../src/compile.hpp
release/synrules.o: ../src/synrules.cpp ../src/foment.hpp ../src/compile.hpp
release/synpass.o: ../src/synpass.cpp ../src/foment.hpp ../src/compile.hpp
release/midpass.o: ../src/midpass.cpp ../src/foment.hpp ../src/compile.hpp
release/genpass.o: ../src/genpass.cpp ../src/foment.hpp ../src/compile.hpp ../src/execute.hpp
release/pairs.o: ../src/pairs.cpp ../src/foment.hpp
release/unicode.o: ../src/unicode.cpp ../src/foment.hpp ../src/unicode.hpp ../src/unidata.hpp
release/chars.o: ../src/chars.cpp ../src/foment.hpp ../src/unicode.hpp
release/strings.o: ../src/strings.cpp ../src/foment.hpp ../src/unicode.hpp
release/vectors.o: ../src/vectors.cpp ../src/foment.hpp ../src/unicode.hpp
release/library.o: ../src/library.cpp ../src/foment.hpp ../src/compile.hpp
release/execute.o: ../src/execute.cpp ../src/foment.hpp ../src/execute.hpp ../src/syncthrd.hpp
release/numbers.o: ../src/numbers.cpp ../src/foment.hpp
release/io.o: ../src/io.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
release/write.o: ../src/write.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/compile.hpp
release/read.o: ../src/read.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
release/main.o: ../src/main.cpp ../src/foment.hpp
release/base.o: debug/base.cpp

debug/%.o: %.cpp
	cc $(CCDEBUG) -I ../src -o $@ $<

debug/%.o: ../src/%.cpp
	cc $(CCDEBUG) -I ../src -o $@ $<

debug/%.o: ../src/%.c
	cc $(CCDEBUG) -I ../src -o $@ $<

debug/%.o: debug/%.cpp
	cc $(CCDEBUG) -I ../src -o $@ $<

release/%.o: %.cpp
	cc $(CCRELEASE) -I ../src -o $@ $<

release/%.o: ../src/%.cpp
	cc $(CCRELEASE) -I ../src -o $@ $<

release/%.o: ../src/%.c
	cc $(CCRELEASE) -I ../src -o $@ $<

release/%.o: debug/%.cpp
	cc $(CCRELEASE) -I ../src -o $@ $<

debug/txt2cpp: ../src/txt2cpp.cpp
	g++ $(CCDEBUG) ../src/txt2cpp.cpp -o debug/txt2cpp.o
	g++ debug/txt2cpp.o -o debug/txt2cpp
