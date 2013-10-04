(define-library (foment base)
    (import (foment bedrock))
    (export ;; (scheme base)
        import
        define-library
        *
        +
        -
;        ...
        /
        <
        <=
        =
        =>
        >
        >=
        abs
        and
        append
        apply
        assoc
        assq
;        assv
        begin
        binary-port?
        boolean=?
        boolean?
        bytevector
        bytevector-append
        bytevector-copy
        bytevector-copy!
        bytevector-length
        bytevector-u8-ref
        bytevector-u8-set!
        bytevector?
        caar
        cadr
        call-with-current-continuation
;        call-with-port
        call-with-values
        (rename call-with-current-continuation call/cc)
        car
        case
        cdar
        cddr
        cdr
;        ceiling
        char->integer
;        char-ready?
        char<=?
        char<?
        char=?
        char>=?
        char>?
        char?
        close-input-port
        close-output-port
        close-port
;        complex?
        cond
        cond-expand
        cons
        current-error-port
        current-input-port
        current-output-port
        define
        define-record-type
        define-syntax
        define-values
;        denominator
        do
        dynamic-wind
        else
;        eof-object
;        eof-object?
        eq?
        equal?
        eqv?
        error
        error-object-irritants
        error-object-message
        error-object?
        even?
;        exact
;        exact-integer-sqrt
        exact-integer?
;        exact?
        expt
        features
        file-error?
;        floor
;        floor-quotient
;        floor-remainder
;        floor/
;        flush-output-port
        for-each
;        gcd
        get-output-bytevector
        get-output-string
        guard
        if
        include
        include-ci
;        inexact
;        inexact?
        input-port-open?
        input-port?
        integer->char
;        integer?
        lambda
;        lcm
        length
        let
        let*
        let*-values
        let-syntax
        let-values
        letrec
        letrec*
        letrec-syntax
        list
        list->string
        list->vector
;        list-copy
        list-ref
;        list-set!
;        list-tail
;        list?
        make-bytevector
        make-list
        make-parameter
        make-string
        make-vector
        map
;        max
;        member
        memq
;        memv
;        min
;        modulo
        negative?
        newline
        not
        null?
        number->string
;        number?
;        numerator
        odd?
        open-input-bytevector
        open-input-string
        open-output-bytevector
        open-output-string
        or
        output-port-open?
        output-port?
        pair?
        parameterize
;        peek-char
;        peek-u8
        port?
        positive?
        procedure?
        quasiquote
        quote
;        quotient
        raise
        raise-continuable
;        rational?
;        rationalize
;        read-bytevector
;        read-bytevector!
;        read-char
        read-error?
;        read-line
;        read-string
;        read-u8
;        real?
;        remainder
        reverse
;        round
        set!
        set-car!
        set-cdr!
;        square
        string
        string->list
;        string->number
        string->symbol
        string->utf8
        string->vector
        string-append
        string-copy
        string-copy!
        string-fill!
        string-for-each
        string-length
        string-map
        string-ref
        string-set!
        string<=?
        string<?
        string=?
        string>=?
        string>?
        string?
        substring
        symbol->string
        symbol=?
        symbol?
        syntax-error
        syntax-rules
        textual-port?
;        truncate
;        truncate-quotient
;        truncate-remainder
;        truncate/
;        u8-ready?
        unless
        unquote
        unquote-splicing
        utf8->string
        values
        vector
        vector->list
        vector->string
        vector-append
        vector-copy
        vector-copy!
        vector-fill!
        vector-for-each
        vector-length
        vector-map
        vector-ref
        vector-set!
        vector?
        when
        with-exception-handler
;        write-bytevector
;        write-char
;        Write-string
;        write-u8
        zero?)
    (export ;; (scheme case-lambda)
        case-lambda)
    (export ;; (scheme char)
        char-alphabetic?
        char-ci<=?
        char-ci<?
        char-ci=?
        char-ci>=?
        char-ci>?
        char-downcase
        char-foldcase
        char-lower-case?
        char-numeric?
        char-upcase
        char-upper-case?
        char-whitespace?
        digit-value
        string-ci<=?
        string-ci<?
        string-ci=?
        string-ci>=?
        string-ci>?
        string-downcase
        string-foldcase
        string-upcase)
    (export ;; (scheme eval)
;        environment
         eval)
    (export ;; (scheme file)
;        call-with-input-file
;        call-with-output-file
        delete-file
        file-exists?
        open-binary-input-file
        open-binary-output-file
;        open-input-file
;        open-output-file
;        with-input-from-file
;        with-output-to-file
    )
    (export ;; (scheme inexact)
;        acos
;        asin
;        atan
;        cos
;        exp
;        finite?
;        infinite?
;        log
;        nan?
;        sin
        sqrt
;        tan
    )
     (export ;; (scheme process-context)
         command-line
;         exit
         get-environment-variable
         get-environment-variables
         emergency-exit)
    (export ;; (scheme repl)
        interaction-environment)
    (export ;; (scheme time)
        current-jiffy
        current-second
        jiffies-per-second)
    (export ;; (scheme write)
         display
         write
         write-shared
         write-simple)
    (export
        syntax unsyntax eq-hash eqv-hash display-shared display-simple
        equal-hash error-object-who full-error loaded-libraries library-path
        write-pretty display-pretty
        with-continuation-mark call-with-continuation-prompt abort-current-continuation
        default-prompt-tag (rename default-prompt-tag default-continuation-prompt-tag)
        default-prompt-handler current-continuation-marks collect make-guardian make-tracker
        make-exclusive make-condition current-thread run-thread enter-exclusive leave-exclusive
        condition-wait thread? condition-wake sleep exclusive? try-exclusive condition?
        condition-wake-all with-exclusive no-value)
    (begin
        (define (caar pair) (car (car pair)))
        (define (cadr pair) (car (cdr pair)))
        (define (cdar pair) (cdr (car pair)))
        (define (cddr pair) (cdr (cdr pair)))

        (define (substring string start end) (string-copy string start end))

        (define-syntax when
            (syntax-rules ()
                ((when test result1 result2 ...)
                    (if test (begin result1 result2 ...)))))

        (define-syntax unless
            (syntax-rules ()
                ((unless test result1 result2 ...)
                    (if (not test) (begin result1 result2 ...)))))

        (define-syntax cond
            (syntax-rules (else =>)
                ((cond (else result1 result2 ...)) (begin 'ignore result1 result2 ...))
                ((cond (test => result))
                    (let ((temp test)) (if temp (result temp))))
                ((cond (test => result) clause1 clause2 ...)
                    (let ((temp test))
                        (if temp
                            (result temp)
                            (cond clause1 clause2 ...))))
                ((cond (test)) test)
                ((cond (test) clause1 clause2 ...)
                    (let ((temp test))
                        (if temp temp (cond clause1 clause2 ...))))
                ((cond (test result1 result2 ...)) (if test (begin result1 result2 ...)))
                ((cond (test result1 result2 ...) clause1 clause2 ...)
                    (if test
                        (begin result1 result2 ...)
                        (cond clause1 clause2 ...)))))

        (define-syntax define-record-field
            (syntax-rules ()
                ((define-record-field type (name accessor))
                    (define accessor
                        (let ((idx (%record-index type 'name)))
                            (lambda (obj) (%record-ref type obj idx)))))
                ((define-record-field type (name accessor modifier))
                    (begin
                        (define accessor
                            (let ((idx (%record-index type 'name)))
                                (lambda (obj) (%record-ref type obj idx))))
                        (define modifier
                            (let ((idx (%record-index type 'name)))
                                (lambda (obj val) (%record-set! type obj idx val))))))))


        (define-syntax define-record-maker
            (syntax-rules ()
                ((define-record-maker type (arg ...) (adx ...) fld flds ...)
                    (let ((idx (%record-index type 'fld)))
                        (define-record-maker type (arg ... fld) (adx ... idx) flds ...)))
                ((define-record-maker type (arg ...) (idx ...))
                    (lambda (arg ...)
                        (let ((obj (%make-record type)))
                            (%record-set! type obj idx arg) ...
                            obj)))))

        (define-syntax define-record-type
            (syntax-rules ()
                ((define-record-type type (maker arg ...) predicate field ...)
                    (begin
                        (define type (%make-record-type 'type '(field ...)))
                        (define maker (define-record-maker type () () arg ...))
                        (define (predicate obj) (%record-predicate type obj))
                        (define-record-field type field) ...))))

        (define (map proc . lists)
            (define (map proc lists)
                (let ((cars (%map-car lists))
                        (cdrs (%map-cdr lists)))
                    (if (null? cars)
                        '()
                        (cons (apply proc cars) (map proc cdrs)))))
            (if (null? lists)
                (full-error 'assertion-violation 'map "map: expected at least one argument")
                (map proc lists)))

        (define (string-map proc . strings)
            (define (map proc idx strings)
                (let ((args (%map-strings idx strings)))
                    (if (null? args)
                        '()
                        (cons (apply proc args) (map proc (+ idx 1) strings)))))
            (if (null? strings)
                (full-error 'assertion-violation 'string-map
                        "string-map: expected at least one argument")
                (list->string (map proc 0 strings))))

        (define (vector-map proc . vectors)
            (define (map proc idx vectors)
                (let ((args (%map-vectors idx vectors)))
                    (if (null? args)
                        '()
                        (cons (apply proc args) (map proc (+ idx 1) vectors)))))
            (if (null? vectors)
                (full-error 'assertion-violation 'vector-map
                        "vector-map: expected at least one argument")
                (list->vector (map proc 0 vectors))))

        (define (for-each proc . lists)
            (define (for-each proc lists)
                (let ((cars (%map-car lists))
                        (cdrs (%map-cdr lists)))
                    (if (null? cars)
                        (no-value)
                        (begin (apply proc cars) (for-each proc cdrs)))))
            (if (null? lists)
                (full-error 'assertion-violation 'for-each
                        "for-each: expected at least one argument")
                (for-each proc lists)))

        (define (string-for-each proc . strings)
            (define (for-each proc idx strings)
                (let ((args (%map-strings idx strings)))
                    (if (null? args)
                        (no-value)
                        (begin (apply proc args) (for-each proc (+ idx 1) strings)))))
            (if (null? strings)
                (full-error 'assertion-violation 'string-for-each
                        "string-for-each: expected at least one argument")
                (for-each proc 0 strings)))

        (define (vector-for-each proc . vectors)
            (define (for-each proc idx vectors)
                (let ((args (%map-vectors idx vectors)))
                    (if (null? args)
                        (no-value)
                        (begin (apply proc args) (for-each proc (+ idx 1) vectors)))))
            (if (null? vectors)
                (full-error 'assertion-violation 'vector-for-each
                        "vector-for-each: expected at least one argument")
                (for-each proc 0 vectors)))

        (define (read-error? obj)
            (and (error-object? obj) (eq? (error-object-who 'read))))

        (define (file-error? obj)
            (and (error-object? obj)
                (let ((who (error-object-who obj)))
                    (or (eq? 'open-binary-input-file) (eq? 'open-binary-output-file)
                            (eq? who 'delete-file)))))

        (define (eval expr env)
            ((compile-eval expr env)))

        (define (call-with-values producer consumer)
                (let-values ((args (producer))) (apply consumer args)))

        (define (dynamic-wind before thunk after)
            (begin
                (before)
                (let-values ((results
                        (%mark-continuation 'dynamic-wind (cons before after) thunk)))
                    (after)
                    (apply values results))))

        (define-syntax with-exclusive
            (syntax-rules ()
                ((with-exclusive exclusive expr1 expr2 ...)
                    (dynamic-wind
                        (lambda () (enter-exclusive exclusive))
                        (lambda () expr1 expr2 ...)
                        (lambda () (leave-exclusive exclusive))))))

        (define parameterize-key (cons #f #f))

        (define (make-parameter init . converter)
            (let* ((converter
                    (if (null? converter)
                        (lambda (val) val)
                        (if (null? (cdr converter))
                            (car converter)
                            (full-error 'assertion-violation 'make-parameter
                                    "make-parameter: expected one or two arguments"))))
                    (init (converter init)))
                (letrec
                    ((parameter
                        (case-lambda
                            (() (let ((stk (eq-hashtable-ref (%parameters) parameter '())))
                                    (if (null? stk)
                                        init
                                        (car stk))))
                            ((val)
                                (let ((stk (eq-hashtable-ref (%parameters) parameter '())))
                                    (eq-hashtable-set! (%parameters) parameter
                                            (cons (converter val)
                                            (if (null? stk) '() (cdr stk))))))
                            ((val key) ;; used by parameterize
                                (if (eq? key parameterize-key)
                                    (converter val)
                                    (full-error 'assertion-violation '<parameter>
                                        "<parameter>: expected zero or one arguments")))
                            (val (full-error 'assertion-violation '<parameter>
                                    "<parameter>: expected zero or one arguments")))))
                    (%procedure->parameter parameter)
                    parameter)))

        (define-syntax parameterize
            (syntax-rules ()
                ((parameterize () body1 body2 ...)
                        (begin body1 body2 ...))
                ((parameterize ((param1 value1) (param2 value2) ...) body1 body2 ...)
                    (call-with-parameterize (list param1 param2 ...) (list value1 value2 ...)
                            (lambda () body1 body2 ...)))))

        (define (before-parameterize params vals)
            (if (not (null? params))
                (let ((p (car params)))
                    (if (not (%parameter? p))
                        (full-error 'assertion-violation 'parameterize
                                "parameterize: expected a parameter" p))
                    (let ((val (p (car vals) parameterize-key)))
                        (eq-hashtable-set! (%parameters) p
                                (cons val (eq-hashtable-ref (%parameters) p '())))
                        (before-parameterize (cdr params) (cdr vals))))))

        (define (after-parameterize params)
            (if (not (null? params))
                (begin
                    (eq-hashtable-set! (%parameters) (car params)
                            (cdr (eq-hashtable-ref (%parameters) (car params) '())))
                    (after-parameterize (cdr params)))))

        (define (call-with-parameterize params vals thunk)
            (before-parameterize params vals)
            (let-values ((results (%mark-continuation 'parameterize (cons params vals) thunk)))
                (after-parameterize params)
                (apply values results)))

        (define-syntax with-continuation-mark
            (syntax-rules ()
                ((_ key val expr) (%mark-continuation key val (lambda () expr)))))

        (define (current-continuation-marks)
            (reverse (cdr (reverse (map (lambda (dyn) (%dynamic-marks dyn)) (%dynamic-stack))))))

        (define default-prompt-tag-key (cons #f #f))

        (define (default-prompt-tag)
            default-prompt-tag-key)

        (define (default-prompt-handler proc)
            (call-with-continuation-prompt proc (default-prompt-tag) default-prompt-handler))

        (define (call-with-continuation-prompt proc tag handler . args)
            (if (and (eq? tag (default-prompt-tag)) (not (eq? handler default-prompt-handler)))
                (full-error 'assertion-violation 'call-with-continuation-prompt
                        "call-with-continuation-prompt: use of default-prompt-tag requires use of default-prompt-handler"))
            (with-continuation-mark tag handler (apply proc args)))

        (define (unwind-mark-list ml)
            (if (pair? ml)
                (begin
                    (if (eq? (car (car ml)) 'parameterize)
                        (after-parameterize (car (cdr (car ml))))
                        (if (eq? (car (car ml)) 'dynamic-wind)
                            ((cdr (cdr (car ml)))))) ; dynamic-wind after
                    (unwind-mark-list (cdr ml)))))

        (define (rewind-mark-list ml)
            (if (pair? ml)
                (begin
                    (if (eq? (car (car ml)) 'parameterize)
                        (before-parameterize (car (cdr (car ml))) (cdr (cdr (car ml))))
                        (if (eq? (car (car ml)) 'dynamic-wind)
                            ((car (cdr (car ml)))))) ; dynamic-wind before
                    (rewind-mark-list (cdr ml)))))

        (define (abort-current-continuation tag . vals)
            (define (find-mark ds key)
                (if (null? ds)
                    (values #f #f)
                    (let ((ret (assq key (%dynamic-marks (car ds)))))
                        (if (not ret)
                            (find-mark (cdr ds) key)
                            (values (car ds) (cdr ret))))))
            (define (unwind to)
                (let ((dyn (car (%dynamic-stack))))
                    (%dynamic-stack (cdr (%dynamic-stack)))
                    (unwind-mark-list (%dynamic-marks dyn))
                    (if (not (eq? dyn to))
                        (unwind to))))
            (let-values (((dyn handler) (find-mark (%dynamic-stack) tag)))
                (if (not dyn)
                    (full-error 'assertion-violation 'abort-current-continuation
                            "abort-current-continuation: expected a prompt tag" tag))
                (unwind dyn)
                (%abort-dynamic dyn (lambda () (apply handler vals)))))

        (define (execute-thunk thunk)
            (%return (call-with-continuation-prompt thunk (default-prompt-tag)
                    default-prompt-handler)))

        (%execute-thunk execute-thunk)

        (define (call-with-current-continuation proc)
            (define (unwind ds)
                (if (pair? ds)
                    (begin
                        (unwind-mark-list (%dynamic-marks (car ds)))
                        (unwind (cdr ds)))))
            (define (rewind ds)
                (if (pair? ds)
                    (begin
                        (rewind (cdr ds))
                        (%dynamic-stack ds)
                        (rewind-mark-list (%dynamic-marks (car ds))))))
            (%capture-continuation
                (lambda (cont)
                    (let ((ds (%dynamic-stack)))
                        (proc
                            (lambda vals
                                (unwind (%dynamic-stack))
                                (%call-continuation cont
                                    (lambda ()
                                        (rewind ds)
                                        (apply values vals)))))))))

        (define (with-exception-handler handler thunk)
            (if (not (procedure? handler))
                (full-error 'assertion-violation 'with-exception-handler
                            "with-exception-handler: expected a procedure" handler))
            (%mark-continuation 'exception-handler
                    (cons handler (%find-mark 'exception-handler '())) thunk))

        (define (raise-handler obj lst)
            (%mark-continuation 'exception-handler (cdr lst)
                    (lambda () ((car lst) obj) (raise obj))))

        (%raise-handler raise-handler)

        (define (raise-continuable obj)
            (let ((lst (%find-mark 'exception-handler '())))
                (if (null? lst)
                    (raise obj))
                (%mark-continuation 'exception-handler (cdr lst)
                        (lambda () ((car lst) obj)))))

        (define guard-key (cons #f #f))

        (define (with-guard guard thunk)
            (let ((gds (%dynamic-stack)))
                (call-with-continuation-prompt
                    (lambda ()
                        (with-exception-handler
                            (lambda (obj)
                                (let ((hds (%dynamic-stack)))
                                    (%dynamic-stack gds)
                                    (let-values ((lst (guard obj hds)))
                                        (%dynamic-stack hds)
                                        (abort-current-continuation guard-key lst))))
                            thunk))
                    guard-key
                    (lambda (lst) (apply values lst)))))

        (define-syntax guard
            (syntax-rules (else)
                ((guard (var clause1 clause2 ... (else result1 result2 ...)) body1 body2 ...)
                    (with-guard
                        (lambda (var hds) (cond clause1 clause2 ... (else result1 result2 ...)))
                        (lambda () body1 body2 ...)))
                ((guard (var clause1 clause2 ...) body1 body2 ...)
                    (with-guard
                        (lambda (var hds)
                            (cond clause1 clause2 ...
                                (else (%dynamic-stack hds) (raise-continuable var))))
                        (lambda () body1 body2 ...)))))

        (define (make-guardian)
            (let ((tconc (let ((last (cons #f '()))) (cons last last))))
                (case-lambda
                    (()
                        (if (eq? (car tconc) (cdr tconc))
                            #f
                            (let ((first (car tconc)))
                                (set-car! tconc (cdr first))
                                (car first))))
                    ((obj) (install-guardian obj tconc)))))

        (define (make-tracker)
            (let ((tconc (let ((last (cons #f '()))) (cons last last))))
                (case-lambda
                    (()
                        (if (eq? (car tconc) (cdr tconc))
                            #f
                            (let ((first (car tconc)))
                                (set-car! tconc (cdr first))
                                (car first))))
                    ((obj) (install-tracker obj obj tconc))
                    ((obj ret) (install-tracker obj ret tconc)))))

        (define current-input-port
            (make-parameter %standard-input
                (lambda (obj)
                    (if (not (and (input-port? obj) (input-port-open? obj)))
                        (full-error 'current-input-port
                                "current-input-port: expected an open textual input port" obj))
                    obj)))

        (define current-output-port
            (make-parameter %standard-output
                (lambda (obj)
                    (if (not (and (output-port? obj) (output-port-open? obj)))
                        (full-error 'current-output-port
                                "current-output-port: expected an open textual output port" obj))
                    obj)))

        (define current-error-port
            (make-parameter %standard-error
                (lambda (obj)
                    (if (not (and (output-port? obj) (output-port-open? obj)))
                        (full-error 'current-error-port
                                "current-error-port: expected an open textual output port" obj))
                    obj)))
    ))

(define-library (scheme base)
    (import (foment base))
    (export
        import
        define-library
        *
        +
        -
;;        ...
        /
        <
        <=
        =
        =>
        >
        >=
        abs
        and
        append
        apply
        assoc
        assq
;;        assv
        begin
        binary-port?
        boolean=?
        boolean?
        bytevector
        bytevector-append
        bytevector-copy
        bytevector-copy!
        bytevector-length
        bytevector-u8-ref
        bytevector-u8-set!
        bytevector?
        caar
        cadr
        call-with-current-continuation
;;        call-with-port
        call-with-values
        (rename call-with-current-continuation call/cc)
        car
        case
        cdar
        cddr
        cdr
;;        ceiling
        char->integer
;;        char-ready?
        char<=?
        char<?
        char=?
        char>=?
        char>?
        char?
        close-input-port
        close-output-port
        close-port
;;        complex?
        cond
        cond-expand
        cons
        current-error-port
        current-input-port
        current-output-port
        define
        define-record-type
        define-syntax
        define-values
;;        denominator
        do
        dynamic-wind
        else
;;        eof-object
;;        eof-object?
        eq?
        equal?
        eqv?
        error
        error-object-irritants
        error-object-message
        error-object?
        even?
;;        exact
;;        exact-integer-sqrt
        exact-integer?
;;        exact?
        expt
        features
        file-error?
;;        floor
;;        floor-quotient
;;        floor-remainder
;;        floor/
;;        flush-output-port
        for-each
;;        gcd
        get-output-bytevector
        get-output-string
        guard
        if
        include
        include-ci
;;        inexact
;;        inexact?
        input-port-open?
        input-port?
        integer->char
;;        integer?
        lambda
;;        lcm
        length
        let
        let*
        let*-values
        let-syntax
        let-values
        letrec
        letrec*
        letrec-syntax
        list
        list->string
        list->vector
;;        list-copy
        list-ref
;;        list-set!
;;        list-tail
;;        list?
        make-bytevector
        make-list
        make-parameter
        make-string
        make-vector
        map
;;        max
;;        member
        memq
;;        memv
;;        min
;;        modulo
        negative?
        newline
        not
        null?
        number->string
;;        number?
;;        numerator
        odd?
        open-input-bytevector
        open-input-string
        open-output-bytevector
        open-output-string
        or
        output-port-open?
        output-port?
        pair?
        parameterize
;;        peek-char
;;        peek-u8
        port?
        positive?
        procedure?
        quasiquote
        quote
;;        quotient
        raise
        raise-continuable
;;        rational?
;;        rationalize
;;        read-bytevector
;;        read-bytevector!
;;        read-char
        read-error?
;;        read-line
;;        read-string
;;        read-u8
;;        real?
;;        remainder
        reverse
;;        round
        set!
        set-car!
        set-cdr!
;;        square
        string
        string->list
;;        string->number
        string->symbol
        string->utf8
        string->vector
        string-append
        string-copy
        string-copy!
        string-fill!
        string-for-each
        string-length
        string-map
        string-ref
        string-set!
        string<=?
        string<?
        string=?
        string>=?
        string>?
        string?
        substring
        symbol->string
        symbol=?
        symbol?
        syntax-error
        syntax-rules
        textual-port?
;;        truncate
;;        truncate-quotient
;;        truncate-remainder
;;        truncate/
;;        u8-ready?
        unless
        unquote
        unquote-splicing
        utf8->string
        values
        vector
        vector->list
        vector->string
        vector-append
        vector-copy
        vector-copy!
        vector-fill!
        vector-for-each
        vector-length
        vector-map
        vector-ref
        vector-set!
        vector?
        when
        with-exception-handler
;;        write-bytevector
;;        write-char
;;        Write-string
;;        write-u8
        zero?))

(define-library (scheme case-lambda)
    (import (foment base))
    (export
        case-lambda))

(define-library (scheme char)
    (import (foment base))
    (export
        char-alphabetic?
        char-ci<=?
        char-ci<?
        char-ci=?
        char-ci>=?
        char-ci>?
        char-downcase
        char-foldcase
        char-lower-case?
        char-numeric?
        char-upcase
        char-upper-case?
        char-whitespace?
        digit-value
        string-ci<=?
        string-ci<?
        string-ci=?
        string-ci>=?
        string-ci>?
        string-downcase
        string-foldcase
        string-upcase))

;; (define-library (scheme complex)

;; (define-library (scheme cxr)

(define-library (scheme eval)
    (import (foment base))
    (export
;;        environment
         eval))

 (define-library (scheme file)
    (import (foment base))
    (export
;;        call-with-input-file
;;        call-with-output-file
        delete-file
        file-exists?
        open-binary-input-file
        open-binary-output-file
;;        open-input-file
;;        open-output-file
;;        with-input-from-file
;;        with-output-to-file
    ))

(define-library (scheme inexact)
    (import (foment base))
    (export
;;        acos
;;        asin
;;        atan
;;        cos
;;        exp
;;        finite?
;;        infinite?
;;        log
;;        nan?
;;        sin
        sqrt
;;        tan
    ))

;; (define-library (scheme lazy)

;; (define-library (scheme load)

(define-library (scheme process-context)
    (import (foment base))
     (export
         command-line
;;         exit
         get-environment-variable
         get-environment-variables
         emergency-exit))

;; (define-library (scheme read)

(define-library (scheme repl)
    (import (foment base))
    (export
        interaction-environment))

(define-library (scheme time)
    (import (foment base))
    (export
        current-jiffy
        current-second
        jiffies-per-second))

(define-library (scheme write)
    (import (foment base))
    (export
         display
         write
         write-shared
         write-simple))
