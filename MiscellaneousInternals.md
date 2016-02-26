# Miscellaneous Internals #

These procedures are in addition to the procedures specified by R7RS.
`(import (foment base))` to use these procedures.

## Hash Trees ##

The implementation of hash trees follows this paper:

Phil Bagwell (2000). Ideal Hash Trees (Report). Infoscience Department,
�cole Polytechnique F�d�rale de Lausanne.
http://infoscience.epfl.ch/record/64398/files/idealhashtrees.pdf

Hash tree indexes must be between `0` inclusive and `#x7ffffff`.

procedure: `(make-hash-tree)`

Return a new hash tree.

procedure: `(hash-tree?` _obj_`)`

Return `#t` if _obj_ is a hash tree and `#f` otherwise.

procedure: `(hash-tree-ref` _hash-tree_ _index_ _not-found_`)`

If a value has been set at _index_ in _hash-tree_ return it; otherwise return _not-found_.

procedure: `(hash-tree-set!` _hash-tree_ _index_ _value_`)`

Set the value of _index_ in _hash-tree_ to be _value_. A new hash tree may be returned.

procedure: `(hash-tree-delete` _hash-tree_ _index_`)`

Remove the value for _index_ in _hash-tree_. A new hash tree may be returned.

procedure: `(hash-tree-buckets` _hash-tree_`)`

Return the buckets of _hash-tree_ as a vector. There will be as many as 64 buckets. Each bucket
will either be another hash tree or a box containing a value in the tree.

procedure: `(hash-tree-bitmap` _hash-tree_`)`

Return the bitmap which indicates which slots have values.