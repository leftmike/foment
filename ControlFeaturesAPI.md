# Control Features API #

`(import (foment base))` to use these procedures.

## Continuations ##

procedure: `(call-with-continuation-prompt` _proc_ _prompt-tag_ _handler_ _arg_ _..._`)`

Mark the current continuation with _prompt\_tag_ and apply _proc_ to _arg_ _..._.

Within the dynamic extent of _proc_ if `abort-current-continuation` is called with _prompt\_tag_
then the continuation is reset to the closest _prompt\_tag_ mark, _handler_ is applied
to the _val_ _..._ passed to `abort-current-continuation`. And the result of
`call-with-continuation-prompt` is the result of _handler_.

Otherwise,  _proc_ returns normally and the result of `call-with-continuation-prompt` is the
result of _proc_.

If _prompt\_tag_ is the default prompt tag, then _handler_ must be `default-prompt-handler`.

procedure: `(abort-current-continuation` _prompt-tag_ _val_ _..._`)`

Reset the continuation to the nearest _prompt\_tag_ mark. Apply _handler_
(from `call-with-continuation-prompt`) to the _val_ _..._.

If aborting to the default prompt tag, pass a thunk as the _val_ _..._.

procedure: `(default-prompt-tag)`
<br>procedure: <code>(default-continuation-prompt-tag)</code>

Return the default prompt tag. The start of the continuation for every thread has a mark for the<br>
default prompt tag.<br>
<br>
procedure: default-prompt-handler<br>
<br>
<code>default-prompt-handler</code> reinstalls the mark for the default prompt tag and calls the thunk.<br>
<br>
<pre><code>(define (default-prompt-handler proc)<br>
    (call-with-continuation-prompt proc (default-prompt-tag) default-prompt-handler))<br>
</code></pre>

<h2>Continuation Marks</h2>

A continuation mark consists of a key and a value.<br>
<br>
syntax: <code>(with-continuation-mark</code> <i>key</i> <i>val</i> <i>expr</i><code>)</code>

The front of the continuation is marked with <i>key</i> and <i>val</i>. If the front of the continuation<br>
already contains a mark with <i>key</i> then it is updated to have <i>val</i> as its value. Otherwise, a<br>
new mark is added for <i>key</i> and <i>val</i>. If the front of the continuation already has any marks,<br>
irrespective of what keys those marks contain, then <i>expr</i> is called in the tail position.<br>
<br>
<pre><code>(with-continuation-mark 'key 1<br>
    (with-continuation-mark 'key 2<br>
        (with-continuation-mark 'key 3<br>
            (current-continuation-marks))))<br>
=&gt; ((key . 3))<br>
<br>
(define (count n m)<br>
    (if (= n m)<br>
        (current-continuation-marks)<br>
        (let ((r (with-continuation-mark 'key n (count (+ n 1) m))))<br>
            r)))<br>
(count 0 4) =&gt; (((key . 3)) ((key . 2)) ((key . 1)) ((key . 0)))<br>
</code></pre>

procedure: <code>(current-continuation-marks)</code>

Return all marks for the current continuation as a list of lists of marks.<br>
Each mark consists of a pair: <code>(</code><i>key</i> . <i>value</i><code>)</code>. The lists of marks are ordered from<br>
front to back (inner to outer).<br>
<br>
<h2>Notify and Control-C</h2>

procedure: <code>(with-notify-handler</code> <i>handler</i> <i>thunk</i><code>)</code>

Execute <i>thunk</i>. If any notifications happen in the dynamic extent of <i>thunk</i> then <i>handler</i> will<br>
be called.<br>
<br>
procedure: <code>(set-ctrl-c-notify! _disposition_</code>)`<br>
<br>
<i>disposition</i> must be <code>exit</code>, <code>ignore</code>, or <code>broadcast</code>. If <i>disposition</i> is <code>exit</code>, then the<br>
program will exit when ctrl-c is pressed. If <i>disposition</i> is <code>ignore</code>, then ctrl-c will be<br>
ignored. If <i>disposition</i> is <code>broadcast</code>, then call every notify <i>handler</i> for every thread<br>
with <code>sigint</code> as the argument.