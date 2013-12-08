#
# Foment
#

CLDEBUG = /nologo /MD /W3 /Gm /EHsc /Zi /Od /c /Fodebug/ /DFOMENT_DEBUG /DFOMENT_WINDOWS
CLRELEASE = /nologo /MD /W3 /O1sy /Gs /GA /EHsc /Forelease/ /c /DFOMENT_WINDOWS

all: debug release debug\foment.exe release\foment.exe

clean: debug release manual
    del /Q debug\*
    del /Q release\*
    del /Q manual\*

test: all
    cd ..\test
    ..\windows\debug\foment runtests.scm r7rs.scm r7rs-eval.scm foment.scm eccentric.scm\
            r5rs_pitfall.scm unicode.scm srfi.scm
    ..\windows\debug\foment r7rs.scm
    ..\windows\release\foment runtests.scm r7rs.scm r7rs-eval.scm foment.scm eccentric.scm\
            r5rs_pitfall.scm unicode.scm srfi.scm
    ..\windows\release\foment r7rs.scm

stress: all
    cd ..\test
    ..\windows\debug\foment runtests.scm stress.scm r7rs.scm
    ..\windows\release\foment runtests.scm stress.scm r7rs.scm

chibi: all
    cd ..\test
    ..\windows\debug\foment r7rs-tests.scm
    ..\windows\release\foment r7rs-tests.scm

debug:
    -mkdir debug

release:
    -mkdir release

manual:
    -mkdir manual

uni: debug\genudf.exe
    debug\genudf ..\unidata\CaseFolding.txt Fullfold 2 20
    debug\genudf ..\unidata\SpecialCasing.txt Fullup 3 20

unicode: debug\genaul.exe debug\genudf.exe
    type << > ..\src\unidata.hpp
/*

Foment

*/

// Automatically generated unicode data and code included by unicode.cpp.

<<
    debug\genudf ..\unidata\UnicodeData.txt Upcase 12 50 >> ..\src\unidata.hpp
    debug\genudf ..\unidata\UnicodeData.txt Downcase 13 50 >> ..\src\unidata.hpp
    debug\genudf ..\unidata\CaseFolding.txt Foldcase 2 50 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic 0 1FE0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic 2060 2180 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic 24A0 24E0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic 2C00 2E20 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic 3000 31E0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic A480 ABE0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic D7A0 D7E0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic F900 10480 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic 10800 10C40 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic 11000 111C0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic 16F00 16F80 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic 1D400 1D7C0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Alphabetic 1EE00 1EEA0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Uppercase 0 0580 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Uppercase 10A0 10C0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Uppercase 1E00 1FE0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Uppercase 2100 2180 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Uppercase 2C00 2D20 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Uppercase A640 A7A0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Uppercase 1D400 1D7C0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Lowercase 0 0580 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Lowercase 1D00 1FE0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Lowercase 2060 2180 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Lowercase 2C20 2D20 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Lowercase A640 A7E0 >> ..\src\unidata.hpp
    debug\genaul ..\unidata\DerivedCoreProperties.txt Lowercase 1D400 1D7C0 >> ..\src\unidata.hpp
    debug\genudf ..\unidata\CaseFolding.txt Fullfold 2 20 >> ..\src\unidata.hpp
    debug\genudf ..\unidata\SpecialCasing.txt Fullup 3 20 >> ..\src\unidata.hpp

debug\base.cpp: debug\txt2cpp.exe ..\src\base.scm makefile
    debug\txt2cpp ..\src\base.scm debug\base.cpp BaseCode

debug\foment.exe: debug\foment.obj debug\gc.obj debug\syncthrd.obj debug\compile.obj debug\io.obj\
        debug\synrules.obj debug\synpass.obj debug\midpass.obj debug\genpass.obj\
        debug\pairs.obj debug\unicode.obj debug\chars.obj debug\strings.obj debug\vectors.obj\
        debug\library.obj debug\execute.obj debug\numbers.obj debug\mini-gmp.obj debug\write.obj\
        debug\read.obj debug\main.obj debug\base.obj
    link /nologo /subsystem:console /out:debug\foment.exe /debug /pdb:debug\foment.pdb $**

release\foment.exe: release\foment.obj release\gc.obj release\syncthrd.obj release\compile.obj\
        release\io.obj release\synrules.obj release\synpass.obj release\midpass.obj\
        release\genpass.obj release\pairs.obj release\unicode.obj release\chars.obj\
        release\strings.obj release\vectors.obj release\library.obj release\execute.obj\
        release\numbers.obj release\mini-gmp.obj release\write.obj release\read.obj\
        release\main.obj release\base.obj
    link /nologo /subsystem:console /out:release\foment.exe $**

debug\txt2cpp.exe: debug\txt2cpp.obj
    link /nologo /subsystem:console /out:debug\txt2cpp.exe\
    debug\txt2cpp.obj

debug\foment.obj: ..\src\foment.cpp ..\src\foment.hpp ..\src\unicode.hpp
debug\gc.obj: ..\src\gc.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp ..\src\numbers.hpp
debug\syncthrd.obj: ..\src\syncthrd.cpp ..\src\foment.hpp ..\src\execute.hpp ..\src\syncthrd.hpp
debug\compile.obj: ..\src\compile.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\synrules.obj: ..\src\synrules.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\synpass.obj: ..\src\synpass.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\midpass.obj: ..\src\midpass.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\genpass.obj: ..\src\genpass.cpp ..\src\foment.hpp ..\src\compile.hpp ..\src\execute.hpp
debug\pairs.obj: ..\src\pairs.cpp ..\src\foment.hpp
debug\unicode.obj: ..\src\unicode.cpp ..\src\foment.hpp ..\src\unicode.hpp ..\src\unidata.hpp
debug\chars.obj: ..\src\chars.cpp ..\src\foment.hpp ..\src\unicode.hpp
debug\strings.obj: ..\src\strings.cpp ..\src\foment.hpp ..\src\unicode.hpp
debug\vectors.obj: ..\src\vectors.cpp ..\src\foment.hpp ..\src\unicode.hpp
debug\library.obj: ..\src\library.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\execute.obj: ..\src\execute.cpp ..\src\foment.hpp ..\src\execute.hpp ..\src\syncthrd.hpp
debug\numbers.obj: ..\src\numbers.cpp ..\src\foment.hpp ..\src\unicode.hpp ..\src\numbers.hpp
debug\mini-gmp.obj: ..\src\mini-gmp.c ..\src\mini-gmp.h
debug\io.obj: ..\src\io.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp ..\src\unicode.hpp
debug\write.obj: ..\src\write.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\compile.hpp
debug\read.obj: ..\src\read.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\unicode.hpp
debug\main.obj: ..\src\main.cpp ..\src\foment.hpp
debug\base.obj: debug\base.cpp

release\foment.obj: ..\src\foment.cpp ..\src\foment.hpp ..\src\unicode.hpp
release\gc.obj: ..\src\gc.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\numbers.hpp
release\syncthrd.obj: ..\src\syncthrd.cpp ..\src\foment.hpp ..\src\execute.hpp ..\src\syncthrd.hpp
release\compile.obj: ..\src\compile.cpp ..\src\foment.hpp ..\src\compile.hpp
release\synrules.obj: ..\src\synrules.cpp ..\src\foment.hpp ..\src\compile.hpp
release\synpass.obj: ..\src\synpass.cpp ..\src\foment.hpp ..\src\compile.hpp
release\midpass.obj: ..\src\midpass.cpp ..\src\foment.hpp ..\src\compile.hpp
release\genpass.obj: ..\src\genpass.cpp ..\src\foment.hpp ..\src\compile.hpp ..\src\execute.hpp
release\pairs.obj: ..\src\pairs.cpp ..\src\foment.hpp
release\unicode.obj: ..\src\unicode.cpp ..\src\foment.hpp ..\src\unicode.hpp ..\src\unidata.hpp
release\chars.obj: ..\src\chars.cpp ..\src\foment.hpp ..\src\unicode.hpp
release\strings.obj: ..\src\strings.cpp ..\src\foment.hpp ..\src\unicode.hpp
release\vectors.obj: ..\src\vectors.cpp ..\src\foment.hpp ..\src\unicode.hpp
release\library.obj: ..\src\library.cpp ..\src\foment.hpp ..\src\compile.hpp
release\execute.obj: ..\src\execute.cpp ..\src\foment.hpp ..\src\execute.hpp ..\src\syncthrd.hpp
release\numbers.obj: ..\src\numbers.cpp ..\src\foment.hpp ..\src\unicode.hpp ..\src\numbers.hpp
release\mini-gmp.obj: ..\src\mini-gmp.c ..\src\mini-gmp.h
release\io.obj: ..\src\io.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\unicode.hpp
release\write.obj: ..\src\write.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\compile.hpp
release\read.obj: ..\src\read.cpp ..\src\foment.hpp ..\src\syncthrd.hpp ..\src\io.hpp\
    ..\src\unicode.hpp
release\main.obj: ..\src\main.cpp ..\src\foment.hpp
release\base.obj: debug\base.cpp

..\src\numbers.hpp: ..\src\mini-gmp.h

debug\txt2cpp.obj: ..\src\txt2cpp.cpp

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

{.}.cpp.{debug}.exe:
    cl $(CLDEBUG) $(*B).cpp
    link /nologo /subsystem:console /out:debug\$(*B).exe\
    debug\$(*B).obj

