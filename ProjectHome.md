[Foment](Foment.md) is an implementation of Scheme.

  * Full R7RS.
  * Libraries and programs work.
  * Native threads and some synchronization primitives.
  * Memory management using a garbage collector with two copying and one mark-sweep generations. Guardians protect objects from being collected and trackers follow objects as they get moved by the copying part of the collector.
  * Full Unicode including reading and writing unicode characters to the console. Files in UTF-8 and UTF-16 encoding can be read and written.
  * The system is built around a compiler and VM. There is support for prompts and continuation marks.
  * Network support.
  * Editing at the REPL including () matching.
  * Portable: Windows, Linux, and FreeBSD.
  * 32 bit and 64 bit.
  * SRFI 106: Basic socket interface.
  * SRFI 111: Boxes.
  * SRFI 112: Environment Inquiry.

See [Foment](Foment.md) for more details.

Future plans include

  * Providing line numbers and stack traces on errors.
  * R7RS large SRFIs.
  * composable continuations

Please note that this is very much a work in progress. Please let me know if you find bugs and omissions. I will do my best to fix them.

mikemon@gmail.com