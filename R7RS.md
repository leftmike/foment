# R7RS #

All of [R7RS](http://trac.sacrideo.us/wg/raw-attachment/wiki/WikiStart/r7rs.pdf) is supported.


---


procedure: `(exit)`
<br>procedure: <code>(exit</code> <i>obj</i><code>)</code>

Runs all outstanding <code>dynamic-wind</code> <i>after</i> procedures for the current thread and terminates<br>
the running program. See <code>exit-thread</code> to exit the current thread without terminating the running<br>
program.