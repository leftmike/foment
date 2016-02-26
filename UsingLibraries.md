# Using Libraries #

All procedures exported by Foment are in the `(foment base)` library.

Each program loads all imported libraries exactly once. If a library is loaded, it is
guaranteed to be loaded exactly once.

Libraries are contained in files. A file may contain more than one library:
`(define-library ...)`. When the file is loaded, all of the libraries in the file will be
loaded.

Library names get mapped to filenames two different ways: deep names and flat names.

Deep names use directories to seperate the components of the library name. The library name
`(`_name1_ _name2_ _..._ _namen_`)` gets mapped to the filename
_name1_`\`_name2_`\`_..._`\`_namen_`.`_ext_.

The use of deep names is strongly recommended for portable libraries.

Flat names use `-` to seperate the components of the library name. The library name
`(`_name1_ _name2_ _..._ _namen_`)` gets mapped to the filename
_name1_`-`_name2_`-`_..._`-`_namen_`.`_ext_.

By default, Foment uses `sld` and `scm` for _ext_. Additional extensions can be specified using
the `-X` command line option. See [RunningFoment](RunningFoment.md).

The library path is a list of one or more directories to search for libraries. To start with,
the library path contains the current directory and the directory containing foment.
The environment variable `FOMENT_LIBPATH` can contain a list of directories to put on the
front of the library path.
The command line switch `-I` adds a directory to the front of the list of directories and `-A` adds
a directory to the end.

For example, if a library named `(tiny example library)` is imported and the library path is
`("." "..\Tools" "d:\Scheme") then the filenames in the following order will be used to try
to load the library.

  1. `.\tiny-example-library.sld`
  1. `.\tiny\example\library.sld`
  1. `..\Tools\tiny-example-library.sld`
  1. `..\Tools\tiny\example\library.sld`
  1. `d:\Scheme\tiny-example-library.sld`
  1. `d:\Scheme\tiny\example\library.sld`

Warning: the libraries `(tiny example library)` and `(tiny-example-library)` both map to the
same flat name: `tiny-example-library.sld`.

The character encoding of the files containing libraries is automatically detected.
See `make-encoded-port` in [InputAndOutput](InputAndOutput.md).