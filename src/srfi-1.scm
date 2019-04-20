(define-library (scheme list)
    (aka (srfi 1))
    (import (foment base))
    (export
        cons
        list
        xcons
        cons*
        make-list
        list-tabulate
        list-copy
        circular-list
        iota
        pair?
        null?
        (rename list? proper-list?)
        circular-list?
        dotted-list?
        not-pair?
        null-list?
        list=
        car
        cdr
        caar
        cadr
        cdar
        cddr
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
        cddddr
        list-ref
        first
        second
        third
        fourth
        fifth
        sixth
        seventh
        eighth
        ninth
        tenth
        car+cdr
        take
        drop
        take-right
        drop-right
        take!
        drop-right!
        split-at
        split-at!
        last
        last-pair
        length
        length+
        append
        append!
        concatenate
        concatenate!
        reverse
        reverse!
        append-reverse
        append-reverse!
        zip
        unzip1
        unzip2
        unzip3
        unzip4
        unzip5
        count
        map
        for-each
        fold
        fold-right
        pair-fold
        pair-fold-right
        reduce
        reduce-right
        unfold
        unfold-right
        append-map
        append-map!
        (rename map map!)
        (rename map map-in-order)
        pair-for-each
        filter-map
        filter
        partition
        remove
        (rename filter filter!)
        (rename partition partition!)
        (rename remove remove!)
        member
        memq
        memv
        find
        find-tail
        take-while
        (rename take-while take-while!)
        drop-while
        span
        (rename span span!)
        break
        (rename break break!)
        any
        every
        list-index
        delete
        delete-duplicates
        (rename delete delete!)
        (rename delete-duplicates delete-duplicates!)
        assoc
        assq
        assv
        alist-cons
        alist-copy
        alist-delete
        (rename alist-delete alist-delete!)
        lset<=
        lset=
        lset-adjoin
        lset-union
        (rename lset-union lset-union!)
        lset-intersection
        (rename lset-intersection lset-intersection!)
        lset-difference
        (rename lset-difference lset-difference!)
        lset-xor
        (rename lset-xor lset-xor!)
        lset-diff+intersection
        (rename lset-diff+intersection lset-diff+intersection!)
        set-car!
        set-cdr!)
    (begin
        (define (xcons obj1 obj2) (cons obj2 obj1))

        (define (cons* obj . lst)
            (define (cons* obj lst)
                (if (pair? lst)
                    (cons obj (cons* (car lst) (cdr lst)))
                    obj))
            (cons* obj lst))

        (define (list-tabulate n proc)
            (define (tabulate i)
                (if (< i n)
                    (cons (proc i) (tabulate (+ i 1)))
                    '()))
            (tabulate 0))

        (define (circular-list obj . lst)
            (let ((ret (cons obj lst)))
                (set-cdr! (last-pair ret) ret)
                ret))

        (define iota
            (case-lambda
                ((count) (%iota count 0 1))
                ((count start) (%iota count start 1))
                ((count start step) (%iota count start step))))

        (define (%iota count value step)
            (if (> count 0)
                (cons value (%iota (- count 1) (+ value step) step))
                '()))

        (define (null-list? obj)
            (not (pair? obj)))

        (define (not-pair? obj)
            (not (pair? obj)))

        (define (list= elt= . lists)
            (define (two-list= lst1 lst2)
                (if (null? lst1)
                    (if (null? lst2)
                        #t
                        #f)
                    (if (null? lst2)
                        #f
                        (if (elt= (car lst1) (car lst2))
                            (two-list= (cdr lst1) (cdr lst2))
                            #f))))
            (define (list-of-lists= lst lists)
                (if (null? lists)
                    #t
                    (if (not (two-list= lst (car lists)))
                        #f
                        (list-of-lists= (car lists) (cdr lists)))))
            (if (pair? lists)
                (list-of-lists= (car lists) (cdr lists))
                #t))

        (define first car)

        (define (second lst)
            (list-ref lst 1))

        (define (third lst)
            (list-ref lst 2))

        (define (fourth lst)
            (list-ref lst 3))

        (define (fifth lst)
            (list-ref lst 4))

        (define (sixth lst)
            (list-ref lst 5))

        (define (seventh lst)
            (list-ref lst 6))

        (define (eighth lst)
            (list-ref lst 7))

        (define (ninth lst)
            (list-ref lst 8))

        (define (tenth lst)
            (list-ref lst 9))

        (define (car+cdr pair)
            (values (car pair) (cdr pair)))

        (define (take lst k)
            (if (> k 0)
                (cons (car lst) (take (cdr lst) (- k 1)))
                '()))

        (define (drop lst k)
            (if (> k 0)
                (drop (cdr lst) (- k 1))
                lst))

        (define (take-right lst k)
            (drop lst (- (length+ lst) k)))

        (define (drop-right lst k)
            (take lst (- (length+ lst) k)))

        (define (take! lst k)
            (if (> k 0)
                (begin
                    (set-cdr! (drop lst (- k 1)) '())
                    lst)
                '()))

        (define (drop-right! lst k)
            (take! lst (- (length+ lst) k)))

        (define (split-at lst k)
            (define (split pre suf k)
                (if (> k 0)
                    (split (cons (car suf) pre) (cdr suf) (- k 1))
                    (values (reverse pre) suf)))
            (split '() lst k))

        (define (split-at! lst k)
            (if (> k 0)
                (let* ((prev (drop lst (- k 1)))
                        (suf (cdr prev)))
                    (set-cdr! prev '())
                    (values lst suf))
                (values '() lst)))

        (define (last lst) (car (last-pair lst)))

        (define (last-pair lst)
            (if (not (pair? (cdr lst)))
                lst
                (last-pair (cdr lst))))

        (define (append! . lsts)
            (define (from-right lsts)
                (if (null? (cdr lsts))
                    (car lsts)
                    (let ((lst (car lsts))
                            (ret (from-right (cdr lsts))))
                        (if (null? lst)
                            ret
                            (begin
                                (set-cdr! (last-pair lst) ret)
                                lst)))))
            (if (null? lsts)
                '()
                (from-right lsts)))

        (define (concatenate lsts)
            (apply append lsts))

        (define (concatenate! lsts)
            (apply append! lsts))

        (define (append-reverse head tail)
            (if (null? head)
                tail
                (append-reverse (cdr head) (cons (car head) tail))))

        (define (append-reverse! head tail)
            (if (null? head)
                tail
                (let ((ret (reverse! head)))
                    (set-cdr! head tail)
                    ret)))

        (define (zip lst . lsts)
            (apply map list lst lsts))

        (define (unzip1 lsts)
            (map car lsts))

        (define (unzip2 lsts)
            (values (map car lsts) (map cadr lsts)))

        (define (unzip3 lsts)
            (values (map car lsts) (map cadr lsts) (map caddr lsts)))

        (define (unzip4 lsts)
            (values (map car lsts) (map cadr lsts) (map caddr lsts) (map cadddr lsts)))

        (define (unzip5 lsts)
            (values (map car lsts) (map cadr lsts) (map caddr lsts) (map cadddr lsts)
                    (map fifth lsts)))

        (define (count pred lst . lsts)
            (define (count-1 lst cnt)
                (if (null? lst)
                    cnt
                    (count-1 (cdr lst) (if (pred (car lst)) (+ cnt 1) cnt))))
            (define (count-n lsts cnt)
                (if (every-1 pair? lsts)
                    (count-n (map cdr lsts) (if (apply pred (map car lsts)) (+ cnt 1) cnt))
                    cnt))
            (if (null? lsts)
                (count-1 lst 0)
                (count-n (cons lst lsts) 0)))

        (define (fold proc val lst . lsts)
            (define (fold-n val lsts)
                (if (every-1 pair? lsts)
                    (fold-n (apply proc (append (map car lsts) (list val))) (map cdr lsts))
                    val))
            (if (null? lsts)
                (fold-1 proc val lst)
                (fold-n val (cons lst lsts))))

        (define (fold-1 proc val lst)
            (if (pair? lst)
                (fold-1 proc (proc (car lst) val) (cdr lst))
                val))

        (define (fold-right proc val lst . lsts)
            (define (fold-right-n lsts)
                (if (every-1 pair? lsts)
                    (apply proc (append (map car lsts) (list (fold-right-n (map cdr lsts)))))
                    val))
            (if (null? lsts)
                (fold-right-1 proc val lst)
                (fold-right-n (cons lst lsts))))

        (define (fold-right-1 proc val lst)
            (if (pair? lst)
                (proc (car lst) (fold-right-1 proc val (cdr lst)))
                val))

        (define (pair-fold proc val lst . lsts)
            (define (pair-fold-1 val lst)
                (if (pair? lst)
                    (let ((tail (cdr lst)))
                        (pair-fold-1 (proc lst val) tail))
                    val))
            (define (pair-fold-n val lsts)
                (if (every-1 pair? lsts)
                    (let ((tails (map cdr lsts)))
                        (pair-fold-n (apply proc (append lsts (list val))) tails))
                    val))
            (if (null? lsts)
                (pair-fold-1 val lst)
                (pair-fold-n val (cons lst lsts))))

        (define (pair-fold-right proc val lst . lsts)
            (define (pair-fold-right-1 val lst)
                (if (pair? lst)
                    (proc lst (pair-fold-right-1 val (cdr lst)))
                    val))
            (define (pair-fold-right-n lsts)
                (if (every-1 pair? lsts)
                    (apply proc (append lsts (list (pair-fold-right-n (map cdr lsts)))))
                    val))
            (if (null? lsts)
                (pair-fold-right-1 val lst)
                (pair-fold-right-n (cons lst lsts))))

        (define (reduce proc ident lst)
            (if (pair? lst)
                (fold proc (car lst) (cdr lst))
                ident))

        (define (reduce-right proc ident lst)
            (define (from-right head tail)
                (if (pair? tail)
                    (proc head (from-right (car tail) (cdr tail)))
                    head))
            (if (pair? lst)
                (from-right (car lst) (cdr lst))
                ident))

        (define (unfold pred proc gen seed . tgen)
            (define (from-left seed)
                (if (pred seed)
                    (if (pair? tgen)
                        ((car tgen) seed)
                        '())
                    (cons (proc seed) (from-left (gen seed)))))
            (from-left seed))

        (define (unfold-right pred proc gen seed . tail)
            (define (from-right seed lst)
                (if (pred seed)
                    lst
                    (from-right (gen seed) (cons (proc seed) lst))))
            (from-right seed (if (pair? tail) (car tail) '())))

        (define (append-map proc lst . lsts)
            (apply append (apply map proc lst lsts)))

        (define (append-map! proc lst . lsts)
            (apply append! (apply map proc lst lsts)))

        (define (pair-for-each proc lst . lsts)
            (define (pair-for-each-1 lst)
                (if (not (null? lst))
                    (let ((tail (cdr lst)))
                        (proc lst)
                        (pair-for-each-1 tail))))
            (define (pair-for-each-n lsts)
                (if (every-1 pair? lsts)
                    (let ((tails (map cdr lsts)))
                        (apply proc lsts)
                        (pair-for-each-n tails))))
            (if (null? lsts)
                (pair-for-each-1 lst)
                (pair-for-each-n (cons lst lsts))))

        (define (filter-map proc lst . lsts)
            (define (filter-map-1 lst rlst)
                (if (not (null? lst))
                    (let ((ret (proc (car lst))))
                        (filter-map-1 (cdr lst) (if ret (cons ret rlst) rlst)))
                    (reverse! rlst)))
            (define (filter-map-n lsts rlst)
                (if (every-1 pair? lsts)
                    (let ((ret (apply proc (map car lsts))))
                        (filter-map-n (map cdr lsts) (if ret (cons ret rlst) rlst)))
                    (reverse! rlst)))
            (if (null? lsts)
                (filter-map-1 lst '())
                (filter-map-n (cons lst lsts) '())))

        (define (filter pred lst)
            (if (pair? lst)
                (if (pred (car lst))
                    (cons (car lst) (filter pred (cdr lst)))
                    (filter pred (cdr lst)))
                '()))

        (define (partition pred lst)
            (define (%partition lst ylst nlst)
                (if (pair? lst)
                    (if (pred (car lst))
                        (%partition (cdr lst) (cons (car lst) ylst) nlst)
                        (%partition (cdr lst) ylst (cons (car lst) nlst)))
                    (values (reverse! ylst) (reverse! nlst))))
            (%partition lst '() '()))

        (define (remove pred lst)
            (if (pair? lst)
                (if (pred (car lst))
                    (remove pred (cdr lst))
                    (cons (car lst) (remove pred (cdr lst))))
                '()))

        (define (find pred lst)
            (cond
                ((find-tail pred lst) => car)
                (else #f)))

        (define (find-tail pred lst)
            (if (pair? lst)
                (if (pred (car lst))
                    lst
                    (find-tail pred (cdr lst)))
                #f))

        (define (take-while pred lst)
            (if (pair? lst)
                (if (pred (car lst))
                    (cons (car lst) (take-while pred (cdr lst)))
                    '())
                '()))

        (define (drop-while pred lst)
            (if (pair? lst)
                (if (pred (car lst))
                    (drop-while pred (cdr lst))
                    lst)
                lst))

        (define (span pred lst)
            (if (pair? lst)
                (if (pred (car lst))
                    (let-values (((pre suf) (span pred (cdr lst))))
                        (values (cons (car lst) pre) suf))
                    (values '() lst))
                (values '() '())))

        (define (break pred lst)
            (if (pair? lst)
                (if (pred (car lst))
                    (values '() lst)
                    (let-values (((pre suf) (break pred (cdr lst))))
                        (values (cons (car lst) pre) suf)))
                (values '() '())))

        (define (any pred lst . lsts)
            (define (any-1 head tail)
                (if (null? tail)
                    (pred head)
                    (or (pred head) (any-1 (car tail) (cdr tail)))))
            (define (any-n heads tails)
                (if (every-1 pair? tails)
                    (or (apply pred heads) (any-n (map car tails) (map cdr tails)))
                    (apply pred heads)))
            (if (null? lsts)
                (if (pair? lst)
                    (any-1 (car lst) (cdr lst))
                    #f)
                    (let ((lsts (cons lst lsts)))
                        (if (every-1 pair? lsts)
                            (any-n (map car lsts) (map cdr lsts))
                            #f))))

        (define (every pred lst . lsts)
            (define (every-n heads tails)
                (if (every-1 pair? tails)
                    (if (apply pred heads)
                        (every-n (map car tails) (map cdr tails))
                        #f)
                    (apply pred heads)))
                (if (null? lsts)
                    (every-1 pred lst)
                    (let ((lsts (cons lst lsts)))
                        (if (every-1 pair? lsts)
                            (every-n (map car lsts) (map cdr lsts))
                            #t))))

        (define (every-1 pred lst)
            (define (every-head head tail)
                (if (null? tail)
                    (pred head)
                    (and (pred head) (every-head (car tail) (cdr tail)))))
            (if (pair? lst)
                (every-head (car lst) (cdr lst))
                #t))

        (define (list-index pred lst . lsts)
            (define (list-index-1 lst n)
                (if (null? lst)
                    #f
                    (if (pred (car lst))
                        n
                        (list-index-1 (cdr lst) (+ n 1)))))
            (define (list-index-n lsts n)
                (if (every-1 pair? lsts)
                    (if (apply pred (map car lsts))
                        n
                        (list-index-n (map cdr lsts) (+ n 1)))
                    #f))
            (if (null? lsts)
                (list-index-1 lst 0)
                (list-index-n (cons lst lsts) 0)))

        (define (delete obj lst . eq)
            (let ((eq (if (null? eq) equal? (car eq))))
                (define (delete-first lst)
                    (if (pair? lst)
                        (if (eq obj (car lst))
                            (delete-first (cdr lst))
                            (cons (car lst) (delete-first (cdr lst))))
                        '()))
                (delete-first lst)))

        (define (delete-duplicates lst . eq)
            (let ((eq (if (null? eq) equal? (car eq))))
                (define (duplicates lst ret)
                    (if (pair? lst)
                        (duplicates (cdr lst)
                            (if (member (car lst) ret eq)
                                ret
                                (cons (car lst) ret)))
                        (reverse! ret)))
                (duplicates lst '())))

        (define (alist-cons key datum alist)
            (cons (cons key datum) alist))

        (define (alist-copy alist)
            (map (lambda (obj) (cons (car obj) (cdr obj))) alist))

        (define (alist-delete key alist . eq)
            (let ((eq (if (null? eq) equal? (car eq))))
                (remove (lambda (obj) (eq (car obj) key)) alist)))

        (define (lset<= eq . lsts)
            (define (lset-2<= lst1 lst2)
                (if (pair? lst1)
                    (if (member (car lst1) lst2 eq)
                        (lset-2<= (cdr lst1) lst2)
                        #f)
                    #t))
            (define (lset-ht<= head tail)
                (if (pair? tail)
                    (if (lset-2<= head (car tail))
                        (lset-ht<= (car tail) (cdr tail))
                        #f)
                    #t))
            (if (pair? lsts)
                (lset-ht<= (car lsts) (cdr lsts))
                #t))

        (define (lset= eq . lsts)
            (and (apply lset<= eq lsts) (apply lset<= eq (reverse lsts))))

        (define (lset-adjoin eq lst . objs)
            (define (adjoin lst objs)
                (if (pair? objs)
                    (if (member (car objs) lst eq)
                        (adjoin lst (cdr objs))
                        (adjoin (cons (car objs) lst) (cdr objs)))
                    lst))
            (adjoin lst objs))

        (define (lset-union eq . lsts)
            (define (union-2 ret lst)
                (if (pair? lst)
                    (if (member (car lst) ret eq)
                        (union-2 ret (cdr lst))
                        (union-2 (cons (car lst) ret) (cdr lst)))
                    ret))
            (define (union-n ret lsts)
                (if (pair? lsts)
                    (if (null? ret)
                        (union-n (car lsts) (cdr lsts))
                        (union-n (union-2 ret (car lsts)) (cdr lsts)))
                    ret))
            (if (pair? lsts)
                (union-n (car lsts) (cdr lsts))
                '()))

        (define (lset-intersection eq ret . lsts)
            (filter (lambda (obj) (every (lambda (lst) (member obj lst eq)) lsts)) ret))

        (define (lset-difference eq ret . lsts)
            (filter (lambda (obj) (every (lambda (lst) (not (member obj lst eq))) lsts)) ret))

        (define (lset-xor eq . lsts)
            (define (difference-2 eq lst1 lst2)
                (remove (lambda (obj) (member obj lst1 eq)) lst2))
            (reduce (lambda (lst1 lst2)
                        (append (difference-2 eq lst1 lst2) (difference-2 eq lst2 lst1)))
                    '() lsts))

        (define (lset-diff+intersection eq lst . lsts)
            (values (apply lset-difference eq lst lsts) (apply lset-intersection eq lst lsts)))
    ))
