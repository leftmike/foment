# Miscellaneous API #

These procedures are in addition to the procedures specified by R7RS.
`(import (foment base))` to use these procedures.

## Assignments ##

syntax: `(set!-values` `(`_var_ _..._`)` _expr_`)`

_expr_ is evaluated and must return as many values as there are _var\_s. Each_var_is set to
the corresponding value returned by_expr_._

## Syntax ##

procedure: `(syntax` _expr_`)`

Return the expansion of _expr_. `syntax` and `unsyntax` are best used together.

procedure: `(unsyntax` _expr_`)`

Return _expr_ converted to datum.

```
(unsyntax (syntax '(when (not (null? lst)) (write "not null"))))
=> (#<syntax: if> (not (null? lst)) (#<syntax: begin> (write "not null")))
```

## Exceptions ##

Error objects contains type, who, message, and irritants fields. Type is a symbol, typically one of
`error`, `assertion-violation`, `implementation-restriction`, `lexical-violation`,
`syntax-violation`, or `undefined-violation`. Who is a symbol, typically which procedure raised
the exception. Message is a string explaining the error to be displayed to the user. Irritants
is a list of additional context for the error.

procedure: `(error-object-type` _error-object_`)`

Return the `type` field of _error\_object_.

procedure: `(error-object-who` _error-object_`)`

Return the `who` field of _error-object_.

procedure: `(error-object-kind` _error-object_`)`

Return the `kind` field of _error-object_.

procedure: `(full-error` _type_ _who_ _kind_ _message_ _obj_ _..._`)`

Raise an exception by calling raise on a new error object. The procedure `error` could be defined
in terms of `full-error` as follows.

```
(define (error message . objs)
    (apply full-error 'assertion-violation 'error #f message objs))
```

## Libraries ##

procedure: `(loaded-libraries)`

Return the list of loaded libraries.

procedure: `(library-path)`

Return the library path. The command line options, `-A` and `-I`, and the environment variable
`FOMENT_LIBPATH` can be used to add to the library path.

## Miscellaneous ##

procedure: `(random` _k_`)`

Return a random number between 0 and _k_ `- 1`.
