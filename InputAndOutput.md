# Input and Output #

## Sockets ##

See [Sockets](Sockets.md).

## Ports ##

`(import (foment base))` to use these procedures.

procedure: `(make-buffered-port` _binary\_port_`)`

Make a buffered binary port from _binary\_port_; if _binary\_port_ is already buffered, then it is
returned. If the
_binary-port_ is an input port, then the new buffered binary port will be an input port.
Likewise, if the
_binary-port_ is an output port or an input and output port then the new textual port will be the
same.

procedure: `(make-ascii-port` _binary-port`)`_

Make a textual port which reads and/or writes characters in the ASCII encoding to the
_binary-port_. If the
_binary-port_ is an input port, then the new textual port will be an input port. Likewise, if the
_binary-port_ is an output port or an input and output port then the new textual port will be the
same.

procedure: `(make-latin1-port` _binary-port`)`_

Make a textual port which reads and/or writes characters in the Latin-1 encoding to the
_binary-port_. If the
_binary-port_ is an input port, then the new textual port will be an input port. Likewise, if the
_binary-port_ is an output port or an input and output port then the new textual port will be the
same.

procedure: `(make-utf8-port` _binary-port`)`_

Make a textual port which reads and/or writes characters in the UTF-8 encoding to the
_binary-port_. If the
_binary-port_ is an input port, then the new textual port will be an input port. Likewise, if the
_binary-port_ is an output port or an input and output port then the new textual port will be the
same.

procedure: `(make-utf16-port` _binary-port_`)`

Make a textual port which reads and/or writes characters in the UTF-16 encoding to the
_binary-port_. If the
_binary-port_ is an input port, then the new textual port will be an input port. Likewise, if the
_binary-port_ is an output port or an input and output port then the new textual port will be the
same.

procedure: `(make-encoded-port` _binary\_port_`)`

Automatically detect the encoding of _binary\_port_ and make a textual port which reads and/or
writes characters in the correct encoding to the _binary\_port_.

The UTF-16 byte order mark is automatically detected. Otherwise, the string `coding:` is searched
for in the first 1024 bytes of the _binary\_port_. Following `coding:` should be the name of a
character encoding. Currently, `ascii`, `utf-8`, `latin-1`, and `iso-8859-1` are supported. If the
encoding can not be automatically determined, `ascii` is used.

procedure: `(file-encoding)`
<br>procedure: <code>(file-encoding</code> <i>proc</i><code>)</code>
<br>procedure: <code>(</code><i>proc</i> <i>binary-port</i><code>)</code>

<code>file-encoding</code> is a parameter. <i>proc</i> is called by <code>open-input-file</code> and <code>open-output-file</code> to<br>
return a textual port appropriate for the encoding of a binary port.<br>
<br>
The following example will open a file encoded using UTF-16.<br>
<br>
<pre><code>(parameterize ((file-encoding make-utf8-port))<br>
    (open-input-file "textfile.utf8"))<br>
</code></pre>

procedure: <code>(want-identifiers</code> <i>port</i> <i>flag</i><code>)</code>

Textual input ports can read symbols or identifiers. Identifiers contain location information<br>
which improve error messages and stack traces. If <i>flag</i> is <code>#f</code> then symbols will be read<br>
from <i>port</i> by read. Otherwise, identifiers will be read.<br>
<br>
procedure: <code>(port-has-port-position?</code> <i>port</i><code>)</code>

Returns <code>#t</code> if <i>port</i> supports the <code>port-position</code> operation.<br>
<br>
procedure: <code>(port-position</code> <i>port</i><code>)</code>

Returns the position of the next read or write operation on <i>port</i> as a non-negative exact integer.<br>
<br>
procedure: <code>(port-has-set-port-position!? _port_</code>)`<br>
<br>
Returns <code>#t</code> if <i>port</i> supports the <code>set-port-position!</code> operation.<br>
<br>
procedure: <code>(set-port-position! _port_ _position_</code>)`<br>
procedure: <code>(set-port-position! _port_ _position_ _whence_</code>)`<br>
<br>
Sets the position of the next read or write operation on <i>port</i>. The <i>position</i> must be an<br>
exact integer taken as relative to <i>whence</i> which may be <code>begin</code>, <code>current</code>, or <code>end</code>.<br>
If <i>whence</i> is not specified, then <code>begin</code> is used.<br>
<br>
<h2>Console</h2>

<code>(import (foment base))</code> to use these procedures.<br>
<br>
procedure: <code>(console-port?</code> <i>obj</i><code>)</code>

Return <code>#t</code> is <i>obj</i> is a console port and <code>#f</code> otherwise.<br>
<br>
procedure: <code>(set-console-input-editline!</code> <i>port</i> <i>flag</i><code>)</code>

<i>port</i> must be a console inport port. <i>flag</i> must be <code>#t</code> or <code>#f</code>. Turn on or off editline mode<br>
for <i>port</i>.<br>
<br>
procedure: <code>(set-console-input-echo!</code> <i>port</i> <i>flag</i><code>)</code>

<i>port</i> must be a console inport port. <i>flag</i> must be <code>#t</code> or <code>#f</code>. Turn on or off echo mode<br>
for <i>port</i>.