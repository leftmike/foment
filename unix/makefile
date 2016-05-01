#
# Foment
#

CCDEBUG = -c -ggdb -Wall -DFOMENT_DEBUG -DFOMENT_UNIX
CCRELEASE = -c -Wall -DFOMENT_UNIX
CCPROFILE = -pg $(CCRELEASE)

all: debug release debug/foment release/foment

prof: profile profile/foment

install: all
	sudo cp debug/foment /usr/local/bin

clean: debug release profile
	-rm debug/*
	-rm release/*
	-rm profile/*

test: all
	cd ../test ; ../unix/debug/foment runtests.scm r7rs.scm r7rs-eval.scm foment.scm eccentric.scm r5rs_pitfall.scm unicode.scm srfi.scm
	cd ../test ; ../unix/debug/foment r7rs.scm
	cd ../test ; ../unix/release/foment runtests.scm r7rs.scm r7rs-eval.scm foment.scm eccentric.scm r5rs_pitfall.scm unicode.scm srfi.scm
	cd ../test ; ../unix/release/foment r7rs.scm

check-test: all
	cd ../test ; ../unix/debug/foment --check-heap runtests.scm r7rs.scm r7rs-eval.scm foment.scm eccentric.scm r5rs_pitfall.scm unicode.scm srfi.scm
	cd ../test ; ../unix/debug/foment --check-heap r7rs.scm
	cd ../test ; ../unix/release/foment --check-heap runtests.scm r7rs.scm r7rs-eval.scm foment.scm eccentric.scm r5rs_pitfall.scm unicode.scm srfi.scm
	cd ../test ; ../unix/release/foment --check-heap r7rs.scm

stress: all
	cd ../test ; ../unix/debug/foment runtests.scm stress.scm r7rs.scm
	cd ../test ; ../unix/release/foment runtests.scm stress.scm r7rs.scm

check-stress: all
	cd ../test ; ../unix/debug/foment --check-heap runtests.scm stress.scm r7rs.scm
	cd ../test ; ../unix/release/foment --check-heap runtests.scm stress.scm r7rs.scm

chibi: all
	cd ../test ; ../unix/debug/foment r7rs-tests.scm
	cd ../test ; ../unix/release/foment r7rs-tests.scm

check-chibi: all
	cd ../test ; ../unix/debug/foment --check-heap r7rs-tests.scm
	cd ../test ; ../unix/release/foment --check-heap r7rs-tests.scm

debug:
	-mkdir debug

release:
	-mkdir release

profile:
	-mkdir profile

debug/base.cpp: debug/txt2cpp ../src/base.scm makefile
	debug/txt2cpp ../src/base.scm debug/base.cpp BaseCode

debug/foment: debug/foment.o debug/gc.o debug/syncthrd.o debug/compile.o debug/io.o\
		debug/synrules.o debug/synpass.o debug/midpass.o debug/genpass.o\
		debug/pairs.o debug/unicode.o debug/chars.o debug/strings.o debug/vectors.o\
		debug/library.o debug/execute.o debug/numbers.o	debug/mini-gmp.o debug/write.o\
		debug/read.o debug/filesys.o debug/hcontain.o debug/compare.o debug/main.o\
		debug/base.o
	g++ -o debug/foment $^ -lpthread

release/foment: release/foment.o release/gc.o release/syncthrd.o release/compile.o release/io.o\
		release/synrules.o release/synpass.o release/midpass.o release/genpass.o\
		release/pairs.o release/unicode.o release/chars.o release/strings.o\
		release/vectors.o release/library.o release/execute.o release/numbers.o\
		release/mini-gmp.o release/write.o release/read.o release/filesys.o\
		release/hcontain.o release/compare.o release/main.o release/base.o
	g++ -o release/foment $^ -lpthread

profile/foment: profile/foment.o profile/gc.o profile/syncthrd.o profile/compile.o profile/io.o\
		profile/synrules.o profile/synpass.o profile/midpass.o profile/genpass.o\
		profile/pairs.o profile/unicode.o profile/chars.o profile/strings.o\
		profile/vectors.o profile/library.o profile/execute.o profile/numbers.o\
		profile/mini-gmp.o profile/write.o profile/read.o profile/filesys.o\
		profile/hcontain.o profile/compare.o profile/main.o profile/base.o
	g++ -pg -o profile/foment $^ -lpthread

debug/foment.o: ../src/foment.cpp ../src/foment.hpp ../src/unicode.hpp
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
debug/numbers.o: ../src/numbers.cpp ../src/foment.hpp ../src/unicode.hpp ../src/mini-gmp.h
debug/mini-gmp.o: ../src/mini-gmp.c ../src/mini-gmp.h
debug/io.o: ../src/io.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
debug/write.o: ../src/write.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/compile.hpp
debug/read.o: ../src/read.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
debug/filesys.o: ../src/filesys.cpp ../src/foment.hpp ../src/unicode.hpp
debug/hcontain.o: ../src/hcontain.cpp ../src/foment.hpp
debug/compare.o: ../src/compare.cpp ../src/foment.hpp
debug/main.o: ../src/main.cpp ../src/foment.hpp
debug/base.o: debug/base.cpp

release/foment.o: ../src/foment.cpp ../src/foment.hpp ../src/unicode.hpp
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
release/numbers.o: ../src/numbers.cpp ../src/foment.hpp ../src/unicode.hpp ../src/mini-gmp.h
debug/mini-gmp.o: ../src/mini-gmp.c ../src/mini-gmp.h
release/io.o: ../src/io.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
release/write.o: ../src/write.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/compile.hpp
release/read.o: ../src/read.cpp ../src/foment.hpp ../src/syncthrd.hpp ../src/io.hpp\
	../src/unicode.hpp
release/filesys.o: ../src/filesys.cpp ../src/foment.hpp ../src/unicode.hpp
release/hcontain.o: ../src/hcontain.cpp ../src/foment.hpp
release/compare.o: ../src/compare.cpp ../src/foment.hpp
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

profile/%.o: %.cpp
	cc $(CCPROFILE) -I ../src -o $@ $<

profile/%.o: ../src/%.cpp
	cc $(CCPROFILE) -I ../src -o $@ $<

profile/%.o: ../src/%.c
	cc $(CCPROFILE) -I ../src -o $@ $<

profile/%.o: debug/%.cpp
	cc $(CCPROFILE) -I ../src -o $@ $<

debug/txt2cpp: ../src/txt2cpp.cpp
	g++ $(CCDEBUG) ../src/txt2cpp.cpp -o debug/txt2cpp.o
	g++ debug/txt2cpp.o -o debug/txt2cpp
