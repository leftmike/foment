(define-library (foment base)
    (import (foment bedrock))
    (export ;; (scheme base)
        *
        +
        -
        ...
        /
        <
        <=
        =
        =>
        >
        >=
        _
        abs
        and
        append
        apply
        assoc
        assq
        assv
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
        call-with-port
        call-with-values
        (rename call-with-current-continuation call/cc)
        car
        case
        cdar
        cddr
        cdr
        ceiling
        char->integer
        char-ready?
        char<=?
        char<?
        char=?
        char>=?
        char>?
        char?
        close-input-port
        close-output-port
        close-port
        complex?
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
        denominator
        do
        dynamic-wind
        else
        eof-object
        eof-object?
        eq?
        equal?
        eqv?
        error
        error-object-irritants
        error-object-message
        error-object?
        even?
        exact
        exact-integer-sqrt
        exact-integer?
        exact?
        expt
        features
        file-error?
        floor
        floor-quotient
        floor-remainder
        floor/
        flush-output-port
        for-each
        gcd
        get-output-bytevector
        get-output-string
        guard
        if
        include
        include-ci
        inexact
        inexact?
        input-port-open?
        input-port?
        integer->char
        integer?
        lambda
        lcm
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
        list-copy
        list-ref
        list-set!
        list-tail
        list?
        make-bytevector
        make-list
        make-parameter
        make-string
        make-vector
        map
        max
        member
        memq
        memv
        min
        (rename floor-remainder modulo)
        negative?
        newline
        not
        null?
        number->string
        number?
        numerator
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
        peek-char
        peek-u8
        port?
        positive?
        procedure?
        quasiquote
        quote
        (rename truncate-quotient quotient)
        raise
        raise-continuable
        rational?
        rationalize
        read-bytevector
        read-bytevector!
        read-char
        read-error?
        read-line
        read-string
        read-u8
        real?
        (rename truncate-remainder remainder)
        reverse
        round
        set!
        set-car!
        set-cdr!
        square
        string
        string->list
        string->number
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
        truncate
        truncate-quotient
        truncate-remainder
        truncate/
        u8-ready?
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
        write-bytevector
        write-char
        write-string
        write-u8
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
    (export ;; (scheme complex)
        angle
        imag-part
        magnitude
        make-polar
        make-rectangular
        real-part)
    (export ;; (scheme cxr)
        caaar
        cdaar
        cadar
        cddar
        caadr
        cdadr
        caddr
        cdddr
        caaaar
        cdaaar
        cadaar
        cddaar
        caadar
        cdadar
        caddar
        cdddar
        caaadr
        cdaadr
        cadadr
        cddadr
        caaddr
        cdaddr
        cadddr
        cddddr)
    (export ;; (scheme eval)
        environment
        eval)
    (export ;; (scheme file)
        call-with-input-file
        call-with-output-file
        delete-file
        file-exists?
        open-binary-input-file
        open-binary-output-file
        open-input-file
        open-output-file
        with-input-from-file
        with-output-to-file
    )
    (export ;; (scheme inexact)
        acos
        asin
        atan
        cos
        exp
        finite?
        infinite?
        log
        nan?
        sin
        sqrt
        tan
    )
    (export ;; (scheme lazy)
        delay
        delay-force
        force
        make-promise
        promise?)
    (export ;; (scheme load)
        load)
    (export ;; (scheme process-context)
        command-line
        exit
        get-environment-variable
        get-environment-variables
        emergency-exit)
    (export ;; (scheme read)
        read)
    (export ;; (scheme repl)
        interaction-environment)
    (export ;; (scheme r5rs)
        scheme-report-environment
        null-environment)
    (export ;; (scheme time)
        current-jiffy
        current-second
        jiffies-per-second)
    (export ;; (scheme write)
         display
         write
         write-shared
         write-simple)
    (export ;; (scheme inquiry) and (srfi 112)
        implementation-name
        implementation-version
        cpu-architecture
        machine-name
        os-name
        os-version)
    (export ;; (scheme boxes) and (srfi 111)
        box
        box?
        unbox
        set-box!)
    (export ;; (srfi 60)
        bitwise-and
        bitwise-ior
        bitwise-xor
        bitwise-not
        bit-count
        integer-length
        first-set-bit
        arithmetic-shift
        )
    (export
        make-ascii-port
        make-latin1-port
        make-utf8-port
        make-utf16-port
        make-buffered-port
        make-encoded-port
        file-encoding
        want-identifiers
        set-console-input-editline!
        set-console-input-echo!
        with-continuation-mark
        call-with-continuation-prompt
        abort-current-continuation
        default-prompt-tag
        (rename default-prompt-tag default-continuation-prompt-tag)
        default-prompt-handler
        current-continuation-marks
        collect
        partial-per-full
        trigger-bytes
        trigger-objects
        dump-gc
        make-guardian
        make-tracker
        make-exclusive
        make-condition
        current-thread
        run-thread
        exit-thread
        emergency-exit-thread
        enter-exclusive
        leave-exclusive
        condition-wait
        thread?
        condition-wake
        exclusive?
        try-exclusive
        condition?
        condition-wake-all
        with-exclusive
        sleep
        syntax
        unsyntax
        error-object-type
        error-object-who
        full-error
        loaded-libraries
        library-path
        make-eq-hashtable
        eq-hashtable-ref
        eq-hashtable-set!
        eq-hashtable-delete
        random
        no-value
        set!-values
        get-ip-addresses
        socket?
        make-socket
        bind-socket
        listen-socket
        accept-socket
        connect-socket
        shutdown-socket
        send-socket
        recv-socket
        address-family
        address-info
        message-type
        ip-protocol
        shutdown-method
        socket-domain
        socket-merge-flags
        socket-purge-flags
        *af-unspec*
        *af-inet*
        *af-inet6*
        *sock-stream*
        *sock-dgram*
        *sock-raw*
        *ai-canonname*
        *ai-numerichost*
        *ai-v4mapped*
        *ai-all*
        *ai-addrconfig*
        *ipproto-ip*
        *ipproto-tcp*
        *ipproto-udp*
        *msg-peek*
        *msg-oob*
        *msg-waitall*
        *shut-rd*
        *shut-wr*
        *shut-rdwr*
        file-size
        file-regular?
        file-directory?
        file-symbolic-link?
        file-writable?
        file-stat-ctime
        file-stat-mtime
        file-stat-atime
        create-symbolic-link
        rename-file
        create-directory
        delete-directory
        list-directory
        copy-file
        current-directory
        build-path)
    (cond-expand
        (unix
            (export
                file-readable?
                file-executable?))
        (windows
            (export
                file-archive?
                file-system?
                file-hidden?)))
    (begin
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

        (define (caar pair) (car (car pair)))
        (define (cadr pair) (car (cdr pair)))
        (define (cdar pair) (cdr (car pair)))
        (define (cddr pair) (cdr (cdr pair)))

        (define (caaar pair) (car (car (car pair))))
        (define (cdaar pair) (cdr (car (car pair))))
        (define (cadar pair) (car (cdr (car pair))))
        (define (cddar pair) (cdr (cdr (car pair))))
        (define (caadr pair) (car (car (cdr pair))))
        (define (cdadr pair) (cdr (car (cdr pair))))
        (define (caddr pair) (car (cdr (cdr pair))))
        (define (cdddr pair) (cdr (cdr (cdr pair))))
        (define (caaaar pair) (car (car (car (car pair)))))
        (define (cdaaar pair) (cdr (car (car (car pair)))))
        (define (cadaar pair) (car (cdr (car (car pair)))))
        (define (cddaar pair) (cdr (cdr (car (car pair)))))
        (define (caadar pair) (car (car (cdr (car pair)))))
        (define (cdadar pair) (cdr (car (cdr (car pair)))))
        (define (caddar pair) (car (cdr (cdr (car pair)))))
        (define (cdddar pair) (cdr (cdr (cdr (car pair)))))
        (define (caaadr pair) (car (car (car (cdr pair)))))
        (define (cdaadr pair) (cdr (car (car (cdr pair)))))
        (define (cadadr pair) (car (cdr (car (cdr pair)))))
        (define (cddadr pair) (cdr (cdr (car (cdr pair)))))
        (define (caaddr pair) (car (car (cdr (cdr pair)))))
        (define (cdaddr pair) (cdr (car (cdr (cdr pair)))))
        (define (cadddr pair) (car (cdr (cdr (cdr pair)))))
        (define (cddddr pair) (cdr (cdr (cdr (cdr pair)))))

        (define (floor/ n1 n2)
            (values (floor-quotient n1 n2) (floor-remainder n1 n2)))

        (define (floor-remainder n1 n2)
            (- n1 (* n2 (floor-quotient n1 n2))))

        (define (truncate/ n1 n2)
            (values (truncate-quotient n1 n2) (truncate-remainder n1 n2)))

        (define (exact-integer-sqrt k)
            (let ((ret (%exact-integer-sqrt k)))
                (values (car ret) (cdr ret))))

        (define (magnitude z)
            (if (real? z)
                z
                (sqrt (+ (* (real-part z) (real-part z)) (* (imag-part z) (imag-part z))))))

        (define (angle z)
            (if (real? z)
                (if (exact? z) 0 0.0)
                (atan (imag-part z) (real-part z))))

        ;; From Chibi Scheme
        ;; Adapted from Bawden's algorithm.
        (define (rationalize x e)
            (define (simplest x y return)
                (let ((fx (floor x)) (fy (floor y)))
                    (cond
                        ((>= fx x)
                            (return fx 1))
                        ((= fx fy)
                            (simplest (/ (- y fy)) (/ (- x fx))
                                    (lambda (n d) (return (+ d (* fx n)) n))))
                        (else
                            (return (+ fx 1) 1)))))
            (let ((return (if (negative? x) (lambda (num den) (/ (- num) den)) /))
                    (x (abs x))
                    (e (abs e)))
                (simplest (- x e) (+ x e) return)))

        (define member
            (case-lambda
                ((obj list) (%member obj list))
                ((obj list compare)
                    (define (member list)
                        (if (null? list)
                            #f
                            (if (compare obj (car list))
                                list
                                (member (cdr list)))))
                    (if (not (list? list))
                        (full-error 'assertion-violation 'member "member: expected a list"))
                    (member list))))

        (define assoc
            (case-lambda
                ((obj list) (%assoc obj list))
                ((obj list compare)
                    (define (assoc list)
                        (if (null? list)
                            #f
                            (if (compare obj (car (car list)))
                                (car list)
                                (assoc (cdr list)))))
                    (if (not (list? list))
                        (full-error 'assertion-violation 'assoc "assoc: expected a list"))
                    (assoc list))))

        (define (substring string start end) (string-copy string start end))

        (define (scheme-report-environment version)
            (if (not (eq? version 5))
                (full-error 'assertion-violation 'scheme-report-environment
                        "scheme-report-environment: expected a version of 5" version))
            (environment '(scheme r5rs)))

        (define (null-environment version)
            (if (not (eq? version 5))
                (full-error 'assertion-violation 'null-environment
                        "null-environment expected a version of 5" version))
            (environment '(scheme null)))

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
            (and (error-object? obj) (eq? (error-object-who obj) 'read)))

        (define (file-error? obj)
            (and (error-object? obj)
                (let ((who (error-object-who obj)))
                    (or (eq? who 'open-binary-input-file) (eq? who 'open-binary-output-file)
                            (eq? who 'delete-file)))))

        (define-record-type promise
            (%make-promise state)
            promise?
            (state promise-state set-promise-state!))

        (define-syntax delay-force
            (syntax-rules ()
                ((delay-force expression) (%make-promise (cons #f (lambda () expression))))))

        (define-syntax delay
            (syntax-rules ()
                ((delay expression) (delay-force (%make-promise (cons #t expression))))))

        (define (make-promise obj)
            (if (promise? obj)
                obj
                (%make-promise (cons #t obj))))

        (define (force promise)
            (if (promise? promise)
                (if (promise-done? promise)
                    (promise-value promise)
                    (let ((promise* ((promise-value promise))))
                        (unless (promise-done? promise) (promise-update! promise* promise))
                        (force promise)))
                promise))

        (define (promise-done? x) (car (promise-state x)))
        (define (promise-value x) (cdr (promise-state x)))
        (define (promise-update! new old)
            (set-car! (promise-state old) (promise-done? new))
            (set-cdr! (promise-state old) (promise-value new))
            (set-promise-state! new (promise-state old)))

        (define (call-with-values producer consumer)
                (let-values ((args (producer))) (apply consumer args)))

        (define (dynamic-wind before thunk after)
            (begin
                (before)
                (let-values ((results
                        (%mark-continuation 'dynamic-wind 'dynamic-wind (cons before after)
                                thunk)))
                    (after)
                    (apply values results))))

        (define-syntax with-exclusive
            (syntax-rules ()
                ((with-exclusive exclusive expr1 expr2 ...)
                    (dynamic-wind
                        (lambda () (enter-exclusive exclusive))
                        (lambda () expr1 expr2 ...)
                        (lambda () (leave-exclusive exclusive))))))

        (define push-parameter (cons #f #f))
        (define pop-parameter (cons #f #f))

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
                                (if (eq? val pop-parameter)
                                    (eq-hashtable-set! (%parameters) parameter
                                            (cdr (eq-hashtable-ref (%parameters)
                                            parameter '()))) ;; used by parameterize
                                    (let ((stk (eq-hashtable-ref (%parameters) parameter '())))
                                        (eq-hashtable-set! (%parameters) parameter
                                                (cons (converter val)
                                                (if (null? stk) '() (cdr stk)))))))
                            ((val key) ;; used by parameterize
                                (if (eq? key push-parameter)
                                    (eq-hashtable-set! (%parameters) parameter
                                            (cons (converter val)
                                            (eq-hashtable-ref (%parameters) parameter '())))
                                    (full-error 'assertion-violation '<parameter>
                                            "<parameter>: expected zero or one arguments")))
                            (val (full-error 'assertion-violation '<parameter>
                                    "<parameter>: expected zero or one arguments")))))
                    (%procedure->parameter parameter)
                    parameter)))

        (define (make-index-parameter index init converter)
            (let ((parameter
                    (case-lambda
                        (() (car (%index-parameter index)))
                        ((val)
                            (if (eq? val pop-parameter)
                                (%index-parameter index (cdr (%index-parameter index)))
                                (%index-parameter index
                                        (cons (converter val) (cdr (%index-parameter index))))))
                        ((val key) ;; used by parameterize
                            (if (eq? key push-parameter)
                                (%index-parameter index (cons (converter val)
                                        (%index-parameter index)))
                                (full-error 'assertion-violation '<parameter>
                                        "<parameter>: expected zero or one arguments")))
                        (val (full-error 'assertion-violation '<parameter>
                                "<parameter>: expected zero or one arguments")))))
                (%index-parameter index (list (converter init)))
                (%procedure->parameter parameter)
                parameter))

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
                    (p (car vals) push-parameter)
                    (before-parameterize (cdr params) (cdr vals)))))

        (define (after-parameterize params)
            (if (not (null? params))
                (begin
                    ((car params) pop-parameter)
                    (after-parameterize (cdr params)))))

        (define (call-with-parameterize params vals thunk)
            (before-parameterize params vals)
            (let-values ((results (%mark-continuation 'mark 'parameterize (cons params vals)
                        thunk)))
                (after-parameterize params)
                (apply values results)))

        (define-syntax with-continuation-mark
            (syntax-rules ()
                ((_ key val expr) (%mark-continuation 'mark key val (lambda () expr)))))

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
            (%mark-continuation 'prompt tag handler (lambda () (apply proc args))))

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

        (define (unwind ds tail)
            (if (pair? ds)
                (begin
                    (unwind-mark-list (%dynamic-marks (car ds)))
                    (if (not (eq? ds tail))
                        (unwind (cdr ds) tail)))))

        (define (rewind ds tail)
            (if (pair? ds)
                (begin
                    (if (not (eq? ds tail))
                        (rewind (cdr ds) tail))
                    (%dynamic-stack ds)
                    (rewind-mark-list (%dynamic-marks (car ds))))))

        (define (find-tail old new)
            (define (find old new)
                (if (eq? old new)
                    old
                    (find (cdr old) (cdr new))))
            (let ((olen (length old))
                    (nlen (length new)))
                (find (if (> olen nlen) (list-tail old (- olen nlen)) old)
                    (if (> nlen olen) (list-tail new (- nlen olen)) new))))

        (define (unwind-rewind old new)
            (let ((tail (find-tail old new)))
                (unwind old tail)
                (rewind new tail)))

        (define (call-with-current-continuation proc)
            (%capture-continuation
                (lambda (cont)
                    (let ((ds (%dynamic-stack)))
                        (proc
                            (lambda vals
                                (let ((tail (find-tail (%dynamic-stack) ds)))
                                (unwind (%dynamic-stack) tail)
                                (%call-continuation cont
                                    (lambda ()
                                        (rewind ds tail)
                                        (apply values vals))))))))))

        (define (with-exception-handler handler thunk)
            (if (not (procedure? handler))
                (full-error 'assertion-violation 'with-exception-handler
                            "with-exception-handler: expected a procedure" handler))
            (%mark-continuation 'mark 'exception-handler
                    (cons handler (%find-mark 'exception-handler '())) thunk))

        (define (raise-handler obj lst)
            (%mark-continuation 'mark 'exception-handler (cdr lst)
                    (lambda () ((car lst) obj) (raise obj))))

        (%set-raise-handler! raise-handler)

        (define (raise-continuable obj)
            (let ((lst (%find-mark 'exception-handler '())))
                (if (null? lst)
                    (raise obj))
                (%mark-continuation 'mark 'exception-handler (cdr lst)
                        (lambda () ((car lst) obj)))))

        (define (with-notify-handler handler thunk)
            (if (not (procedure? handler))
                (full-error 'assertion-violation 'with-notify-handler
                            "with-notify-handler: expected a procedure" handler))
            (%mark-continuation 'mark 'notify-handler
                    (cons handler (%find-mark 'notify-handler '())) thunk))

        (define (notify-handler obj lst)
            (%mark-continuation 'mark 'notify-handler (cdr lst)
                    (lambda () ((car lst) obj) (exit-thread obj))))

        (%set-notify-handler! notify-handler)

        (define guard-key (cons #f #f))

        (define (with-guard guard thunk)
            (call-with-continuation-prompt
                (lambda ()
                    (let ((gds (%dynamic-stack)))
                        (with-exception-handler
                            (lambda (obj)
                                (let ((hds (%dynamic-stack))
                                        (abort (box #t)))
                                    (unwind-rewind hds gds)
                                    (let-values ((lst (guard obj hds abort)))
                                        (if (unbox abort)
                                            (abort-current-continuation guard-key lst)
                                            (apply values lst)))))
                            thunk)))
                guard-key
                (lambda (lst) (apply values lst))))

        (define-syntax guard
            (syntax-rules (else)
                ((guard (var clause ... (else result1 result2 ...)) body1 body2 ...)
                    (with-guard
                        (lambda (var hds abort) (cond clause ... (else result1 result2 ...)))
                        (lambda () body1 body2 ...)))
                ((guard (var clause1 clause2 ...) body1 body2 ...)
                    (with-guard
                        (lambda (var hds abort)
                            (cond clause1 clause2 ...
                                (else
                                    (set-box! abort #f)
                                    (unwind-rewind (%dynamic-stack) hds)
                                    (raise-continuable var))))
                        (lambda () body1 body2 ...)))))

        (define (exit . args)
            (unwind (%dynamic-stack) '())
            (apply %exit args))

        (define (emergency-exit . args)
            (apply %exit args))

        (define (exit-thread obj)
            (unwind (%dynamic-stack) '())
            (%exit-thread obj))

        (define (emergency-exit-thread obj)
            (%exit-thread obj))

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
            (make-index-parameter 0 %standard-input
                (lambda (obj)
                    (if (not (and (input-port? obj) (input-port-open? obj)))
                        (full-error 'assertion-violation 'current-input-port
                                "current-input-port: expected an open input port" obj))
                    obj)))

        (define current-output-port
            (make-index-parameter 1 %standard-output
                (lambda (obj)
                    (if (not (and (output-port? obj) (output-port-open? obj)))
                        (full-error 'assertion-violation 'current-output-port
                                "current-output-port: expected an open output port" obj))
                    obj)))

        (define current-error-port
            (make-index-parameter 2 %standard-error
                (lambda (obj)
                    (if (not (and (output-port? obj) (output-port-open? obj)))
                        (full-error 'assertion-violation 'current-error-port
                                "current-error-port: expected an open output port" obj))
                    obj)))

        (define file-encoding
            (make-parameter make-encoded-port))

        (define (open-input-file string)
            ((file-encoding) (open-binary-input-file string)))

        (define (open-output-file string)
            ((file-encoding) (open-binary-output-file string)))

        (define (call-with-port port proc)
            (let-values ((results (proc port)))
                (close-port port)
                (apply values results)))

        (define (call-with-input-file string proc)
            (call-with-port (open-input-file string) proc))

        (define (call-with-output-file string proc)
            (call-with-port (open-output-file string) proc))

        (define (with-input-from-file string thunk)
            (let ((port (open-input-file string)))
                (let-values ((results (parameterize ((current-input-port port)) (thunk))))
                    (close-input-port port)
                    (apply values results))))

        (define (with-output-to-file string thunk)
            (let ((port (open-output-file string)))
                (let-values ((results (parameterize ((current-output-port port)) (thunk))))
                    (close-output-port port)
                    (apply values results))))

        (define-syntax address-family
            (syntax-rules (unspec inet inet6)
                ((address-family unspec) *af-unspec*)
                ((address-family inet) *af-inet*)
                ((address-family inet6) *af-inet6*)))

        (define-syntax socket-domain
            (syntax-rules (stream datagram raw)
                ((socket-domain stream) *sock-stream*)
                ((socket-domain datagram) *sock-dgram*)
                ((socket-domain raw) *sock-raw*)))

        (define-syntax ip-protocol
            (syntax-rules (ip tcp udp)
                ((ip-protocol ip) *ipproto-ip*)
                ((ip-protocol tcp) *ipproto-tcp*)
                ((ip-protocol udp) *ipproto-udp*)))

        (define-syntax shutdown-method
            (syntax-rules (read write)
                ((shutdown-method read) *shut-rd*)
                ((shutdown-method write) *shut-wr*)
                ((shutdown-method read write) *shut-rdwr*)
                ((shutdown-method write read) *shut-rdwr*)))

        (define (alist-lookup obj alist who msg)
            (cond
                ((assq obj alist) => cdr)
                (else (full-error 'syntax-error who msg obj))))

        (define address-info-alist
            (list
                (cons 'canonname *ai-canonname*)
                (cons 'numerichost *ai-numerichost*)
                (cons 'v4mapped *ai-v4mapped*)
                (cons 'all *ai-all*)
                (cons 'addrconfig *ai-addrconfig*)))

        (define-syntax address-info
            (syntax-rules ()
                ((address-info name ...)
                    (apply socket-merge-flags
                        (map
                            (lambda (n)
                                (alist-lookup n address-info-alist 'address-info
                                        "address-info: expected an address info flag"))
                            '(name ...))))))

        (define message-type-alist
            (list
                (cons 'none 0)
                (cons 'peek *msg-peek*)
                (cons 'oob *msg-oob*)
                (cons 'waitall *msg-waitall*)))

        (define-syntax message-type
            (syntax-rules ()
                ((message-type name ...)
                    (apply socket-merge-flags
                        (map
                            (lambda (n)
                                (alist-lookup n message-type-alist 'message-type
                                        "message-type: expected a message type flag"))
                            '(name ...))))))

        (define (build-path path1 path2)
            (string-append path1 (cond-expand (windows "\\") (else "/")) path2))

        (define (execute-thunk thunk)
            (%return (call-with-continuation-prompt thunk (default-prompt-tag)
                    default-prompt-handler)))

        (%execute-thunk execute-thunk)

        (define (eval expr env)
            ((%compile-eval expr env)))

        (define (%load filename env)
            (define (read-eval port)
                (let ((obj (read port)))
                    (if (not (eof-object? obj))
                        (begin
                            (eval obj env)
                            (read-eval port)))))
            (call-with-port (open-input-file filename)
                    (lambda (port) (want-identifiers port #t) (read-eval port))))

        (define load
            (case-lambda
                ((filename) (%load filename (interaction-environment)))
                ((filename env) (%load filename env))))

        (define (repl env exit)
            (define (exception-handler obj)
                (abort-current-continuation 'repl-prompt
                    (lambda ()
                        (cond
                            ((error-object? obj)
                                (write obj)
                                (newline))
                            (else
                                (display "unexpected exception object: ")
                                (write obj)
                                (newline))))))
            (define (notify-handler obj)
                (if (eq? obj 'sigint)
                    (abort-current-continuation 'repl-prompt
                        (lambda () (display "^C") (newline)))))
            (define (read-eval-write)
                (let ((obj (read)))
                    (if (eof-object? obj)
                        (exit obj)
                        (let-values ((lst (eval obj env)))
                            (if (and (pair? lst)
                                    (or (not (eq? (car lst) (no-value))) (pair? (cdr lst))))
                                (write-values lst))))))
            (define (write-values lst)
                (if (pair? lst)
                    (begin
                        (write (car lst))
                        (newline)
                        (write-values (cdr lst)))))
            (display "{") (write (%bytes-allocated)) (display "} =] ")
            (call-with-continuation-prompt
                (lambda ()
                    (with-exception-handler
                        exception-handler
                        (lambda ()
                            (with-notify-handler
                                notify-handler
                                read-eval-write))))
                'repl-prompt
                (lambda (abort) (abort)))
            (repl env exit))

        (define (handle-command-line lst env)
            (if (not (null? lst))
                (cond
                    ((and (string=? "-p" (car lst)) (not (null? (cdr lst))))
                            (write (eval (read (open-input-string (cadr lst))) env))
                            (newline)
                            (handle-command-line (cddr lst) env))
                    ((and (string=? "-e" (car lst)) (not (null? (cdr lst))))
                            (eval (read (open-input-string (cadr lst))) env)
                            (handle-command-line (cddr lst) env))
                    ((and (string=? "-l" (car lst)) (not (null? (cdr lst))))
                            (load (cadr lst) env)
                            (handle-command-line (cddr lst) env))
                    (else
                        (handle-command-line (cdr lst) env)))))

        (define history-file
            (cond-expand
                (windows "foment.history")
                (else (string-append (get-environment-variable "HOME") "/.foment_history"))))

        (define (interactive-thunk)
            (when (console-port? (current-output-port))
                (display "Foment Scheme ")
                (display (implementation-version))
                (if %debug-build
                    (display " (debug)"))
                (newline))
            (let ((env (interaction-environment)))
                (handle-command-line (cdr (command-line)) env)
                (call-with-current-continuation
                    (lambda (exit)
                        (set-ctrl-c-notify! 'broadcast)
                        (let ((port (current-input-port)))
                            (if (console-port? port)
                                (dynamic-wind
                                    (lambda ()
                                        (set-console-input-editline! port #t)
                                        (%load-history port history-file))
                                    (lambda ()
                                        (repl env exit))
                                    (lambda ()
                                        (%save-history port history-file)))
                                (repl env exit)))))))

        (%interactive-thunk interactive-thunk)
    ))

(define-library (scheme base)
    (import (foment base))
    (export
        *
        +
        -
        ...
        /
        <
        <=
        =
        =>
        >
        >=
        _
        abs
        and
        append
        apply
        assoc
        assq
        assv
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
        call-with-port
        call-with-values
        (rename call-with-current-continuation call/cc)
        car
        case
        cdar
        cddr
        cdr
        ceiling
        char->integer
        char-ready?
        char<=?
        char<?
        char=?
        char>=?
        char>?
        char?
        close-input-port
        close-output-port
        close-port
        complex?
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
        denominator
        do
        dynamic-wind
        else
        eof-object
        eof-object?
        eq?
        equal?
        eqv?
        error
        error-object-irritants
        error-object-message
        error-object?
        even?
        exact
        exact-integer-sqrt
        exact-integer?
        exact?
        expt
        features
        file-error?
        floor
        floor-quotient
        floor-remainder
        floor/
        flush-output-port
        for-each
        gcd
        get-output-bytevector
        get-output-string
        guard
        if
        include
        include-ci
        inexact
        inexact?
        input-port-open?
        input-port?
        integer->char
        integer?
        lambda
        lcm
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
        list-copy
        list-ref
        list-set!
        list-tail
        list?
        make-bytevector
        make-list
        make-parameter
        make-string
        make-vector
        map
        max
        member
        memq
        memv
        min
        (rename floor-remainder modulo)
        negative?
        newline
        not
        null?
        number->string
        number?
        numerator
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
        peek-char
        peek-u8
        port?
        positive?
        procedure?
        quasiquote
        quote
        (rename truncate-quotient quotient)
        raise
        raise-continuable
        rational?
        rationalize
        read-bytevector
        read-bytevector!
        read-char
        read-error?
        read-line
        read-string
        read-u8
        real?
        (rename truncate-remainder remainder)
        reverse
        round
        set!
        set-car!
        set-cdr!
        square
        string
        string->list
        string->number
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
        truncate
        truncate-quotient
        truncate-remainder
        truncate/
        u8-ready?
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
        write-bytevector
        write-char
        write-string
        write-u8
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

(define-library (scheme complex)
    (import (foment base))
    (export
        angle
        imag-part
        magnitude
        make-polar
        make-rectangular
        real-part))

(define-library (scheme cxr)
    (import (foment base))
    (export
        caaar
        cdaar
        cadar
        cddar
        caadr
        cdadr
        caddr
        cdddr
        caaaar
        cdaaar
        cadaar
        cddaar
        caadar
        cdadar
        caddar
        cdddar
        caaadr
        cdaadr
        cadadr
        cddadr
        caaddr
        cdaddr
        cadddr
        cddddr))

(define-library (scheme eval)
    (import (foment base))
    (export
        environment
        eval))

 (define-library (scheme file)
    (import (foment base))
    (export
        call-with-input-file
        call-with-output-file
        delete-file
        file-exists?
        open-binary-input-file
        open-binary-output-file
        open-input-file
        open-output-file
        with-input-from-file
        with-output-to-file
    ))

(define-library (scheme inexact)
    (import (foment base))
    (export
        acos
        asin
        atan
        cos
        exp
        finite?
        infinite?
        log
        nan?
        sin
        sqrt
        tan
    ))

(define-library (scheme lazy)
    (import (foment base))
    (export
        delay
        delay-force
        force
        make-promise
        promise?))

(define-library (scheme load)
    (import (foment base))
    (export
        load))

(define-library (scheme process-context)
    (import (foment base))
     (export
         command-line
         exit
         get-environment-variable
         get-environment-variables
         emergency-exit))

(define-library (scheme read)
    (import (foment base))
    (export
        read))

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

(define-library (scheme inquiry)
    (aka (srfi 112))
    (import (foment base))
    (export
        implementation-name
        implementation-version
        cpu-architecture
        machine-name
        os-name
        os-version))

(define-library (scheme boxes)
    (aka (srfi 111))
    (import (foment base))
    (export
        box
        box?
        unbox
        set-box!))

(define-library (scheme r5rs)
    (import (foment base))
    (export
        *
        +
        -
        /
        <
        <=
        =
        >
        >=
        abs
        acos
        and
        angle
        append
        apply
        asin
        assoc
        assq
        assv
        atan
        begin
        boolean?
        caaaar
        caaadr
        caaar
        caadar
        caaddr
        caadr
        caar
        cadaar
        cadadr
        cadar
        caddar
        cadddr
        caddr
        cadr
        call-with-current-continuation
        call-with-input-file call-with-output-file
        call-with-values
        car
        case
        cdaaar
        cdaadr
        cdaar
        cdadar
        cdaddr
        cdadr
        cdar
        cddaar
        cddadr
        cddar
        cdddar
        cddddr
        cdddr
        cddr
        cdr
        ceiling
        char->integer
        char-alphabetic?
        char-ci<=?
        char-ci<?
        char-ci=?
        char-ci>=?
        char-ci>?
        char-downcase
        char-lower-case?
        char-numeric?
        char-ready?
        char-upcase
        char-upper-case?
        char-whitespace?
        char<=?
        char<?
        char=?
        char>=?
        char>?
        char?
        close-input-port
        close-output-port
        complex?
        cond
        cons
        cos
        current-input-port
        current-output-port
        define
        define-syntax
        delay
        denominator
        display
        do
        dynamic-wind
        eof-object?
        eq?
        equal?
        eqv?
        eval
        even?
        (rename inexact exact->inexact)
        exact?
        exp
        expt
        floor
        for-each
        force
        gcd
        if
        imag-part
        (rename exact inexact->exact)
        inexact?
        input-port?
        integer->char
        integer?
        interaction-environment
        lambda
        lcm
        length
        let
        let*
        let-syntax
        letrec
        letrec-syntax
        list
        list->string
        list->vector
        list-ref
        list-tail
        list?
        load
        log
        magnitude
        make-polar
        make-rectangular
        make-string
        make-vector
        map
        max
        member
        memq
        memv
        min
        modulo
        negative?
        newline
        not
        null-environment
        null?
        number->string
        number?
        numerator
        odd?
        open-input-file
        open-output-file
        or
        output-port?
        pair?
        peek-char
        positive?
        procedure?
        quasiquote
        quote
        quotient
        rational?
        rationalize
        read
        read-char
        real-part
        real?
        remainder
        reverse
        round
        scheme-report-environment
        set!
        set-car!
        set-cdr!
        sin
        sqrt
        string
        string->list
        string->number
        string->symbol
        string-append
        string-ci<=?
        string-ci<?
        string-ci=?
        string-ci>=?
        string-ci>?
        string-copy
        string-fill!
        string-length
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
        symbol?
        tan
        truncate
        values
        vector
        vector->list
        vector-fill!
        vector-length
        vector-ref
        vector-set!
        vector?
        with-input-from-file
        with-output-to-file
        write
        write-char
        zero?))

(define-library (scheme null)
    (import (foment base))
    (export
        and
        begin
        case
        cond
        define
        define-syntax
        delay
        do
        force
        if
        lambda
        let
        let*
        let-syntax
        letrec
        letrec-syntax
        or
        quasiquote
        quote
        set!))

(define-library (srfi 106)
    (import (foment base))
    (export
        make-client-socket
        make-server-socket
        socket?
        (rename accept-socket socket-accept)
        socket-send
        socket-recv
        (rename shutdown-socket socket-shutdown)
        socket-input-port
        socket-output-port
        call-with-socket
        address-family
        address-info
        socket-domain
        ip-protocol
        message-type
        shutdown-method
        socket-merge-flags
        socket-purge-flags
        *af-unspec*
        *af-inet*
        *af-inet6*
        *sock-stream*
        *sock-dgram*
        *ai-canonname*
        *ai-numerichost*
        *ai-v4mapped*
        *ai-all*
        *ai-addrconfig*
        *ipproto-ip*
        *ipproto-tcp*
        *ipproto-udp*
        *msg-peek*
        *msg-oob*
        *msg-waitall*
        *shut-rd*
        *shut-wr*
        *shut-rdwr*)
    (begin
        (define make-client-socket
           (case-lambda
               ((node svc)
                   (make-client-socket node svc *af-inet* *sock-stream*
                           (socket-merge-flags *ai-v4mapped* *ai-addrconfig*) *ipproto-ip*))
               ((node svc fam)
                   (make-client-socket node svc fam *sock-stream*
                           (socket-merge-flags *ai-v4mapped* *ai-addrconfig*) *ipproto-ip*))
               ((node svc fam type)
                   (make-client-socket node svc fam type
                           (socket-merge-flags *ai-v4mapped* *ai-addrconfig*) *ipproto-ip*))
               ((node svc fam type flags)
                   (make-client-socket node svc fam type flags *ipproto-ip*))
               ((node svc fam type flags prot)
                   (let ((s (make-socket fam type prot)))
                       (connect-socket s node svc fam type flags prot)
                       s))))

        (define make-server-socket
            (case-lambda
                ((svc) (make-server-socket svc *af-inet* *sock-stream* *ipproto-ip*))
                ((svc fam) (make-server-socket svc fam *sock-stream* *ipproto-ip*))
                ((svc fam type) (make-server-socket svc fam type *ipproto-ip*))
                ((svc fam type prot)
                    (let ((s (make-socket fam type prot)))
                        (bind-socket s "" svc fam type prot)
                        (listen-socket s)
                        s))))

        (define socket-send
            (case-lambda
                ((socket bv) (send-socket socket bv 0))
                ((socket bv flags) (send-socket socket bv flags))))

        (define socket-recv
            (case-lambda
                ((socket size) (recv-socket socket size 0))
                ((socket size flags) (recv-socket socket size flags))))

        (define (socket-close socket) (close-port socket))

        (define (socket-input-port socket) socket)

        (define (socket-output-port socket) socket)

        (define (call-with-socket socket proc)
            (let-values ((results (proc socket)))
                (close-port socket)
                (apply values results)))))


(define-library (srfi 60)
    (import (foment base))
    (export
        bitwise-and
        (rename bitwise-and logand)
        bitwise-ior
        (rename bitwise-ior logior)
        bitwise-xor
        (rename bitwise-xor logxor)
        bitwise-not
        (rename bitwise-not lognot)
        bitwise-merge
        (rename bitwise-merge bitwise-if)
        any-bits-set?
        (rename any-bits-set? logtest)
        bit-count
        (rename bit-count logcount)
        integer-length
        first-set-bit
        (rename first-set-bit log2-binary-factors)
        bit-set?
        (rename bit-set? logbit?)
        copy-bit
        bit-field
        copy-bit-field
        arithmetic-shift
        (rename arithmetic-shift ash)
        rotate-bit-field
        reverse-bit-field
        integer->list
        list->integer
        booleans->integer
        )
    (begin
        (define (bitwise-merge mask n0 n1)
            (bitwise-ior (bitwise-and mask n0) (bitwise-and (bitwise-not mask) n1)))
        (define (any-bits-set? n1 n2)
            (not (zero? (bitwise-and n1 n2))))
        (define (bit-set? index n)
            (any-bits-set? (expt 2 index) n))
        (define (copy-bit idx to bit)
            (if bit
                (bitwise-ior to (arithmetic-shift 1 idx))
                (bitwise-and to (bitwise-not (arithmetic-shift 1 idx)))))
        (define (bit-field n start end)
            (bitwise-and (bitwise-not (arithmetic-shift -1 (- end start)))
                (arithmetic-shift n (- start))))
        (define (copy-bit-field to from start end)
            (bitwise-merge
                (arithmetic-shift (bitwise-not (arithmetic-shift -1 (- end start))) start)
                (arithmetic-shift from start) to))
        (define (rotate-bit-field n count start end)
            (define width (- end start))
            (set! count (modulo count width))
            (let ((mask (bitwise-not (arithmetic-shift -1 width))))
                (define zn (bitwise-and mask (arithmetic-shift n (- start))))
                (bitwise-ior
                    (arithmetic-shift
                        (bitwise-ior
                            (bitwise-and mask (arithmetic-shift zn count))
                            (arithmetic-shift zn (- count width)))
                        start)
                   (bitwise-and (bitwise-not (arithmetic-shift mask start)) n))))
        (define (bit-reverse k n)
            (do ((m (if (negative? n) (bitwise-not n) n) (arithmetic-shift m -1))
                    (k (+ -1 k) (+ -1 k))
                   (rvs 0 (bitwise-ior (arithmetic-shift rvs 1) (bitwise-and 1 m))))
                ((negative? k) (if (negative? n) (bitwise-not rvs) rvs))))
        (define (reverse-bit-field n start end)
            (define width (- end start))
            (let ((mask (bitwise-not (arithmetic-shift -1 width))))
                (define zn (bitwise-and mask (arithmetic-shift n (- start))))
                (bitwise-ior
                    (arithmetic-shift (bit-reverse width zn) start)
                    (bitwise-and (bitwise-not (arithmetic-shift mask start)) n))))
        (define (integer->list k . len)
            (if (null? len)
                (do ((k k (arithmetic-shift k -1))
                    (lst '() (cons (odd? k) lst)))
                    ((<= k 0) lst))
                (do ((idx (+ -1 (car len)) (+ -1 idx))
                    (k k (arithmetic-shift k -1))
                    (lst '() (cons (odd? k) lst)))
                   ((negative? idx) lst))))
        (define (list->integer bools)
            (do ((bs bools (cdr bs))
                (acc 0 (+ acc acc (if (car bs) 1 0))))
                ((null? bs) acc)))
        (define (booleans->integer . bools)
            (list->integer bools))))
