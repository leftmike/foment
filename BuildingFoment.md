# Building Foment #

Use git to get a local copy of the repository.

## Windows ##

Install Microsoft Visual C++ Express 2010 or Microsoft Visual Studio Express 2013 for Desktop.
Use the 2013 version if you want to build a 64 bit version.
Note that Microsoft Visual C++ Express and Microsoft Visual Studio Express do not cost
anything.

2010: Open a Visual Studio Command Prompt; this will set your environment variables correctly.

2013 for 32 bit: Open a Developer Command Prompt for VS2013; this will set your environment
variables correctly.

2013 for 64 bit: Open VS2013 x64 Cross Tools Command Prompt; this will set your environment
variables correctly.

```
cd foment\windows
nmake
nmake test
nmake stress
```

`debug\foment.exe` contains extra checks and debugging information so that you can use the
debugger built into Visual Studio to debug the C level code only.

`release\foment.exe` should be a little faster, but I recommend using `debug\foment.exe` at
this point.

## Linux ##

I have built it on Debian 32 bit and Debian 64 bit.

```
cd foment/unix
make
make test
make stress
```

`debug/foment` contains extra checks and debugging information so that you can use gdb
to debug the C level code only.

`release/foment` should be a little faster, but I recommend using `debug/foment` at
this point.