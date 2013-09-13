(define-library (foment base)
    (import (foment bedrock))
    (export quote lambda if set! let letrec letrec* let* let-values let*-values
        let-syntax letrec-syntax case or begin do syntax-rules syntax-error
        include include-ci cond-expand case-lambda quasiquote define define-values define-syntax
        import define-library else => unquote unquote-splicing
        when unless and cond call-with-values make-parameter parameterize map for-each eval
        interaction-environment boolean? not error eq? eqv? equal? command-line
        write display write-shared display-shared write-simple display-simple
        + * - / = < > <= >= zero? positive? negative? odd? even? exact-integer? expt abs sqrt
        number->string pair? cons car cdr length
        set-car! set-cdr! list null? append reverse list-ref map-car map-cdr string=? string?
        vector? make-vector vector-ref vector-set! list->vector values apply
        call-with-current-continuation (rename call-with-current-continuation call/cc) procedure?
        string->symbol caar cadr cdar cddr newline dynamic-wind with-exception-handler
        raise-continuable raise)
    (export
        syntax unsyntax eq-hash eqv-hash
        equal-hash full-error loaded-libraries library-path full-command-line
        open-output-string get-output-string write-pretty display-pretty string-hash
        with-continuation-mark call-with-continuation-prompt abort-current-continuation
        default-prompt-tag (rename default-prompt-tag default-continuation-prompt-tag)
        default-prompt-handler current-continuation-marks)
    (begin
        (define (caar pair) (car (car pair)))
        (define (cadr pair) (car (cdr pair)))
        (define (cdar pair) (cdr (car pair)))
        (define (cddr pair) (cdr (cdr pair)))

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

        (define (map proc . lists)
            (define (map proc lists)
                (let ((cars (map-car lists))
                        (cdrs (map-cdr lists)))
                    (if (null? cars)
                        '()
                        (cons (apply proc cars) (map proc cdrs)))))
            (if (null? lists)
                (full-error 'assertion-violation 'map "map: expected at least one argument")
                (map proc lists)))

        (define (for-each proc . lists)
            (define (for-each proc lists)
                (let ((cars (map-car lists))
                        (cdrs (map-cdr lists)))
                    (if (null? cars)
                        #f
                        (begin (apply proc cars) (for-each proc cdrs)))))
            (if (null? lists)
                (full-error 'assertion-violation 'for-each
                        "for-each: expected at least one argument")
                (for-each proc lists)))

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
                                    (eq-hashtable-set (%parameters) parameter
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
                        (eq-hashtable-set (%parameters) p
                                (cons val (eq-hashtable-ref (%parameters) p '())))
                        (before-parameterize (cdr params) (cdr vals))))))

        (define (after-parameterize params)
            (if (not (null? params))
                (begin
                    (eq-hashtable-set (%parameters) (car params)
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

        (define (default-prompt-tag)
            (%default-prompt-tag))

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
                    (full-error 'assertion-violation 'abort-current-continuation-tag
                            "abort-current-continuation-tag: expected a prompt tag" tag))
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
    ))

(define-library (scheme base)
    (import (foment base))
    (export quote lambda if set! let letrec letrec* let* let-values let*-values
        let-syntax letrec-syntax case or begin do syntax-rules syntax-error
        include include-ci cond-expand case-lambda quasiquote define define-values define-syntax
        import define-library else => unquote unquote-splicing
        when unless and cond call-with-values make-parameter parameterize map for-each eval
        interaction-environment boolean? not error eq? eqv? equal? command-line
        write display write-shared display-shared write-simple display-simple
        + * - / = < > <= >= zero? positive? negative? odd? even? exact-integer? expt abs sqrt
        number->string pair? cons car cdr length
        set-car! set-cdr! list null? append reverse list-ref map-car map-cdr string=? string?
        vector? make-vector vector-ref vector-set! list->vector values apply
        call/cc (rename call/cc call-with-current-continuation) procedure? string->symbol
        caar cadr cdar cddr newline dynamic-wind with-exception-handler raise-continuable raise))
