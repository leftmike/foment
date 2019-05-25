(define-library (scheme charset)
    (aka (srfi 14))
    (import (foment base))
    (export
        char-set?
        char-set=
        char-set<=
        char-set-hash
        char-set-cursor
        char-set-ref
        char-set-cursor-next
        end-of-char-set?
        char-set-fold
        char-set-unfold
        char-set-unfold!
        char-set-for-each
        char-set-map
        char-set-copy
        char-set
        list->char-set
        string->char-set
        list->char-set!
        string->char-set!
        char-set-filter
        char-set-filter!
        ucs-range->char-set
        ucs-range->char-set!
        ->char-set
        char-set-size
        char-set-count
        char-set->list
        char-set->string
        char-set-contains?
        char-set-every
        char-set-any
        char-set-adjoin
        char-set-delete
        (rename char-set-adjoin char-set-adjoin!)
        (rename char-set-delete char-set-delete!)
        char-set-complement
        (rename char-set-complement char-set-complement!)
        char-set-union
        char-set-union!
        char-set-intersection
        char-set-intersection!
        char-set-difference
        (rename char-set-difference char-set-difference!)
        char-set-xor
        char-set-xor!
        char-set-diff+intersection
        char-set-diff+intersection!
        char-set:lower-case
        char-set:upper-case
        char-set:title-case
        char-set:letter
        char-set:digit
        char-set:letter+digit
        char-set:graphic
        char-set:printing
        char-set:whitespace
        char-set:iso-control
        char-set:punctuation
        char-set:symbol
        char-set:hex-digit
        char-set:blank
        char-set:ascii
        char-set:empty
        char-set:full
        )
    (begin
        (define (char-set-ref cset cursor)
            cursor)
        (define (end-of-char-set? cursor)
            (not (char? cursor)))
        (define (char-set-fold proc seed cset)
            (define (fold cursor seed)
                (if (end-of-char-set? cursor)
                    seed
                    (let ((ch (char-set-ref cset cursor)))
                        (fold (char-set-cursor-next cset cursor) (proc ch seed)))))
            (fold (char-set-cursor cset) seed))
        (define char-set-unfold
            (case-lambda
                ((to-char pred gen seed) (char-set-unfold! to-char pred gen seed (char-set)))
                ((to-char pred gen seed base) (char-set-unfold! to-char pred gen seed base))))
        (define (char-set-unfold! to-char pred gen seed base)
            (define (unfold seed lst)
                (if (pred seed)
                    lst
                    (unfold (gen seed) (cons (to-char seed) lst))))
            (list->char-set (unfold seed '()) base))
        (define (char-set-for-each proc cset)
            (for-each proc (char-set-fold cons '() cset)))
        (define (char-set-map proc cset)
            (list->char-set (map proc (char-set-fold cons '() cset))))
        (define (char-set-copy cset)
            cset)
        (define (char-set . char-list)
            (list->char-set char-list))
        (define (list->char-set! char-list cset)
            (list->char-set char-list cset))
        (define (string->char-set! char-list cset)
            (string->char-set char-list cset))
        (define char-set-filter
            (case-lambda
                ((pred cset) (char-set-filter! pred cset (char-set)))
                ((pred cset base) (char-set-filter! pred cset base))))
        (define (char-set-filter! pred cset base)
            (list->char-set
                (char-set-fold (lambda (ch lst) (if (pred ch) (cons ch lst) lst)) '() cset)
                base))
        (define (->char-set obj)
            (cond
                ((string? obj) (string->char-set obj))
                ((char? obj) (char-set obj))
                ((char-set? obj) obj)
                (else (full-error 'assertion-violation '->char-set #f
                            "->char-set: expected a character, string, or char-set" obj))))
        (define (ucs-range->char-set! lower upper error? base)
            (ucs-range->char-set lower upper error? base))
        (define (char-set-size cset)
            (char-set-fold (lambda (ch cnt) (+ cnt 1)) 0 cset))
        (define (char-set-count pred cset)
            (char-set-fold (lambda (ch cnt) (if (pred ch) (+ cnt 1) cnt)) 0 cset))
        (define (char-set->list cset)
            (char-set-fold (lambda (ch lst) (cons ch lst)) '() cset))
        (define (char-set->string cset)
            (list->string (char-set->list cset)))
        (define (char-set-every pred cset)
            (define (every cursor)
                (if (end-of-char-set? cursor)
                    #t
                    (if (pred (char-set-ref cset cursor))
                        (every (char-set-cursor-next cset cursor))
                        #f)))
            (every (char-set-cursor cset)))
        (define (char-set-any pred cset)
            (define (any cursor)
                (if (end-of-char-set? cursor)
                    #f
                    (or
                        (pred (char-set-ref cset cursor))
                        (any (char-set-cursor-next cset cursor)))))
            (any (char-set-cursor cset)))
        (define (char-set-adjoin cset . chars)
            (list->char-set chars cset))
        (define (char-set-delete cset . chars)
            (let ((delete (list->char-set chars)))
                (char-set-filter (lambda (ch) (not (char-set-contains? delete ch))) cset)))
        (define (char-set-union! cset . csets)
            (apply char-set-union cset csets))
        (define char-set-intersection
            (case-lambda
                (() char-set:full)
                ((cset) cset)
                ((cset1 cset2) (%char-set-intersection cset1 cset2))
                ((cset . csets) (%char-set-intersection cset (apply char-set-union csets)))))
        (define char-set-intersection!
            (case-lambda
                ((cset) cset)
                ((cset1 cset2) (%char-set-intersection cset1 cset2))
                ((cset . csets) (%char-set-intersection cset (apply char-set-union csets)))))
        (define char-set-difference
            (case-lambda
                ((cset) cset)
                ((cset1 cset2) (%char-set-intersection cset1 (char-set-complement cset2)))
                ((cset . csets)
                    (%char-set-intersection cset
                            (char-set-complement (apply char-set-union csets))))))
        (define (%char-set-xor cset csets)
            (if (null? csets)
                cset
                (%char-set-xor
                    (char-set-union
                        (%char-set-intersection cset (char-set-complement (car csets)))
                        (%char-set-intersection (car csets) (char-set-complement cset)))
                    (cdr csets))))
        (define char-set-xor
            (case-lambda
                (() char-set:empty)
                ((cset . csets) (%char-set-xor cset csets))))
        (define (char-set-xor! cset . csets)
            (%char-set-xor cset csets))
        (define (char-set-diff+intersection cset . csets)
            (values (%char-set-intersection cset (char-set-complement (apply char-set-union csets)))
                (%char-set-intersection cset (apply char-set-union csets))))
        (define (char-set-diff+intersection! cset1 cset2 . csets)
            (values
                (%char-set-intersection cset1
                        (char-set-complement (apply char-set-union cset2 csets)))
                (%char-set-intersection cset1 (apply char-set-union cset2 csets))))
        (define char-set:letter+digit
            (char-set-union char-set:letter char-set:digit))
        (define char-set:graphic
            (char-set-union char-set:letter char-set:digit char-set:punctuation char-set:symbol))
        (define char-set:printing
            (char-set-union char-set:graphic char-set:whitespace))
    ))
