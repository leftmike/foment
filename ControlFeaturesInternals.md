# Control Features Internals #

## Continuation Marks ##

The implementation of continuation marks is based on John Clements' dissertation:
_Portable and high-level access to the stack with Continuation Marks_.

The syntax `with-continuation-mark` expands directly into a call to the procedure
`%mark-continuation`.

```
(define-syntax with-continuation-mark
    (syntax-rules ()
        ((_ key val expr) (%mark-continuation key val (lambda () expr)))))
```

procedure: `(%mark-continuation` _who_ _key_ _val_ _thunk_`)`

`%mark-continuation` looks at the front of the continuation to see what procedure it would
return to, if it were to return. If that procedure is `%mark-continuation` and has the same
_who_ then
an existing mark with the same key can be updated or a new mark can be added, and a tail call
can be made to _thunk_. Otherwise, a new mark is placed on the front of the continuation
and a normal call is make to _thunk_. Note that updating an existing mark only applies for marks
at the front of the continuation.

```
(define dynamic-stack '())

(define (update-mark key val)
    (let ((p (assq key (car dynamic-stack))))
        (if p
            (set-cdr! p val)
            (set-car! dynamic-stack (cons (cons key val) (car dynamic-stack))))))

(define (%mark-continuation key val thunk)
    (if (eq? (continuation-front-procedure) %mark-continuation)
        (begin
            (update-mark key val)
            (thunk))
        (begin
            (set! dynamic-stack (cons (list (cons key val)) dynamic-stack))
            (let-values ((v (thunk)))
                (set! dynamic-stack (cdr dynamic-stack))
                (apply values v))))))
```

The procedure `continuation-front-procedure` returns the procedure on the front of the
continuation.

```
(define (a)
    (define (b)
        (continuation-front-procedure))
    (let ((p (b)))
        (eq? (b) a)))
=> #t
```

In the actual implementation, `%mark-continuation` is a primitive. Frames on the dynamic stack
are created by calls to `%mark-continuation`. Each frame is an object containing a list of marks
and a location in the continuation.

procedure: `(%dynamic-stack)`
<br> procedure: <code>(%dynamic-stack</code> <i>stack</i><code>)</code>

Get and set the dynamic stack.<br>
<br>
procedure: <code>(%dynamic-marks _dynamic_ </code>)`<br>
<br>
Get the list of marks contained in the <i>dynamic</i> frame.<br>
<br>
<pre><code>(let ((ml (with-continuation-mark 'key 10<br>
        (%dynamic-marks (car (%dynamic-stack))))))<br>
    ml)<br>
=&gt; ((key . 10))<br>
</code></pre>

procedure: <code>(%find-mark</code> <i>key</i> <i>default</i><code>)</code>

Find the first mark with <i>key</i> and return its values; otherwise return <i>default</i>.<br>
<br>
<h2>Continuations</h2>

The implementation of continuations is inspired by these two papers:<br>
<br>
Matthew Flatt, Gang Yu, Robert Bruce Findler, and Matthias Felleisen.<br>
<i>Adding Delimited and Composable Control to a Production Programming Environment</i>
<a href='http://www.cs.utah.edu/plt/publications/icfp07-fyff.pdf'>http://www.cs.utah.edu/plt/publications/icfp07-fyff.pdf</a>

Marc Feeley. <i>A Better API for First-Class Continuations</i>
<a href='http://www.schemeworkshop.org/2001/feeley.pdf'>http://www.schemeworkshop.org/2001/feeley.pdf</a>

Note that delimited and composable continuations are not yet supported.<br>
<br>
procedure: <code>(%capture-continuation</code> <i>proc</i><code>)</code>

Capture the current continuation as an object and pass it to <i>proc</i>.<br>
<br>
procedure: <code>(%call-continuation</code> <i>cont</i> <i>thunk</i><code>)</code>

Replace the current continuation with <i>cont</i> and call <i>thunk</i>. The continuation of <i>thunk</i>
is <i>cont</i>.<br>
<br>
procedure: <code>(%abort-dynamic</code> <i>dynamic</i> <i>thunk</i><code>)</code>

Unwind the current continuation to location of <i>dynamic</i> and call <i>thunk</i>.<br>
