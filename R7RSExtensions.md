# R7RS Extensions #

procedure: `(interaction-environment` _import-set_ _..._ `)`

The procedure `interaction-environment` can be passed one or more
_import-set\_s. A mutable environment will be returned containing the bindings specified by
the_import-set\_s.

procedure: `(eval` _expr_ _env_ `)`
<br> procedure: <code>(load</code> <i>filename</i> <code>)</code>
<br> procedure: <code>(load</code> <i>filename</i> <i>env</i> <code>)</code>

These procedures will recognize <code>define-library</code> forms and define the specified library. This<br>
means that you can define libraries in interactive sessions and load them using <code>load</code> and<br>
the command line option <code>-l</code>.<br>
<br>
library declaration: <code>(aka</code> <i>library_name</i><code>)</code>

Declare <i>library_name</i> as an additional name for a library.<br>
