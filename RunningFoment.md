# Running Foment #

Foment works on Windows, Linux, and FreeBSD.

If you are using Windows, you will need at least Windows Vista
for it to work. To see more Unicode characters, change the font of the console window from a
raster font to a TrueType font.

Foment can be used in two modes: program mode and interactive mode. In program mode, Foment will
compile and run a program. In interactive mode, Foment will run an interactive session (REPL).

## Program Mode ##

`foment` _option_ _..._ _program_ _arg_ _..._

Compile and run the program in the file named _program_. There may be zero or more options and
zero or more arguments.

Everything from `foment` to _program_ will be concatenated into a single string; this string
will be the `(car (command-line))`. Everything after _program_ will be the
`(cdr (command-line))`, with each _arg_ being a seperate string.

### Options ###

`-A` _directory_

Append _directory_ to the library search path.

`-I` _directory_

Prepend _directory_ to the library search path.

`-X` _ext_

Add _ext_ as a possible extension for the filename of a library. See [UsingLibraries](UsingLibraries.md).

`-no-inline-procedures`

The compiler is not allowed to inline some procedures.

`-no-inline-imports`

The compiler is not allowed to inline imports that it knows are constant.

## Interactive Mode ##

`foment` _option_ _..._ _flag_ ... _arg_ ...
<br><code>foment</code> <i>option</i> <i>...</i> <i>flag</i> ...<br>
<br>
Run an interactive session. Any options must be specified before any flags.<br>
If one or more <i>args</i> are specified, then at least<br>
one flag must be specified; otherwise, Foment will think the first <i>arg</i> is a program.<br>
<br>
<h3>Flags</h3>

<code>-i</code>

Run an interactive session.<br>
<br>
<code>-e</code> <i>expr</i>

Evalute <i>expr</i>.<br>
<br>
<code>-p</code> <i>expr</i>

Print the result of evaluating <i>expr</i>.<br>
<br>
<code>-l</code> <i>filename</i>

Load <i>filename</i>.<br>
<br>
<h1>Using Foment</h1>

As an extension to R7RS, <code>eval</code> recognizes (define-library ...) and loads it as a library. This<br>
works in an interactive session and when a file is loaded via <code>load</code> or <code>-l</code> on the command line.<br>
As a result, you can start developing your program and libraries all in one file and run it<br>
via <code>-l</code> on the command line.<br>
<br>
<h1>Interactive Session</h1>

The REPL uses editline which provides interactive editing of lines of input.<br>
<br>
<code>ctrl-a</code> : beginning-of-line<br>
<br><code>ctrl-b</code> : backward-char<br>
<br><code>ctrl-d</code> : delete-char<br>
<br><code>ctrl-e</code> : end-of-line<br>
<br><code>ctrl-f</code> : forward-char<br>
<br><code>ctrl-h</code> : delete-backward-char<br>
<br><code>ctrl-k</code> : kill-line<br>
<br><code>alt-b</code> : backward-word<br>
<br><code>alt-d</code> : kill-word<br>
<br><code>alt-f</code> : forward-word<br>
<br><code>backspace</code> : delete-backward-char<br>
<br><code>left-arrow</code> : backward-char<br>
<br><code>right-arrow</code> : forward-char