(define-library (srfi 151)
    (import (scheme base))
    (import (scheme case-lambda))
    (import (srfi 60))
    (export
        bitwise-not
        bitwise-and
        bitwise-ior
        bitwise-xor
        bitwise-eqv
        bitwise-nand
        bitwise-nor
        bitwise-andc1
        bitwise-andc2
        bitwise-orc1
        bitwise-orc2
        arithmetic-shift
        bit-count
        integer-length
        bitwise-if
        bit-set?
        copy-bit
        bit-swap
        any-bit-set?
        every-bit-set?
        first-set-bit
        bit-field
        bit-field-any?
        bit-field-every?
        bit-field-clear
        bit-field-set
        bit-field-replace
        bit-field-replace-same
        bit-field-rotate
        bit-field-reverse
        bits->list
        list->bits
        bits->vector
        vector->bits
        bits
        bitwise-fold
        bitwise-for-each
        bitwise-unfold
        make-bitwise-generator
        )
    (begin
        (define (bitwise-nand i j)
             (bitwise-not (bitwise-and i j)))
        (define (bitwise-nor i j)
            (bitwise-not (bitwise-ior i j)))
        (define (bitwise-andc1 i j)
            (bitwise-and (bitwise-not i) j))
        (define (bitwise-andc2 i j)
            (bitwise-and i (bitwise-not j)))
        (define (bitwise-orc1 i j)
            (bitwise-ior (bitwise-not i) j))
        (define (bitwise-orc2 i j)
            (bitwise-ior i (bitwise-not j)))
        (define (bitwise-eqv . args)
            (define (b-eqv args ret)
                (if (pair? args)
                    (b-eqv (cdr args) (bitwise-not (bitwise-xor ret (car args))))
                    ret))
            (b-eqv args -1))
        (define (bit-swap i1 i2 n)
            (copy-bit i2
                    (copy-bit i1 n (bit-set? i2 n))
                    (bit-set? i1 n)))
        (define (any-bit-set? test-bits i)
            (not (zero? (bitwise-and test-bits i))))
        (define (every-bit-set? test-bits i)
            (= test-bits (bitwise-and test-bits i)))
        (define (bit-field-any? i start end)
            (not (zero? (bitwise-and (arithmetic-shift i (- start)) (mask start end)))))
        (define (bit-field-every? i start end)
            (let ((m (mask start end)))
                (eqv? m (bitwise-and (arithmetic-shift i (- start)) m))))
        (define (bit-field-clear i start end)
            (bit-field-replace i 0 start end))
        (define (bit-field-set i start end)
            (bit-field-replace i -1 start end))
        (define (mask start end)
            (bitwise-not (arithmetic-shift -1 (- end start))))
        (define (bit-field-replace dest src start end)
            (let ((m (mask start end)))
                (bitwise-ior (bitwise-and dest (bitwise-not (arithmetic-shift m start)))
                        (arithmetic-shift (bitwise-and src m) start))))
        (define (bit-field-replace-same dest src start end)
            (bitwise-if (arithmetic-shift (mask start end) start) src dest))
        (define bits->vector
            (case-lambda
                ((i) (list->vector (bits->list i)))
                ((i len) (list->vector (bits->list i len)))))
        (define (vector->bits vec)
            (list->bits (vector->list vec)))

        ;;; Copyright John Cowan 2017

        (define (bitwise-fold proc seed i)
          (let ((len (integer-length i)))
            (let loop ((n 0) (r seed))
              (if (= n len)
                r
                (loop (+ n 1) (proc (bit-set? n i) r))))))

        (define (bitwise-for-each proc i)
          (let ((len (integer-length i)))
            (let loop ((n 0))
              (when (< n len)
                (proc (bit-set? n i))
                (loop (+ n 1))))))

        (define (bitwise-unfold stop? mapper successor seed)
          (let loop ((n 0) (result 0) (state seed))
            (if (stop? state)
              result
                (loop (+ n 1)
                      (copy-bit n result (mapper state))
                      (successor state)))))

        (define (make-bitwise-generator i)
          (lambda ()
            (let ((bit (bit-set? 0 i)))
               (set! i (arithmetic-shift i -1))
               bit)))

        ;;; Copyright (C) 1991, 1993, 2001, 2003, 2005 Aubrey Jaffer
        ;
        ;Permission to copy this software, to modify it, to redistribute it,
        ;to distribute modified versions, and to use it for any purpose is
        ;granted, subject to the following restrictions and understandings.
        ;
        ;1.  Any copy made of this software must include this copyright notice
        ;in full.
        ;
        ;2.  I have made no warranty or representation that the operation of
        ;this software will be error-free, and I am under no obligation to
        ;provide any services, by way of maintenance, update, or otherwise.
        ;
        ;3.  In conjunction with products arising from the use of this
        ;material, there shall be no use of my name in any advertising,
        ;promotional, or sales literature without prior written consent in
        ;each case.

        (define (bit-field-rotate n count start end)
          (define width (- end start))
          (set! count (modulo count width))
          (let ((mask (bitwise-not (arithmetic-shift -1 width))))
            (define zn (bitwise-and mask (arithmetic-shift n (- start))))
            (bitwise-ior (arithmetic-shift
                     (bitwise-ior (bitwise-and mask (arithmetic-shift zn count))
                             (arithmetic-shift zn (- count width)))
                     start)
                    (bitwise-and (bitwise-not (arithmetic-shift mask start)) n))))

        (define (bit-reverse k n)
          (do ((m (if (negative? n) (bitwise-not n) n) (arithmetic-shift m -1))
               (k (+ -1 k) (+ -1 k))
               (rvs 0 (bitwise-ior (arithmetic-shift rvs 1) (bitwise-and 1 m))))
              ((negative? k) (if (negative? n) (bitwise-not rvs) rvs))))


        (define (bit-field-reverse n start end)
          (define width (- end start))
          (let ((mask (bitwise-not (arithmetic-shift -1 width))))
            (define zn (bitwise-and mask (arithmetic-shift n (- start))))
            (bitwise-ior (arithmetic-shift (bit-reverse width zn) start)
                    (bitwise-and (bitwise-not (arithmetic-shift mask start)) n))))

        #|
        (define (copy-bit index to bool)
          (if bool
              (bitwise-ior to (arithmetic-shift 1 index))
              (bitwise-and to (bitwise-not (arithmetic-shift 1 index)))))
|#
        (define (bits->list k . len)
          (if (null? len)
              (do ((k k (arithmetic-shift k -1))
                   (lst '() (cons (odd? k) lst)))
                  ((<= k 0) (reverse lst)))
              (do ((idx (+ -1 (car len)) (+ -1 idx))
                   (k k (arithmetic-shift k -1))
                   (lst '() (cons (odd? k) lst)))
                  ((negative? idx) (reverse lst)))))

        (define (list->bits bools)
          (do ((bs (reverse bools) (cdr bs))
               (acc 0 (+ acc acc (if (car bs) 1 0))))
              ((null? bs) acc)))

        (define (bits . bools)
          (list->bits bools))

        #|
        (define (bitwise-if mask n0 n1)
          (bitwise-ior (bitwise-and mask n0)
                  (bitwise-and (bitwise-not mask) n1)))
|#
        )
    )
