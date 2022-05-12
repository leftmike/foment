[Foment](https://github.com/leftmike/foment/wiki/Foment) is an implementation of Scheme.

* Full R7RS.
* Libraries and programs work.
* Native threads and some synchronization primitives.
* [Proccess](Processes) is a subset of the [Racket](https://racket-lang.org/)
[Processes API](https://docs.racket-lang.org/reference/subprocess.html).
* Memory management including guardians. Guardians protect objects from being collected.
* Full Unicode including reading and writing unicode characters to the console. Files in UTF-8 and UTF-16 encoding can be read and written.
* The system is built around a compiler and VM. There is support for prompts and continuation marks.
* Network support.
* Editing at the REPL including ( ) matching.
* Portable: Windows, Mac OS X, Linux, and FreeBSD.
* [Package](https://gitlab.com/jpellegrini/openwrt-packages) for OpenWRT.
* [Dockerfile](https://github.com/weinholt/scheme-docker/tree/foment/foment).
* 32 bit and 64 bit.
* SRFI 1: List Library
* SRFI 14: Character-set Library
* SRFI 22: Running Scheme Scripts on Unix
* SRFI 27: Sources of Random Bits
* SRFI 39: Parameter objects
* SRFI 60: Integers as Bits
* SRFI 106: Basic socket interface
* SRFI 111: Boxes
* SRFI 112: Environment Inquiry
* SRFI 124: Ephemerons
* SRFI 125: Hash Tables
* SRFI 128: Comparators
* SRFI 133: Vector Library (R7RS-compatible)
* SRFI 157: Continuation marks
* SRFI 176: Version flag
* SRFI 181: Custom ports (including transcoded ports)
* SRFI 192: Port Positioning
* SRFI 193: Command line
* SRFI 229: Tagged Procedures

See [Foment](https://github.com/leftmike/foment/wiki/Foment) for more details.

Please note that this is very much a work in progress. Please let me know if
you find bugs and omissions. I will do my best to fix them.

mikemon@gmail.com
