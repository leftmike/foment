# Memory Management API #

`(import (foment base))` to use these procedures.

## Guardians ##

procedure: `(make-guardian)`
<br>procedure: <code>(</code><i>guardian</i><code>)</code>
<br>procedure: <code>(</code><i>guardian</i> <i>obj</i><code>)</code>

A new guardian will be returned. A guardian is a procedure which encapsulates a group of objects<br>
to be protected from being collected by the garbage collector and a group of objects saved from<br>
collection. Initially, both groups of objects are empty.<br>
<br>
To add an object to the group of objects being protected, call the guardian with the object as the<br>
only argument.<br>
<br>
<pre><code>(define g (make-guardian))<br>
(define obj (cons 'a 'b))<br>
(g obj)<br>
</code></pre>

When a protected object is inaccessible, other than from a guardian, and could be collected, it<br>
is moved to the saved group of any guardians protecting it, rather than being collected. To get<br>
the objects from the saved group, the guardian is called without any arguments. The objects from<br>
the saved group are returned one at a time until the saved group is empty in which case <code>#f</code> is<br>
returned.<br>
<br>
<pre><code>(g) =&gt; #f<br>
(set! obj 123)<br>
(collect)<br>
(g) =&gt; (a . b)<br>
(g) =&gt; #f<br>
</code></pre>

An object may be protected by more than on guardians and more than once by the same guardian. When<br>
an object becomes inaccessible it will be moved into saved groups the same number of times that<br>
it was protected. Finally, guardians may be protected by guardians.<br>
<br>
<h2>Trackers</h2>

procedure: <code>(make-tracker)</code>
<br>procedure: <code>(</code><i>tracker</i><code>)</code>
<br>procedure: <code>(</code><i>tracker</i> <i>obj</i><code>)</code>
<br>procedure: <code>(</code><i>tracker</i> <i>obj</i> <i>ret</i><code>)</code>

A new tracker will be returned. A tracker is a procedure which encapsulates a group of objects to<br>
be tracked for when the garbage collector moves them during a copying collection and a group of<br>
objects which have been moved. Initially, both groups of objects are empty.<br>
<br>
To add an object to the group of objects being tracked, call the tracker<br>
with the object as the only argument. Or call the tracker with the object to be tracked and an<br>
object to be returned when the tracked object moves.<br>
<br>
<pre><code>(define t (make-tracker))<br>
(define obj (cons 'a 'b))<br>
(define ret (cons (cons 'w 'x) (cons 'y 'z)))<br>
(t obj)<br>
(t (car ret) ret)<br>
</code></pre>

When a tracked object has moved in memory, it is put into the moved group of any trackers<br>
tracking it. To get the objects from the moved group, the tracker is called without any arguments.<br>
The objects from the moved group are returned one at a time until the moved group is empty in<br>
which case <code>#f</code> is returned.<br>
<br>
<pre><code>(t) =&gt; #f<br>
(collect)<br>
(t) =&gt; (a . b)<br>
(t) =&gt; ((w . x) y . z)<br>
(t) =&gt; #f<br>
(collect)<br>
(t) =&gt; #f<br>
</code></pre>

To continue tracking an object once it has been placed in the moved group, it will need to<br>
registered with the tracker again.<br>
<br>
Tracking the movement of objects is important for <code>eq-hash</code> which uses an object's address as<br>
it's hash. When the object is<br>
moved during a copying collection, it is the same object, but it has a different hash because it<br>
is now at a different address. Uses of <code>eq-hash</code> will need to used a tracker to follow address<br>
changes of objects.<br>
<br>
<h2>Garbage Collection</h2>

procedure: <code>(collect)</code>
<br> procedure: <code>(collect</code> <i>full</i><code>)</code>

If <i>full</i> is not specified or <code>#f</code> then a partial collection is performed. Otherwise, a full<br>
collection is performed.<br>
<br>
<hr />

procedure: <code>(partial-per-full)</code>
<br> procedure: <code>(partial-per-full</code> <i>val</i><code>)</code>

<code>partial-per-full</code> is the number of partial collections performed for every full collection.<br>
If <i>val</i> is specified, it specifies a new value for <code>partial-per-full</code>.<br>
<i>val</i> must be a non-negative fixnum. A value of zero means that every<br>
collection is a full collection. The current value of <code>partial-per-full</code> is returned.<br>
<br>
<hr />

procedure: <code>(trigger-bytes)</code>
<br> procedure: <code>(trigger-bytes</code> <i>val</i><code>)</code>

<code>trigger-bytes</code> is the number of bytes allocated since the last collection before<br>
triggering another collection.<br>
If <i>val</i> is specified, it specifies a new value for <code>trigger-bytes</code>.<br>
<i>val</i> must be a non-negative fixnum. The current value of <code>trigger-bytes</code> is<br>
returned.<br>
<br>
Many more bytes than <code>trigger-bytes</code> may be allocated before a collection actually occurs.<br>
<br>
<hr />

procedure: <code>(trigger-objects)</code>
<br> procedure: <code>(trigger-objects</code> <i>val</i><code>)</code>

<code>trigger-objects</code> is the number of objects allocated since the last collection before<br>
triggering another collection.<br>
If <i>val</i> is specified, it specifies a new value for <code>trigger-objects</code>.<br>
<i>val</i> must be a non-negative fixnum. The current value of <code>trigger-objects</code> is<br>
returned.<br>
<br>
Many more objects than <code>trigger-objects</code> may be allocated before a collection actually occurs.<br>
<br>
<hr />

procedure: <code>(dump-gc)</code>

Dump information about memory to standard output.