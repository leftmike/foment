#
# Foment
#

CLDEBUG = /nologo /MD /W3 /Gm /EHsc /Zi /Od /c /Fodebug/ /DFOMENT_DEBUG /DFOMENT_TEST\
        /DFOMENT_GCCHK /DFOMENT_WIN32
CLRELEASE = /nologo /MD /W3 /O1sy /Gs /GA /EHsc /Forelease/ /c /DFOMENT_TEST /DFOMENT_WIN32

all: debug release debug\foment.exe release\foment.exe

doc: manual
    @cd manual
    makeinfo --force --html --no-split ..\..\doc\foment.texi
    makeinfo --force ..\..\doc\foment.texi
    texi2dvi --pdf ..\..\doc\foment.texi
    @cd ..

clean: debug release manual
    del /Q debug\*
    del /Q release\*
    del /Q manual\*

test: all
    debug\foment -I ..\test -t ..\test\r7rs.scm ..\test\foment.scm ..\test\eccentric.scm
    release\foment -I ..\test -t ..\test\r7rs.scm ..\test\foment.scm ..\test\eccentric.scm

stress: all
    debug\foment -I ..\test -t ..\test\stress.scm ..\test\r7rs.scm
    release\foment -I ..\test -t ..\test\stress.scm ..\test\r7rs.scm

debug:
    -mkdir debug

release:
    -mkdir release

manual:
    -mkdir manual

debug\startup.cpp: debug\txt2cpp.exe ..\src\startup.scm makefile
    debug\txt2cpp ..\src\startup.scm debug\startup.cpp StartupCode

debug\foment.exe: debug\foment.obj debug\gc.obj debug\syncthrd.obj debug\compile.obj debug\io.obj\
        debug\synrules.obj debug\synpass.obj debug\midpass.obj debug\genpass.obj\
        debug\pairs.obj debug\strings.obj debug\vectors.obj debug\library.obj\
        debug\execute.obj debug\numbers.obj debug\pretty.obj debug\main.obj debug\startup.obj\
        debug\test.obj
    link /nologo /subsystem:console /out:debug\foment.exe\
        /debug /pdb:debug\foment.pdb\
        debug\foment.obj debug\gc.obj debug\syncthrd.obj debug\compile.obj debug\io.obj\
        debug\synrules.obj debug\synpass.obj debug\midpass.obj debug\genpass.obj debug\pairs.obj\
        debug\strings.obj debug\vectors.obj debug\library.obj debug\execute.obj debug\numbers.obj\
        debug\pretty.obj debug\main.obj debug\startup.obj debug\test.obj

release\foment.exe: release\foment.obj release\gc.obj release\syncthrd.obj release\compile.obj\
        release\io.obj release\synrules.obj release\synpass.obj release\midpass.obj\
        release\genpass.obj release\pairs.obj release\strings.obj release\vectors.obj\
        release\library.obj release\execute.obj release\numbers.obj release\pretty.obj\
        release\main.obj release\startup.obj release\test.obj
    link /nologo /subsystem:console /out:release\foment.exe\
        release\foment.obj release\gc.obj  release\syncthrd.obj release\compile.obj\
        release\io.obj release\synrules.obj release\synpass.obj release\midpass.obj\
        release\genpass.obj release\pairs.obj release\strings.obj release\vectors.obj\
        release\library.obj release\execute.obj release\numbers.obj release\pretty.obj\
        release\main.obj release\startup.obj release\test.obj

debug\txt2cpp.exe: debug\txt2cpp.obj
    link /nologo /subsystem:console /out:debug\txt2cpp.exe\
    debug\txt2cpp.obj

debug\foment.obj: ..\src\foment.cpp ..\src\foment.hpp
debug\gc.obj: ..\src\gc.cpp ..\src\foment.hpp ..\src\io.hpp ..\src\syncthrd.hpp
debug\syncthrd.obj: ..\src\syncthrd.cpp ..\src\foment.hpp ..\src\syncthrd.hpp
debug\compile.obj: ..\src\compile.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\synrules.obj: ..\src\synrules.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\synpass.obj: ..\src\synpass.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\midpass.obj: ..\src\midpass.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\genpass.obj: ..\src\genpass.cpp ..\src\foment.hpp ..\src\compile.hpp ..\src\execute.hpp
debug\pairs.obj: ..\src\pairs.cpp ..\src\foment.hpp
debug\strings.obj: ..\src\strings.cpp ..\src\foment.hpp
debug\vectors.obj: ..\src\vectors.cpp ..\src\foment.hpp
debug\library.obj: ..\src\library.cpp ..\src\foment.hpp ..\src\compile.hpp
debug\execute.obj: ..\src\execute.cpp ..\src\foment.hpp ..\src\execute.hpp ..\src\syncthrd.hpp
debug\numbers.obj: ..\src\numbers.cpp ..\src\foment.hpp
debug\io.obj: ..\src\io.cpp ..\src\foment.hpp ..\src\io.hpp
debug\pretty.obj: ..\src\pretty.cpp ..\src\foment.hpp ..\src\io.hpp ..\src\compile.hpp
debug\main.obj: ..\src\main.cpp ..\src\foment.hpp
debug\startup.obj: debug\startup.cpp
debug\test.obj: ..\src\test.cpp ..\src\foment.hpp

release\foment.obj: ..\src\foment.cpp ..\src\foment.hpp
release\gc.obj: ..\src\gc.cpp ..\src\foment.hpp ..\src\io.hpp ..\src\syncthrd.hpp
release\syncthrd.obj: ..\src\syncthrd.cpp ..\src\foment.hpp ..\src\syncthrd.hpp
release\compile.obj: ..\src\compile.cpp ..\src\foment.hpp ..\src\compile.hpp
release\synrules.obj: ..\src\synrules.cpp ..\src\foment.hpp ..\src\compile.hpp
release\synpass.obj: ..\src\synpass.cpp ..\src\foment.hpp ..\src\compile.hpp
release\midpass.obj: ..\src\midpass.cpp ..\src\foment.hpp ..\src\compile.hpp
release\genpass.obj: ..\src\genpass.cpp ..\src\foment.hpp ..\src\compile.hpp ..\src\execute.hpp
release\pairs.obj: ..\src\pairs.cpp ..\src\foment.hpp
release\strings.obj: ..\src\strings.cpp ..\src\foment.hpp
release\vectors.obj: ..\src\vectors.cpp ..\src\foment.hpp
release\library.obj: ..\src\library.cpp ..\src\foment.hpp ..\src\compile.hpp
release\execute.obj: ..\src\execute.cpp ..\src\foment.hpp ..\src\execute.hpp ..\src\syncthrd.hpp
release\numbers.obj: ..\src\numbers.cpp ..\src\foment.hpp
release\io.obj: ..\src\io.cpp ..\src\foment.hpp ..\src\io.hpp
release\pretty.obj: ..\src\pretty.cpp ..\src\foment.hpp ..\src\io.hpp ..\src\compile.hpp
release\main.obj: ..\src\main.cpp ..\src\foment.hpp
release\startup.obj: debug\startup.cpp
release\test.obj: ..\src\test.cpp ..\src\foment.hpp

debug\txt2cpp.obj: ..\src\txt2cpp.cpp

{.}.cpp.{debug}.obj:
    cl $(CLDEBUG) $(*B).cpp

{..\src\}.cpp.{debug}.obj:
    cl $(CLDEBUG) ..\src\$(*B).cpp

{debug\}.cpp.{debug}.obj:
    cl $(CLDEBUG) debug\$(*B).cpp

{.}.cpp.{release}.obj:
    cl $(CLRELEASE) $(*B).cpp

{..\src\}.cpp.{release}.obj:
    cl $(CLRELEASE) ..\src\$(*B).cpp

{debug\}.cpp.{release}.obj:
    cl $(CLRELEASE) debug\$(*B).cpp

