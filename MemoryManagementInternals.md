# Memory Management Internals #

The garbage collector uses three generations: zero, one, and mature. Zero and one are young
generations. They are always collected together using a copying collector. The mature generation
is collected using a mark and sweep collector. A partial collection collects the young generations
only. A full collection collects all three generations.

## Guardians ##

The implementation of guardians closely follows this paper:

R. Kent Dybvig, Carl Bruggeman, and David Eby. Guardians in a generation-based garbage collector.
In _Proceedings of the SIGPLAN '93 Conference on Programming Language Design and Implementation_,
207-216, June 1993. http://www.cs.indiana.edu/~dyb/pubs/guardians-pldi93.pdf

## Trackers ##

The implemenation of trackers is inspired by guardians and this paper:

Abdulaziz Ghuloum and R. Kent Dybvig. Generation-friendly eq hash tables.
In _2007 Workshop on Scheme and Functional Programming_, 27-35, 2007
http://www.schemeworkshop.org/2007/procPaper3.pdf

Trackers are like guardians, but rather than watching for objects to become inaccessible, they
watch for objects to move during a copying collection.

procedure: `(install-tracker` _obj_ _ret_ _tconc_`)`

`install-tracker` is used by `make-tracker` to tell the collector to start tracking _obj_. When
_obj_ moves, the collector will put _ret_ onto the end of _tconc_. Tracking will continue as long
as _obj_, _ret_, and _tconc_ are all accessible and _obj_ has not yet moved.
