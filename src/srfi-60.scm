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
        (define (first-set-bit n)
            (- (integer-length (bitwise-and n (- n))) 1))
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
            (list->integer bools))
    ))
