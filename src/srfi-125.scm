(define-library (scheme hash-table)
    (aka (srfi 125))
    (aka (srfi 69))
    (import (foment base))
    (import (scheme comparator))
    (export
        make-hash-table
        hash-table
        hash-table-unfold
        alist->hash-table
        hash-table?
        hash-table-contains?
        (rename hash-table-contains? hash-table-exists?)
        hash-table-empty?
        hash-table=?
        hash-table-mutable?
        hash-table-ref
        hash-table-ref/default
        hash-table-set!
        hash-table-delete!
        hash-table-intern!
        hash-table-update!
        hash-table-update!/default
        hash-table-pop!
        hash-table-clear!
        hash-table-size
        hash-table-keys
        hash-table-values
        hash-table-entries
        hash-table-find
        hash-table-count
        hash-table-map
        hash-table-for-each
        hash-table-walk
        hash-table-map!
        hash-table-map->list
        hash-table-fold
        hash-table-prune!
        hash-table-copy
        hash-table-empty-copy
        hash-table->alist
        hash-table-union!
        (rename hash-table-union! hash-table-merge!)
        hash-table-intersection!
        hash-table-difference!
        hash-table-xor!
        (rename default-hash hash)
        string-hash
        string-ci-hash
        (rename default-hash hash-by-identity)
        (rename %hash-table-equality-predicate hash-table-equivalence-function)
        hash-table-hash-function
        )
    (begin
        (define (make-hash-table arg . args)
            (define (make/functions ttp eqp hashfn args)
                (cond
                    ((eq? eqp eq?)
                            (%make-hash-table (if ttp ttp (lambda (obj) #t)) eq? eq-hash args))
                    ((eq? eqp eqv?)
                            (%make-hash-table (if ttp ttp (lambda (obj) #t)) eqv? eq-hash args))
                    ((eq? eqp equal?)
                            (%make-hash-table (if ttp ttp (lambda (obj) #t)) equal?
                                    default-hash args))
                    ((eq? eqp string=?)
                            (%make-hash-table (if ttp ttp string?) string=? string-hash args))
                    ((eq? eqp string-ci=?)
                            (%make-hash-table (if ttp ttp string?) string-ci=?
                                    string-ci-hash args))
                    ((not hashfn)
                            (full-error 'assertion-violation 'make-hash-table #f
                                    "make-hash-table: missing hash function"))
                    (else
                            (%make-hash-table (if ttp ttp (lambda (obj) #t)) eqp hashfn args))))
            (cond
                ((comparator? arg)
                    (make/functions (comparator-type-test-predicate arg)
                            (comparator-equality-predicate arg)
                            (comparator-hash-function arg) args))
                ((procedure? arg)
                    (if (and (pair? args) (procedure? (car args)))
                        (make/functions #f arg (car args) (cdr args))
                        (make/functions #f arg #f args)))
                (else
                    (full-error 'assertion-violation 'make-hash-table #f
                            "make-hash-table: expected comparator or procedure" arg))))

        (define (hash-table comparator . args)
            (let ((htbl (make-hash-table comparator)))
                (hash-table-set-n htbl args)
                (%hash-table-immutable! htbl)
                htbl))

        (define (hash-table-unfold stop? mapper successor seed comparator . args)
            (let ((htbl (apply make-hash-table comparator args)))
                (define (unfold seed)
                    (if (stop? seed)
                        htbl
                        (let-values (((key val) (mapper seed)))
                            (hash-table-set! htbl key val)
                            (unfold (successor seed)))))
                (unfold seed)))

        (define (alist->hash-table lst comparator . args)
            (let ((htbl (apply make-hash-table comparator args)))
                (for-each
                    (lambda (key-val)
                        (if (not (hash-table-contains? htbl (car key-val)))
                            (hash-table-set! htbl (car key-val) (cdr key-val))))
                    lst)
                htbl))

        (define unique (cons #f #f))
        (define (hash-table-contains? htbl key)
            (not (eq? (hash-table-ref/default htbl key unique) unique)))

        (define (hash-table-empty? htbl)
            (= (hash-table-size htbl) 0))

        (define (hash-table=? value-comparator htbl-1 htbl-2)
            (let ((eqp (comparator-equality-predicate value-comparator)))
                (if (or (not (= (hash-table-size htbl-1) (hash-table-size htbl-2)))
                        (not (eq? (%hash-table-equality-predicate htbl-1)
                                    (%hash-table-equality-predicate htbl-2))))
                    #f
                    (not (hash-table-find
                                (lambda (key val)
                                    (not (eqp val (hash-table-ref/default htbl-2 key unique))))
                                htbl-1 (lambda () #f))))))

        (define (hash-table-ref htbl key . optional)
            (let ((val (hash-table-ref/default htbl key unique)))
                (if (eq? val unique)
                    (if (pair? optional)
                        ((car optional))
                        (full-error 'assertion-violation 'hash-table-ref #f
                                "hash-table-ref: key not found and no failure specified" key))
                    (if (and (pair? optional) (pair? (cdr optional)))
                        ((cadr optional) val)
                        val))))

        (define (hash-table-ref/default htbl key default)
            (if (%eq-hash-table? htbl)
                (%hash-table-ref htbl key default)
                (let* ((buckets (%hash-table-buckets htbl))
                        (idx (modulo ((hash-table-hash-function htbl) key) (vector-length buckets)))
                        (eqp (%hash-table-equality-predicate htbl)))
                    (define (node-list-ref nlst)
                        (if (%hash-node? nlst)
                            (let ((val (%hash-node-value nlst)))
                                (if (and (eqp (%hash-node-key nlst) key)
                                            (not (%hash-node-broken? nlst)))
                                    val
                                    (node-list-ref (%hash-node-next nlst))))
                            default))
                    (node-list-ref (vector-ref buckets idx)))))

        (define (check-mutable htbl name)
            (if (not (hash-table-mutable? htbl))
                (full-error 'assertion-violation name #f
                        (string-append (symbol->string name) ": hash table is immutable") htbl)))

        (define (hash-table-set-1 htbl key val)
            (let* ((buckets (%hash-table-buckets htbl))
                    (eqp (%hash-table-equality-predicate htbl))
                    (hashfn (hash-table-hash-function htbl))
                    (hsh (hashfn key))
                    (idx (modulo hsh (vector-length buckets))))
                (define (node-list-set nlst)
                    (if (%hash-node? nlst)
                        (if (and (eqp (%hash-node-key nlst) key)
                                    (not (%hash-node-broken? nlst)))
                            (%hash-node-value-set! nlst val)
                            (node-list-set (%hash-node-next nlst)))
                        (begin
                            (vector-set! buckets idx
                                    (%make-hash-node htbl key val (vector-ref buckets idx) hsh))
                            (%hash-table-adjust! htbl (+ (hash-table-size htbl) 1)))))
                (node-list-set (vector-ref buckets idx))))

        (define (hash-table-set-n htbl args)
            (check-mutable htbl 'hash-table-set!)
            (if (and (pair? args) (pair? (cdr args)))
                (begin
                    (if (%eq-hash-table? htbl)
                        (%hash-table-set! htbl (car args) (cadr args))
                        (let ((exc (%hash-table-exclusive htbl)))
                            (if (exclusive? exc)
                                (with-exclusive exc (hash-table-set-1 htbl (car args) (cadr args)))
                                (hash-table-set-1 htbl (car args) (cadr args)))))
                    (hash-table-set-n htbl (cddr args)))
                (if (not (null? args))
                    (full-error 'assertion-violation 'hash-table-set! #f
                            "hash-table-set!: key specified but no value" args))))

        (define (hash-table-set! htbl . args)
            (hash-table-set-n htbl args))

        (define (hash-table-delete-1 htbl key)
            (let* ((buckets (%hash-table-buckets htbl))
                    (eqp (%hash-table-equality-predicate htbl))
                    (hashfn (hash-table-hash-function htbl))
                    (idx (modulo (hashfn key) (vector-length buckets)))
                    (nlst (vector-ref buckets idx)))
                (define (node-list-delete node)
                    (if (%hash-node? node)
                        (if (and (eqp (%hash-node-key node) key)
                                    (not (%hash-node-broken? node)))
                            (begin
                                (if (eq? nlst node)
                                    (vector-set! buckets idx (%hash-node-next nlst))
                                    (vector-set! buckets idx (%copy-hash-node-list htbl nlst node)))
                                (%hash-table-adjust! htbl (- (hash-table-size htbl) 1)))
                            (node-list-delete (%hash-node-next node)))))
                (node-list-delete nlst)))

        (define (hash-table-delete! htbl . keys)
            (define (delete-n keys)
                (if (pair? keys)
                    (begin
                        (if (%eq-hash-table? htbl)
                            (%hash-table-delete! htbl (car keys))
                            (let ((exc (%hash-table-exclusive htbl)))
                                (if (exclusive? exc)
                                    (with-exclusive exc (hash-table-delete-1 htbl (car keys)))
                                    (hash-table-delete-1 htbl (car keys)))))
                        (delete-n (cdr keys)))))
            (check-mutable htbl 'hash-table-delete!)
            (delete-n keys))

        (define (hash-table-intern! htbl key failure)
            (let ((ret (hash-table-ref/default htbl key unique)))
                (if (eq? ret unique)
                    (let ((val (failure)))
                        (hash-table-set! htbl key val)
                        val)
                    ret)))

        (define (hash-table-update! htbl key updater . optional)
            (hash-table-set! htbl key (updater (apply hash-table-ref htbl key optional))))

        (define (hash-table-update!/default htbl key updater default)
            (hash-table-set! htbl key (updater (hash-table-ref/default htbl key default))))

        (define (hash-table-pop! htbl)
            (check-mutable htbl 'hash-table-pop!)
            (let* ((node (%hash-table-pop! htbl))
                    (key (%hash-node-key node))
                    (val (%hash-node-value node)))
                (if (%hash-node-broken? node)
                    (hash-table-pop! htbl)
                    (values key val))))

        (define (hash-table-clear! htbl)
            (check-mutable htbl 'hash-table-clear!)
            (%hash-table-clear! htbl))

        (define (hash-table-entries htbl)
            (let ((entries (%hash-table-entries htbl)))
                (values (car entries) (cdr entries))))

        (define (hash-table-find proc htbl failure)
            (define (find-entries keys values)
                (if (and (pair? keys) (pair? values))
                    (let ((ret (proc (car keys) (car values))))
                        (if ret
                            ret
                            (find-entries (cdr keys) (cdr values))))
                    (failure)))
            (let ((entries (%hash-table-entries htbl)))
                (find-entries (car entries) (cdr entries))))

        (define (hash-table-count pred htbl)
            (hash-table-fold (lambda (key val cnt) (if (pred key val) (+ cnt 1) cnt)) 0 htbl))

        (define (hash-table-map proc comparator htbl)
            (hash-table-fold (lambda (key val nhtbl) (hash-table-set! nhtbl key (proc val)) nhtbl)
                    (make-hash-table comparator) htbl))

        (define (hash-table-for-each proc htbl)
            (hash-table-fold (lambda (key val ignore) (proc key val) (no-value)) (no-value) htbl))

        (define (hash-table-walk htbl proc)
            (hash-table-for-each proc htbl))

        (define (hash-table-map! proc htbl)
            (define (map-entries keys values)
                (if (and (pair? keys) (pair? values))
                    (begin
                        (hash-table-set-1 htbl (car keys) (proc (car keys) (car values)))
                        (map-entries (cdr keys) (cdr values)))))
            (check-mutable htbl 'hash-table-map!)
            (let ((entries (%hash-table-entries htbl))
                    (exc (%hash-table-exclusive htbl)))
                (if (exclusive? exc)
                    (with-exclusive exc (map-entries (car entries) (cdr entries)))
                    (map-entries (car entries) (cdr entries)))))

        (define (hash-table-map->list proc htbl)
            (hash-table-fold (lambda (key val lst) (cons (proc key val) lst)) '() htbl))

        (define (eq-hash-table-fold proc seed htbl)
            (define (fold-entries keys values seed)
                (if (and (pair? keys) (pair? values))
                    (fold-entries (cdr keys) (cdr values) (proc (car keys) (car values) seed))
                    seed))
            (let ((entries (%hash-table-entries htbl)))
                (fold-entries (car entries) (cdr entries) seed)))

        (define (%hash-table-fold proc seed htbl)
            (let* ((buckets (%hash-table-buckets htbl))
                    (len (vector-length buckets)))
              (define (fold-buckets idx seed)
                  (if (< idx len)
                      (fold-buckets (+ idx 1) (fold-node-list (vector-ref buckets idx) seed))
                      seed))
              (define (fold-node-list nlst seed)
                  (if (%hash-node? nlst)
                      (let ((key (%hash-node-key nlst))
                              (val (%hash-node-value nlst)))
                          (if (%hash-node-broken? nlst)
                              (fold-node-list (%hash-node-next nlst) seed)
                              (fold-node-list (%hash-node-next nlst) (proc key val seed))))
                      seed))
              (fold-buckets 0 seed)))

        (define (hash-table-fold proc seed htbl)
            (if (hash-table? proc)
                (hash-table-fold seed htbl proc) ;; SRFI-69, deprecated.
                (if (%eq-hash-table? htbl)
                    (eq-hash-table-fold proc seed htbl)
                    (%hash-table-fold proc seed htbl))))

        (define (hash-table-prune! proc htbl)
            (define (prune-entries keys values)
                (if (and (pair? keys) (pair? values))
                    (begin
                        (if (proc (car keys) (car values))
                            (hash-table-delete-1 htbl (car keys)))
                        (prune-entries (cdr keys) (cdr values)))))
            (check-mutable htbl 'hash-table-prune!)
            (let ((entries (%hash-table-entries htbl))
                    (exc (%hash-table-exclusive htbl)))
                (if (exclusive? exc)
                    (with-exclusive exc (prune-entries (car entries) (cdr entries)))
                    (prune-entries (car entries) (cdr entries)))))

        (define (hash-table-copy htbl . mutable)
            (let ((nhtbl (%hash-table-copy htbl)))
                (if (or (null? mutable) (not (car mutable)))
                    (%hash-table-immutable! nhtbl))
                nhtbl))

        (define (hash-table-union! htbl-1 htbl-2)
            (hash-table-for-each
                    (lambda (key val)
                        (if (not (hash-table-contains? htbl-1 key))
                             (hash-table-set! htbl-1 key val))) htbl-2)
            htbl-1)

        (define (hash-table-intersection! htbl-1 htbl-2)
            (hash-table-for-each
                    (lambda (key val)
                        (if (not (hash-table-contains? htbl-2 key))
                            (hash-table-delete! htbl-1 key))) htbl-1)
            htbl-1)

        (define (hash-table-difference! htbl-1 htbl-2)
            (hash-table-for-each
                    (lambda (key val)
                        (if (hash-table-contains? htbl-2 key)
                            (hash-table-delete! htbl-1 key))) htbl-1)
            htbl-1)

        (define (hash-table-xor! htbl-1 htbl-2)
            (hash-table-for-each
                    (lambda (key val)
                        (if (hash-table-contains? htbl-1 key)
                           (hash-table-delete! htbl-1 key)
                           (hash-table-set! htbl-1 key val))) htbl-2)
            htbl-1)
    ))
