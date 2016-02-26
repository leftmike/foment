# Release Notes #

## 0.1 ##

23 Oct 2013: initial release

## 0.2 ##

5 November 2013

  * command line argument handling changed
  * `FOMENT_LIBPATH` environment variable added
  * 64 bit version on Windows
  * `cond-expand` works in programs
  * `environment` procedure added
  * unicode conversion routines rewritten; convertutf.c is no longer used
  * Linux version: 32 and 64 bit
  * SRFI 112: Environment Inquiry is fully supported
  * SRFI 111: Boxes is fully supported
  * `(scheme lazy)` is no longer delayed

## 0.3 ##

12 January 2014

  * `syntax-rules` now treat `...` and `_` as identifiers rather than symbols
  * FreeBSD version
  * `set!-values`
  * read datum labels and datum references
  * `equal?` works with circular data structures
  * `current-second` fixed to return an inexact number
  * numbers fully supported
  * `(scheme r5rs)` library added
  * `scheme-report-environment` and `null-environment` added
  * `(aka` _library\_name_`)` to declare an additional name for a library

## 0.4 ##

29 March 2014

  * library extensions: .scm and .sld
  * use command line argument `-X` to specify additional library extensions
  * gc: wait on conditions in a loop and test predicate correctly
  * `exit` supported
  * `exit-thread` and `emergency-exit-thread`
  * ctrl-c handling
  * editline with history
  * `console-port?`, `set-console-input-editline!`, and `set-console-input-echo!`
  * `with-notify-handler` and `set-ctrl-c-notify!`
  * `char-ready?` and `byte-ready?` work correctly; all of R7RS is now supported
  * sockets are supported
  * SRFI 106: Basic socket interface is fully supported
  * use `coding:` to specify the encoding of a text file
  * threads blocked on read, recv-socket, accept-socket, and connect-socket will no longer block garbage collection

## 0.5 ##

  * on unix, using foment for scripts works: #!/usr/local/bin/foment as the first line of a program
  * empty `cond-expand` works
  * include, include-ci, and include-library-declarations are relative to including file
  * number->string returns lowercase rather than uppercase
  * SRFI 60: Integers as Bits is fully supported.
  * [FileSystemAPI](FileSystemAPI.md)
  * `port-has-port-position?`, `port-position`, `port-has-set-port-position!?', and `set-port-position!`
  * hash trees supported: [MiscellaneousInternals](MiscellaneousInternals.md)
  * hash maps supported: [HashContainerAPI](HashContainerAPI.md)
  * hashtables changed to hash-maps
  * SRFI 114: Comparators is fully supported.