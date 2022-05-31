#
# Foment
#

PATH = $(PATH);../windows/debug

TEST_OPTIONS = --check-heap --zero-heap
TEST_BUILD = debug

CLDEBUG = /nologo /MD /W3 /EHsc /Zi /Od /c /Fodebug\ /DFOMENT_DEBUG /DFOMENT_WINDOWS /std:c++latest
CLRELEASE = /nologo /MD /W3 /Ox /Zi /GA /EHsc /Forelease\ /c /DFOMENT_WINDOWS /std:c++latest

LIBS = ws2_32.lib iphlpapi.lib

all: debug release debug\genprops.exe debug\foment.exe release\foment.exe\
        debug\stdread.exe debug\stdwrite.exe debug\exitcode.exe debug\hang.exe makefile

clean: debug release
    del /Q debug\*
    del /Q release\*

test: all foment-test stress-test chibi-test process-test threads-test

test-all: all test-no-collector test-mark-sweep

test-no-collector: all
    $(MAKE) test TEST_BUILD=debug TEST_OPTIONS=--no-collector
    $(MAKE) test TEST_BUILD=release TEST_OPTIONS=--no-collector
    $(MAKE) test TEST_BUILD=debug "TEST_OPTIONS=--no-collector --check-heap --zero-heap"
    $(MAKE) test TEST_BUILD=release "TEST_OPTIONS=--no-collector --check-heap --zero-heap"

test-mark-sweep: all
    $(MAKE) test TEST_BUILD=debug TEST_OPTIONS=--mark-sweep
    $(MAKE) test TEST_BUILD=release TEST_OPTIONS=--mark-sweep
    $(MAKE) test TEST_BUILD=debug "TEST_OPTIONS=--mark-sweep --check-heap --zero-heap"
    $(MAKE) test TEST_BUILD=release "TEST_OPTIONS=--mark-sweep --check-heap --zero-heap"

foment-test: all
    cd ..\test
    ..\windows\$(TEST_BUILD)\foment $(TEST_OPTIONS) runtests.scm r7rs.scm r7rs-eval.scm foment.scm\
            eccentric.scm r5rs_pitfall.scm unicode.scm srfi.scm
    ..\windows\$(TEST_BUILD)\foment $(TEST_OPTIONS) r7rs.scm

stress-test: all
    cd ..\test
    ..\windows\$(TEST_BUILD)\foment $(TEST_OPTIONS) runtests.scm stress.scm r7rs.scm

chibi-test: all
    cd ..\test
    ..\windows\$(TEST_BUILD)\foment $(TEST_OPTIONS) runtests.scm stress.scm r7rs-tests.scm

process-test: all
    cd ..\test
    ..\windows\$(TEST_BUILD)\foment $(TEST_OPTIONS) runtests.scm process.scm

threads-test: all
    cd ..\test
    ..\windows\$(TEST_BUILD)\foment $(TEST_OPTIONS) runtests.scm threads.scm

debug:
    -mkdir debug

release:
    -mkdir release

debug\base.cpp: debug\txt2cpp.exe makefile ..\src\base.scm ..\src\srfi-106.scm ..\src\srfi-60.scm\
        ..\src\srfi-1.scm ..\src\srfi-128.scm ..\src\srfi-125.scm ..\src\srfi-133.scm\
        ..\src\srfi-14.scm ..\src\srfi-193.scm ..\src\srfi-151.scm
    debug\txt2cpp debug\base.cpp ..\src\base.scm ..\src\srfi-106.scm ..\src\srfi-60.scm\
        ..\src\srfi-1.scm ..\src\srfi-128.scm ..\src\srfi-125.scm ..\src\srfi-133.scm\
        ..\src\srfi-14.scm ..\src\srfi-193.scm ..\src\srfi-151.scm

debug\foment.exe: debug\foment.obj debug\gc.obj debug\syncthrd.obj debug\compile.obj debug\io.obj\
        debug\synrules.obj debug\synpass.obj debug\midpass.obj debug\genpass.obj\
        debug\pairs.obj debug\unicode.obj debug\chars.obj debug\strings.obj debug\vectors.obj\
        debug\library.obj debug\execute.obj debug\numbers.obj debug\write.obj\
        debug\read.obj debug\filesys.obj debug\compare.obj debug\main.obj debug\hashtbl.obj\
        debug\bignums.obj debug\charset.obj debug\process.obj debug\random.obj debug\base.obj
    git rev-parse --abbrev-ref HEAD > debug\buildprops.out
    git rev-parse --short HEAD >> debug\buildprops.out
    ver >> debug\buildprops.out
    cl > nul 2>> debug\buildprops.out
    debug\genprops.exe < debug\buildprops.out > debug\buildprops.cpp
    cl $(CLDEBUG) debug\buildprops.cpp
    link /nologo /subsystem:console /out:debug\foment.exe /debug /pdb:debug\foment.pdb\
        /largeaddressaware $** debug\buildprops.obj $(LIBS)

release\foment.exe: release\foment.obj release\gc.obj release\syncthrd.obj release\compile.obj\
        release\io.obj release\synrules.obj release\synpass.obj release\midpass.obj\
        release\genpass.obj release\pairs.obj release\unicode.obj release\chars.obj\
        release\strings.obj release\vectors.obj release\library.obj release\execute.obj\
        release\numbers.obj release\write.obj release\read.obj\
        release\filesys.obj release\compare.obj release\main.obj release\hashtbl.obj\
        release\bignums.obj release\charset.obj release\process.obj release\random.obj\
        release\base.obj
    git rev-parse --abbrev-ref HEAD > release\buildprops.out
    git rev-parse --short HEAD >> release\buildprops.out
    ver >> release\buildprops.out
    cl > nul 2>> release\buildprops.out
    debug\genprops.exe < release\buildprops.out > release\buildprops.cpp
    cl $(CLRELEASE) release\buildprops.cpp
#    link /nologo /subsystem:console /out:release\foment.exe $**
    link /nologo /subsystem:console /out:release\foment.exe /debug /pdb:release\foment.pdb\
        /largeaddressaware $** release\buildprops.obj $(LIBS)

debug\txt2cpp.exe: debug\txt2cpp.obj
    link /nologo /subsystem:console /out:debug\txt2cpp.exe debug\txt2cpp.obj

debug\stdread.exe: debug\stdread.obj
    link /nologo /subsystem:console /out:debug\stdread.exe debug\stdread.obj

debug\stdwrite.exe: debug\stdwrite.obj
    link /nologo /subsystem:console /out:debug\stdwrite.exe debug\stdwrite.obj

debug\exitcode.exe: debug\exitcode.obj
    link /nologo /subsystem:console /out:debug\exitcode.exe debug\exitcode.obj

debug\hang.exe: debug\hang.obj
    link /nologo /subsystem:console /out:debug\hang.exe debug\hang.obj

debug\genprops.exe: debug\genprops.obj
    link /nologo /subsystem:console /out:debug\genprops.exe debug\genprops.obj

debug\foment.obj: ..\src\foment.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\unicode.hpp
debug\gc.obj: ..\src\gc.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp
debug\syncthrd.obj: ..\src\syncthrd.cpp ..\src\foment.hpp ..\src\execute.hpp ..\src\syncthrd.hpp
debug\compile.obj: ..\src\compile.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\synrules.obj: ..\src\synrules.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\synpass.obj: ..\src\synpass.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\midpass.obj: ..\src\midpass.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\genpass.obj: ..\src\genpass.cpp ..\src\foment.hpp ..\src\compile.hpp ..\src\execute.hpp
debug\pairs.obj: ..\src\pairs.cpp ..\src\foment.hpp
debug\unicode.obj: ..\src\unicode.cpp ..\src\foment.hpp ..\src\unicode.hpp ..\src\unicase.hpp
debug\chars.obj: ..\src\chars.cpp ..\src\foment.hpp ..\src\unicode.hpp
debug\strings.obj: ..\src\strings.cpp ..\src\foment.hpp ..\src\unicode.hpp
debug\vectors.obj: ..\src\vectors.cpp ..\src\foment.hpp ..\src\unicode.hpp
debug\library.obj: ..\src\library.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\execute.obj: ..\src\execute.cpp ..\src\foment.hpp ..\src\execute.hpp ..\src\syncthrd.hpp
debug\numbers.obj: ..\src\numbers.cpp ..\src\foment.hpp ..\src\unicode.hpp ..\src\bignums.hpp
debug\bignums.obj: ..\src\bignums.cpp ..\src\foment.hpp ..\src\unicode.hpp ..\src\bignums.hpp
debug\charset.obj: ..\src\charset.cpp ..\src\foment.hpp ..\src\unicode.hpp  ..\src\unicrng.hpp
debug\process.obj: ..\src\process.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\unicode.hpp
debug\io.obj: ..\src\io.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp ..\src\unicode.hpp
debug\write.obj: ..\src\write.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\compile.hpp
debug\read.obj: ..\src\read.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\unicode.hpp
debug\filesys.obj: ..\src\filesys.cpp ..\src\foment.hpp ..\src\unicode.hpp
debug\hashtbl.obj: ..\src\hashtbl.cpp ..\src\foment.hpp ..\src\syncthrd.hpp
debug\compare.obj: ..\src\compare.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\unicode.hpp
debug\main.obj: ..\src\main.cpp ..\src\foment.hpp
debug\random.obj: ..\src\random.cpp ..\src\foment.hpp
debug\base.obj: debug\base.cpp

release\foment.obj: ..\src\foment.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\unicode.hpp
release\gc.obj: ..\src\gc.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp
release\syncthrd.obj: ..\src\syncthrd.cpp ..\src\foment.hpp ..\src\execute.hpp ..\src\syncthrd.hpp
release\compile.obj: ..\src\compile.cpp ..\src\foment.hpp ..\src\compile.hpp
release\synrules.obj: ..\src\synrules.cpp ..\src\foment.hpp ..\src\compile.hpp
release\synpass.obj: ..\src\synpass.cpp ..\src\foment.hpp ..\src\compile.hpp
release\midpass.obj: ..\src\midpass.cpp ..\src\foment.hpp ..\src\compile.hpp
release\genpass.obj: ..\src\genpass.cpp ..\src\foment.hpp ..\src\compile.hpp ..\src\execute.hpp
release\pairs.obj: ..\src\pairs.cpp ..\src\foment.hpp
release\unicode.obj: ..\src\unicode.cpp ..\src\foment.hpp ..\src\unicode.hpp ..\src\unicase.hpp
release\chars.obj: ..\src\chars.cpp ..\src\foment.hpp ..\src\unicode.hpp
release\strings.obj: ..\src\strings.cpp ..\src\foment.hpp ..\src\unicode.hpp
release\vectors.obj: ..\src\vectors.cpp ..\src\foment.hpp ..\src\unicode.hpp
release\library.obj: ..\src\library.cpp ..\src\foment.hpp ..\src\compile.hpp
release\execute.obj: ..\src\execute.cpp ..\src\foment.hpp ..\src\execute.hpp ..\src\syncthrd.hpp
release\numbers.obj: ..\src\numbers.cpp ..\src\foment.hpp ..\src\unicode.hpp ..\src\bignums.hpp
release\bignums.obj: ..\src\bignums.cpp ..\src\foment.hpp ..\src\unicode.hpp ..\src\bignums.hpp
release\charset.obj: ..\src\charset.cpp ..\src\foment.hpp ..\src\unicode.hpp  ..\src\unicrng.hpp
release\process.obj: ..\src\process.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\unicode.hpp
release\io.obj: ..\src\io.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\unicode.hpp
release\write.obj: ..\src\write.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\compile.hpp
release\read.obj: ..\src\read.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\unicode.hpp
release\filesys.obj: ..\src\filesys.cpp ..\src\foment.hpp ..\src\unicode.hpp
release\hashtbl.obj: ..\src\hashtbl.cpp ..\src\foment.hpp ..\src\syncthrd.hpp
release\compare.obj: ..\src\compare.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\unicode.hpp
release\main.obj: ..\src\main.cpp ..\src\foment.hpp
release\random.obj: ..\src\random.cpp ..\src\foment.hpp
release\base.obj: debug\base.cpp

debug\txt2cpp.obj: ..\src\txt2cpp.cpp
debug\genprops.obj: ..\src\genprops.cpp

debug\stdread.obj: ..\test\stdread.cpp
debug\stdwrite.obj: ..\test\stdwrite.cpp
debug\exitcode.obj: ..\test\exitcode.cpp
debug\hang.obj: ..\test\hang.cpp

{.}.cpp.{debug}.obj:
    cl $(CLDEBUG) $(*B).cpp

{..\src\}.cpp.{debug}.obj:
    cl $(CLDEBUG) ..\src\$(*B).cpp

{..\src\}.c.{debug}.obj:
    cl $(CLDEBUG) ..\src\$(*B).c

{debug\}.cpp.{debug}.obj:
    cl $(CLDEBUG) debug\$(*B).cpp

{.}.cpp.{release}.obj:
    cl $(CLRELEASE) $(*B).cpp

{..\src\}.cpp.{release}.obj:
    cl $(CLRELEASE) ..\src\$(*B).cpp

{..\src\}.c.{release}.obj:
    cl $(CLRELEASE) ..\src\$(*B).c

{debug\}.cpp.{release}.obj:
    cl $(CLRELEASE) debug\$(*B).cpp

{..\test\}.cpp.{debug}.obj:
    cl $(CLDEBUG) ..\test\$(*B).cpp
