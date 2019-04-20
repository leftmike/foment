(define-library (scheme comparator)
    (aka (srfi 128))
    (import (foment base))
    (export
        comparator?
        comparator-ordered?
        comparator-hashable?
        make-comparator
        make-pair-comparator
        make-list-comparator
        make-vector-comparator
        make-eq-comparator
        make-eqv-comparator
        make-equal-comparator
        boolean-hash
        char-hash
        char-ci-hash
        string-hash
        string-ci-hash
        symbol-hash
        number-hash
        hash-bound-parameter
        hash-bound
        hash-salt-parameter
        hash-salt
        make-default-comparator
        default-hash
        comparator-register-default!
        comparator-type-test-predicate
        comparator-equality-predicate
        comparator-ordering-predicate
        comparator-hash-function
        comparator-test-type
        comparator-check-type
        comparator-hash
        =?
        <?
        >?
        <=?
        >=?
        comparator-if<=>
        )
    (begin
        (define-syntax hash-bound (syntax-rules () ((hash-bound) (hash-bound-parameter))))

        (define-syntax hash-salt (syntax-rules () ((hash-salt) (hash-salt-parameter))))

        (define (hash-accumulate . hashes)
            (define (accumulate hashes)
                (if (pair? hashes)
                    (+ (modulo (* (accumulate (cdr hashes)) 33) (hash-bound)) (car hashes))
                    (hash-salt)))
            (accumulate hashes))

        (define (make-pair=? car-comp cdr-comp)
            (lambda (obj1 obj2)
                (and ((comparator-equality-predicate car-comp) (car obj1) (car obj2))
                    ((comparator-equality-predicate cdr-comp) (cdr obj1) (cdr obj2)))))

        (define (make-pair<? car-comp cdr-comp)
            (lambda (obj1 obj2)
                (if ((comparator-equality-predicate car-comp) (car obj1) (car obj2))
                    ((comparator-ordering-predicate cdr-comp) (cdr obj1) (cdr obj2))
                    ((comparator-ordering-predicate car-comp) (car obj1) (car obj2)))))

        (define (make-pair-hash car-comp cdr-comp)
            (lambda (obj . arg)
                (hash-accumulate ((comparator-hash-function car-comp) (car obj))
                                 ((comparator-hash-function cdr-comp) (cdr obj)))))

        (define (make-pair-comparator car-comp cdr-comp)
            (make-comparator
                (lambda (obj)
                    (and
                        (pair? obj)
                        ((comparator-type-test-predicate car-comp) (car obj))
                        ((comparator-type-test-predicate cdr-comp) (cdr obj))))
                (make-pair=? car-comp cdr-comp)
                (make-pair<? car-comp cdr-comp)
                (make-pair-hash car-comp cdr-comp)))

        (define (make-list-comparator elem-comp type-test empty? head tail)
            (make-comparator
                (lambda (obj)
                    (define (list-elem-type-test elem-type-test obj)
                        (if (empty? obj)
                            #t
                            (if (not (elem-type-test (head obj)))
                                #f
                                (list-elem-type-test elem-type-test (tail obj)))))
                    (and (type-test obj)
                        (list-elem-type-test (comparator-type-test-predicate elem-comp) obj)))
                (lambda (obj1 obj2)
                    (define (list-elem=? elem=? obj1 obj2)
                        (cond
                            ((and (empty? obj1) (empty? obj2) #t))
                            ((empty? obj1) #f)
                            ((empty? obj2) #f)
                            ((elem=? (head obj1) (head obj2))
                                (list-elem=? elem=? (tail obj1) (tail obj2)))
                            (else #f)))
                    (list-elem=? (comparator-equality-predicate elem-comp) obj1 obj2))
                (lambda (obj1 obj2)
                    (define (list-elem<? elem=? elem<? obj1 obj2)
                        (cond
                            ((and (empty? obj1) (empty? obj2)) #f)
                            ((empty? obj1) #t)
                            ((empty? obj2) #f)
                            ((elem=? (head obj1) (head obj2))
                                (list-elem<? elem=? elem<? (tail obj1) (tail obj2)))
                            ((elem<? (head obj1) (head obj2)) #t)
                            (else #f)))
                    (list-elem<? (comparator-equality-predicate elem-comp)
                            (comparator-ordering-predicate elem-comp) obj1 obj2))
                (lambda (obj . arg)
                    (define (list-hash elem-hash hash obj)
                        (if (empty? obj)
                            hash
                            (list-hash elem-hash (hash-accumulate (elem-hash (head obj)) hash)
                                    (tail obj))))
                    (list-hash (comparator-hash-function elem-comp) (hash-salt) obj))))

        (define (make-vector=? elem-comp length ref)
            (lambda (obj1 obj2)
                (define (vector=? elem=? idx len)
                    (if (< idx len)
                        (if (elem=? (ref obj1 idx) (ref obj2 idx))
                            (vector=? elem=? (+ idx 1) len)
                            #f)
                        #t))
                (and (= (length obj1) (length obj2))
                    (vector=? (comparator-equality-predicate elem-comp) 0 (length obj1)))))

        (define (make-vector<? elem-comp length ref)
            (lambda (obj1 obj2)
                (define (vector<? elem=? elem<? idx len)
                    (if (< idx len)
                        (if (elem=? (ref obj1 idx) (ref obj2 idx))
                            (vector<? elem=? elem<? (+ idx 1) len)
                            (elem<? (ref obj1 idx) (ref obj2 idx)))
                        #f))
                (if (< (length obj1) (length obj2))
                    #t
                    (if (> (length obj1) (length obj2))
                        #f
                        (vector<? (comparator-equality-predicate elem-comp)
                                (comparator-ordering-predicate elem-comp) 0 (length obj1))))))

        (define (make-vector-hash elem-comp length ref)
            (lambda (obj . arg)
                (define (vector-hash elem-hash hash idx len)
                    (if (< idx len)
                        (vector-hash elem-hash (hash-accumulate (elem-hash (ref obj idx)) hash)
                                (+ idx 1) len)
                        hash))
                (vector-hash (comparator-hash-function elem-comp) (hash-salt) 0
                        (length obj))))

        (define (make-vector-comparator elem-comp type-test length ref)
            (make-comparator
                (lambda (obj)
                    (define (vector-type-test elem-type-test idx len)
                        (if (< idx len)
                            (if (not (elem-type-test (ref obj idx)))
                                #f
                                (vector-type-test elem-type-test (+ idx 1) len))
                            #t))
                    (and (type-test obj)
                        (vector-type-test
                                (comparator-type-test-predicate elem-comp) 0 (length obj))))
                (make-vector=? elem-comp length ref)
                (make-vector<? elem-comp length ref)
                (make-vector-hash elem-comp length ref)))

        (define char-comparator
            (make-comparator char? char=? char<? char-hash))

        (define empty-list-comparator
            (make-comparator null?
                (lambda (obj1 obj2) #t)
                (lambda (obj1 obj2) #f)
                (lambda (obj) 0)))

        (define eof-comparator
            (make-comparator eof-object?
                (lambda (obj1 obj2) #t)
                (lambda (obj1 obj2) #f)
                (lambda (obj) 0)))

        (define boolean-comparator
            (make-comparator boolean? eq? (lambda (obj1 obj2) (and (not obj1) obj2)) boolean-hash))

        (define number-comparator
            (make-comparator number? = < number-hash))

        (define (make-box-comparator elem-comp)
            (make-comparator box?
                (lambda (obj1 obj2)
                    ((comparator-equality-predicate elem-comp) (unbox obj1) (unbox obj2)))
                (lambda (obj1 obj2)
                    ((comparator-ordering-predicate elem-comp) (unbox obj1) (unbox obj2)))
                (lambda (obj)
                     ((comparator-hash-function elem-comp) (unbox obj)))))

        (define string-comparator
            (make-comparator string? string=? string<? string-hash))

        (define symbol-comparator
            (make-comparator symbol? eq?
                    (lambda (obj1 obj2) (string<? (symbol->string obj1) (symbol->string obj2)))
                    symbol-hash))

        (define no-comparator
            (make-comparator
                    (lambda (obj)
                       (full-error 'assertion-violation 'no-comparator #f
                               "no-comparator: no comparator for object type" obj))
                    (lambda (obj1 obj2)
                       (full-error 'assertion-violation 'no-comparator #f
                               "no-comparator: no comparator for object type" obj1 obj2))
                    (lambda (obj1 obj2)
                       (full-error 'assertion-violation 'no-comparator #f
                               "no-comparator: no comparator for object type" obj1 obj2))
                    (lambda (obj)
                       (full-error 'assertion-violation 'no-comparator #f
                               "no-comparator: no comparator for object type" obj))))

        (define (standard-comparator-register! standard-comp type-comp . dflt-flag)
            (let ((tlst (lookup-type-tags (comparator-type-test-predicate type-comp)))
                    (ctx (comparator-context standard-comp)))
                (if (not (vector? ctx))
                    (full-error 'assertion-violation 'standard-comparator-register! #f
    "standard-comparator-register!: expected a standard or default comparator"))
                (if (and (pair? dflt-flag) (car dflt-flag))
                    (for-each (lambda (tag)
                                  (if (not (eq? (vector-ref ctx tag) no-comparator))
                                      (full-error 'assertion-violation
                                              'standard-comparator-register! #f
    "standard-comparator-register!: attempt to override comparator"))) tlst))
                (for-each (lambda (tag)
                              (vector-set! ctx tag type-comp)) tlst)))

        (define (make-standard-comparator)
            (let ((vec (make-vector 64 no-comparator)))
                (define (standard-type-test obj)
                    ((comparator-type-test-predicate (vector-ref vec (object-type-tag obj))) obj))
                (define (standard=? obj1 obj2)
                    (let ((tag1 (object-type-tag obj1))
                            (tag2 (object-type-tag obj2)))
                        (if (= tag1 tag2)
                            ((comparator-equality-predicate (vector-ref vec tag1)) obj1 obj2)
                            #f)))
                (define (standard<? obj1 obj2)
                    (let ((tag1 (object-type-tag obj1))
                            (tag2 (object-type-tag obj2)))
                        (if (< tag1 tag2)
                            #t
                            (if (= tag1 tag2)
                                ((comparator-ordering-predicate (vector-ref vec tag1)) obj1 obj2)
                                #f))))
                (define (standard-hash obj . arg)
                    ((comparator-hash-function (vector-ref vec (object-type-tag obj))) obj))
                (let ((comp
                        (make-comparator standard-type-test standard=? standard<? standard-hash)))
                    (comparator-context-set! comp vec)
                    (standard-comparator-register! comp char-comparator)
                    (standard-comparator-register! comp empty-list-comparator)
                    (standard-comparator-register! comp eof-comparator)
                    (standard-comparator-register! comp boolean-comparator)
                    (standard-comparator-register! comp number-comparator)
                    (standard-comparator-register! comp (make-box-comparator comp))
                    (standard-comparator-register! comp
                            (make-comparator pair?
                                    (make-pair=? comp comp)
                                    (make-pair<? comp comp)
                                    (make-pair-hash comp comp)))
                    (standard-comparator-register! comp string-comparator)
                    (standard-comparator-register! comp
                            (make-comparator vector?
                                    (make-vector=? comp vector-length vector-ref)
                                    (make-vector<? comp vector-length vector-ref)
                                    (make-vector-hash comp vector-length vector-ref)))
                    (standard-comparator-register! comp
                            (make-comparator bytevector?
                                    (make-vector=? comp bytevector-length bytevector-u8-ref)
                                    (make-vector<? comp bytevector-length bytevector-u8-ref)
                                    (make-vector-hash comp bytevector-length bytevector-u8-ref)))
                    (standard-comparator-register! comp symbol-comparator)
                    comp)))

        (define default-comparator (make-standard-comparator))
        (define (make-default-comparator) default-comparator)

        (define default-hash (comparator-hash-function default-comparator))

        (define (comparator-register-default! comparator)
            (standard-comparator-register! default-comparator comparator #t))

        (define eq-comparator (make-comparator (lambda (obj) #t) eq? #f default-hash))
        (define (make-eq-comparator) eq-comparator)

        (define eqv-comparator (make-comparator (lambda (obj) #t) eqv? #f default-hash))
        (define (make-eqv-comparator) eqv-comparator)

        (define equal-comparator (make-comparator (lambda (obj) #t) equal? #f default-hash))
        (define (make-equal-comparator) equal-comparator)

        (define (comparator-test-type comparator obj)
            ((comparator-type-test-predicate comparator) obj))

        (define (comparator-check-type comparator obj)
            (if ((comparator-type-test-predicate comparator) obj)
                #t
                (full-error 'assertion-violation 'comparator-check-type #f
                            "comparator-check-type: type test failed" comparator obj)))

        (define (comparator-hash comparator obj)
            ((comparator-hash-function comparator) obj))

        (define (=? comparator obj1 obj2 . lst)
            (let ((=-2? (comparator-equality-predicate comparator)))
                (define (=-n? obj1 obj2 lst)
                    (if (=-2? obj1 obj2)
                        (if (pair? lst)
                            (=-n? obj2 (car lst) (cdr lst))
                            #t)
                        #f))
                (=-n? obj1 obj2 lst)))

        (define (<? comparator obj1 obj2 . lst)
            (let ((<-2? (comparator-ordering-predicate comparator)))
                (define (<-n? obj1 obj2 lst)
                    (if (<-2? obj1 obj2)
                        (if (pair? lst)
                            (<-n? obj2 (car lst) (cdr lst))
                            #t)
                        #f))
                (<-n? obj1 obj2 lst)))

        (define (>? comparator obj1 obj2 . lst)
            (let ((<-2? (comparator-ordering-predicate comparator)))
                (define (>-n? obj1 obj2 lst)
                    (if (<-2? obj2 obj1)
                        (if (pair? lst)
                            (>-n? obj2 (car lst) (cdr lst))
                            #t)
                        #f))
                (>-n? obj1 obj2 lst)))

        (define (<=? comparator obj1 obj2 . lst)
            (let ((<-2? (comparator-ordering-predicate comparator)))
                (define (<=-n? obj1 obj2 lst)
                    (if (not (<-2? obj2 obj1))
                        (if (pair? lst)
                            (<=-n? obj2 (car lst) (cdr lst))
                            #t)
                        #f))
                (<=-n? obj1 obj2 lst)))

        (define (>=? comparator obj1 obj2 . lst)
            (let ((<-2? (comparator-ordering-predicate comparator)))
                (define (>=-n? obj1 obj2 lst)
                    (if (not (<-2? obj1 obj2))
                        (if (pair? lst)
                            (>=-n? obj2 (car lst) (cdr lst))
                            #t)
                        #f))
                (>=-n? obj1 obj2 lst)))

        (define-syntax comparator-if<=>
            (syntax-rules ()
                ((compartor-if<=> obj1 obj2 less-than equal-to greater-than)
                    (comparator-if<=> (make-default-comparator) obj1 obj2 less-than
                            equal-to greater-than))
                ((compartor-if<=> comparator obj1 obj2 less-than equal-to greater-than)
                    (if (=? comparator obj1 obj2)
                        equal-to
                        (if (<? comparator obj1 obj2)
                            less-than
                            greater-than)))))
    ))
