# Hash Container API #

`(import (foment base))` to use these procedures.

## Hash Maps ##

procedure: `(make-hash-map)`
<br>procedure: <code>(make-hash-map</code> <i>comparator</i><code>)</code>

Return a new hash map. Use <i>comparator</i> as the comparator for the new hash map if specified;<br>
otherwise, use <code>default-comparator</code>.<br>
<br>
procedure: <code>(alist-&gt;hash-map</code> <i>alist</i><code>)</code>
<br>procedure: <code>(alist-&gt;hash-map</code> <i>alist</i> <i>comparator</i><code>)</code>

Takes an association list, <i>alist</i>, and returns a new hash map which maps the <code>car</code> of every<br>
element in <i>alist</i> to the <code>cdr</code> of corresponding elements in <i>alist</i>. Use <i>comparator</i> as the<br>
comparator for the new hash map if specified; otherwise, use <code>default-comparator</code>.<br>
If some key occurs multiple times in <i>alist</i>, the value in the first association will take<br>
precedence over later ones.<br>
<br>
procedure: <code>(hash-map? _obj_</code>)`<br>
<br>
Return <code>#t</code> if <i>obj</i> is a hash map and <code>#f</code> otherwise.<br>
<br>
procedure: <code>(hash-map-size</code> <i>hash-map</i><code>)</code>
<br>procedure: <code>(hash-map-size</code> <i>eq-hash-map</i><code>)</code>

Return the number of entries in <i>hash-map</i> or <i>eq-hash-map</i>.<br>
<br>
procedure: <code>(hash-map-ref</code> <i>hash-map</i> <i>key</i> <i>default</i><code>)</code>

Return the value associated with <i>key</i> in <i>hash-map</i>; otherwise return <i>default</i>.<br>
<br>
procedure: <code>(hash-map-set!</code> <i>hash-map</i> <i>key</i> <i>value</i><code>)</code>

Set the value associated with <i>key</i> in <i>hash-map</i> to be <i>value</i>. If there already exists a value<br>
associated with <i>key</i> it is replaced.<br>
<br>
procedure: <code>(hash-map-delete!</code> <i>hash-map</i> <i>key</i><code>)</code>
If <i>key</i> exists in <i>hash-map</i> remove <i>key</i> and its associated value.<br>
<br>
procedure: <code>(hash-map-contains?</code> <i>hash-map</i> <i>key</i><code>)</code>

Return <code>#t</code> if <i>key</i> exists in <i>hash-map</i> and <code>#f</code> otherwise.<br>
<br>
procedure: <code>(hash-map-update!</code> <i>hash-map</i> <i>key</i> <i>proc</i> <i>default</i><code>)</code>
<br>procedure: <code>(</code><i>proc</i> <i>value</i><code>)</code>
Applies <i>proc</i> to the value in <i>hash-map</i> associated with <i>key</i>, or to <i>default</i> if <i>hash-map</i>
does not contain an association for <i>key</i>. The <i>hash-map</i> is then changed to associate <i>key</i>
with the value returned by <i>proc</i>.<br>
<br>
procedure: <code>(hash-map-copy</code> <i>hash-map</i><code>)</code>
Return a copy of <i>hash-map</i>.<br>
<br>
procedure: <code>(hash-map-keys</code> <i>hash-map</i><code>)</code>
<br>procedure: <code>(hash-map-keys</code> <i>eq-hash-map</i><code>)</code>
Returns a vector of all keys in <i>hash-map</i> or <i>eq-hash-map</i>. The order of the vector is<br>
unspecified.<br>
<br>
procedure: <code>(hash-map-entries</code> <i>hash-map</i><code>)</code>
<br>procedure: <code>(hash-map-entries</code> <i>eq-hash-map</i><code>)</code>
Returns two values, a vector of the keys in <i>hash-map</i> or <i>eq-hash_map</i>, and<br>
a vector of the corresponding values.<br>
<br>
procedure: <code>(hash-map-comparator</code> <i>hash-map</i><code>)</code>
Returns the comparator used by <i>hash-map</i>.<br>
<br>
procedure: <code>(hash-map-walk</code> <i>hash-map</i> <i>proc</i><code>)</code>
<br>procedure: <code>(hash-map-walk</code> <i>eq-hash-map</i> <i>proc</i><code>)</code>
<br>procedure: <code>(</code><i>proc</i> <i>key</i> <i>value</i><code>)</code>
Call <i>proc</i> for each association in <i>hash-map</i> or <i>eq-hash_map</i>, giving the key of the<br>
association as <i>key</i> and the value of the association as <i>value</i>. The results of <i>proc</i> are<br>
discarded. The order in which <i>proc</i> is called for the different associations is unspecified.<br>
<br>
procedure: <code>(hash-map-fold</code> <i>hash-map</i> <i>func</i> <i>init</i><code>)</code>
<br>procedure: <code>(hash-map-fold</code> <i>eq-hash-map</i> <i>func</i> <i>init</i><code>)</code>
<br>procedure: <code>(</code><i>func</i> <i>key</i> <i>value</i> <i>accum</i><code>)</code>

Call <i>func</i> for every association in <i>hash-map</i> or <i>eq-hash-map</i> with three arguments: the<br>
key of the association, <i>key</i>, the value of the association, <i>value</i>, and an accumulated<br>
value, <i>accum</i>. For the first invocation of <i>func</i>, <i>accum</i> is <i>init</i>. For subsequent<br>
invocations of <i>func</i>, <i>accum</i> is the return value of the previous invocation of <i>func</i>.<br>
The value returned by <code>hash-map-fold</code> is the return value of the last invocation of <i>func</i>.<br>
The order in which <i>func</i> is called for different associations is unspecified.<br>
<br>
procedure: <code>(hash-map-&gt;alist</code> <i>hash-map</i><code>)</code>
<br>procedure: <code>(hash-map-&gt;alist</code> <i>eq-hash-map</i><code>)</code>

Return an association list such that the <code>car</code> of each element in alist is a key in <i>hash-map</i>
or <i>eq-hash-map</i> and the corresponding <code>cdr</code> of each element in alist is the value associated<br>
to the key in <i>hash-map</i> or <i>eq-hash-map</i>. The order of the elements is unspecified.<br>
<br>
<h2>Eq Hash Maps</h2>

Eq hash maps automatically use trackers (see <a href='MemoryManagementAPI.md'>MemoryManagementAPI</a>) to update when the garbage<br>
collector moves objects.<br>
<br>
procedure: <code>(make-eq-hash-map)</code>

Return a new hash map that uses <code>eq</code> to compare keys.<br>
<br>
procedure: <code>(alist-&gt;eq-hash-map</code> <i>alist</i><code>)</code>

Takes an association list, <i>alist</i>, and returns a new eq hash map which maps the <code>car</code> of every<br>
element in <i>alist</i> to the <code>cdr</code> of corresponding elements in <i>alist</i>.<br>
If some key occurs multiple times in <i>alist</i>, the value in the first association will take<br>
precedence over later ones.<br>
<br>
procedure: <code>(eq-hash-map?</code> <i>obj</i><code>)</code>

Return <code>#t</code> if <i>obj</i> is a hash map that uses <code>eq</code> to compare keys and <code>#f</code> otherwise.<br>
<br>
procedure: <code>(eq-hash-map-ref</code> <i>eq-hash-map</i> <i>key</i> <i>default</i><code>)</code>

Lookup <i>key</i> in <i>eq-hash-map</i> and return the <i>key</i>'s associated value. If the <i>key</i> is not found,<br>
then return <i>default</i>.<br>
<br>
procedure: <code>(eq-hash-map-set!</code> <i>eq-hash-map</i> <i>key</i> <i>value</i><code>)</code>

Set the value associated with <i>key</i> to <i>value</i>. If <i>key</i> is not in the <i>eq-hash-map</i> it will be<br>
added.<br>
<br>
procedure: <code>(eq-hash-map-delete</code> <i>eq-hash-map</i> <i>key</i><code>)</code>

Delete <i>key</i> from <i>eq-hash-map</i>.<br>
<br>
<h2>Eq Hash Sets</h2>

Eq hash sets automatically use trackers (see <a href='MemoryManagementAPI.md'>MemoryManagementAPI</a>) to update when the garbage<br>
collector moves objects.<br>
<br>
procedure: <code>(make-eq-hash-set)</code>

Return a new hash set that uses <code>eq</code> to compare elements.<br>
<br>
procedure: <code>(eq-hash-set?</code> <i>obj</i><code>)</code>

Return <code>#t</code> if <i>obj</i> is a hash set that uses <code>eq</code> to compare elements and <code>#f</code> otherwise.<br>
<br>
procedure: <code>(eq-hash-set-contains</code> <i>eq-hash-set</i> <i>element</i><code>)</code>

Returns <code>#t</code> if <i>eq-hash-set</i> contains <i>element</i> and <code>#f</code> otherwise.<br>
<br>
procedure: <code>(eq-hash-set-adjoint!</code> <i>eq-hash-set</i> <i>element</i> <i>...</i><code>)</code>

Add each <i>element</i> to <i>eq-hash-set</i> if it is not already there.<br>
<br>
procedure: <code>(eq-hash-set-delete!</code> <i>eq-hash-set</i> <i>element</i><code>)</code>

If <i>element</i> is in <i>eq-hash-set</i>, delete it from the set.