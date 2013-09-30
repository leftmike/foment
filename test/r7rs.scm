;;;
;;; R7RS
;;;

(import (scheme base))
(import (scheme case-lambda))
(import (scheme char))
(import (scheme inexact))
(import (scheme write))

;;
;; ---- expressions ----
;;

;; quote

(must-equal a (quote a))
(must-equal #(a b c) (quote #(a b c)))
(must-equal (+ 1 2) (quote (+ 1 2)))

(must-equal a 'a)
(must-equal #(a b c) '#(a b c))
(must-equal () '())
(must-equal (+ 1 2) '(+ 1 2))
(must-equal (quote a) '(quote a))
(must-equal (quote a) ''a)

(must-equal 145932 '145932)
(must-equal 145932 145932)
(must-equal "abc" '"abc")
(must-equal "abc" "abc")
(must-equal #\a '#\a)
(must-equal #\a #\a)
(must-equal #(a 10) '#(a 10))
(must-equal #(a 10) #(a 10))
(must-equal #u8(64 65) '#u8(64 65))
(must-equal #u8(64 65) #u8(64 65))
(must-equal #t '#t)
(must-equal #t #t)
(must-equal #t #true)
(must-equal #f #false)

(must-equal #(a 10) ((lambda () '#(a 10))))
(must-equal #(a 10) ((lambda () #(a 10))))

(must-raise (syntax-violation quote) (quote))
(must-raise (syntax-violation quote) (quote . a))
(must-raise (syntax-violation quote) (quote  a b))
(must-raise (syntax-violation quote) (quote a . b))

;; procedure call

(must-equal 7 (+ 3 4))
(must-equal 12 ((if #f + *) 3 4))

(must-equal 0 (+))
(must-equal 12 (+ 12))
(must-equal 19 (+ 12 7))
(must-equal 23 (+ 12 7 4))
(must-raise (syntax-violation procedure-call) (+ 12 . 7))

;; lambda

(must-equal 8 ((lambda (x) (+ x x)) 4))

(define reverse-subtract (lambda (x y) (- y x)))
(must-equal 3 (reverse-subtract 7 10))

(define add4 (let ((x 4)) (lambda (y) (+ x y))))
(must-equal 10 (add4 6))

(must-equal (3 4 5 6) ((lambda x x) 3 4 5 6))
(must-equal (5 6) ((lambda (x y . z) z) 3 4 5 6))

(must-equal 4 ((lambda () 4)))

(must-raise (syntax-violation lambda) (lambda (x x) x))
(must-raise (syntax-violation lambda) (lambda (x y z x) x))
(must-raise (syntax-violation lambda) (lambda (z x y . x) x))
(must-raise (syntax-violation lambda) (lambda (x 13 x) x))
(must-raise (syntax-violation lambda) (lambda (x)))
(must-raise (syntax-violation lambda) (lambda (x) . x))

(must-equal 12 ((lambda (a b)
    (define (x c) (+ c b))
    (define (y b)
        (define (z e) (+ (x e) a b))
        (z (+ b b)))
    (y 3)) 1 2))

(define (q w)
    (define (d v) (+ v 1000))
    (define (a x)
        (define (b y)
            (define (c z)
                (d (+ z w)))
            (c (+ y 10)))
        (b (+ x 100)))
    (a 0))
(must-equal 11110 (q 10000))

(define (q u)
    (define (a v)
        (define (b w)
            (define (c x)
                (define (d y)
                    (define (e z)
                        (f z))
                    (e y))
                (d x))
            (c w))
        (b v))
    (define (f t) (* t 2))
    (a u))
(must-equal 4 (q 2))

(define (l x y) x)
(must-raise (assertion-violation l) (l 3))
(must-raise (assertion-violation l) (l 3 4 5))

(define (make-counter n)
    (lambda () n))
(define counter (make-counter 10))
(must-equal 10 (counter))

(must-equal 200
    (letrec ((recur (lambda (x max) (if (= x max) (+ x max) (recur (+ x 1) max))))) (recur 1 100)))

;; if

(must-equal yes (if (> 3 2) 'yes 'no))
(must-equal no (if (> 2 3) 'yes 'no))
(must-equal 1 (if (> 3 2) (- 3 2) (+ 3 2)))

(must-equal true (if #t 'true 'false))
(must-equal false (if #f 'true 'false))
(must-equal 11 (+ 1 (if #t 10 20)))
(must-equal 21 (+ 1 (if #f 10 20)))
(must-equal 11 (+ 1 (if #t 10)))
(must-raise (assertion-violation +) (+ 1 (if #f 10)))

;; set!

(define x 2)
(must-equal 3 (+ x 1))
(set! x 4)
(must-equal 5 (+ x 1))

(define (make-counter n)
    (lambda () (set! n (+ n 1)) n))
(define c1 (make-counter 0))
(must-equal 1 (c1))
(must-equal 2 (c1))
(must-equal 3 (c1))
(define c2 (make-counter 100))
(must-equal 101 (c2))
(must-equal 102 (c2))
(must-equal 103 (c2))
(must-equal 4 (c1))

(define (q x)
    (set! x 10) x)
(must-equal 10 (q 123))

(define (q x)
    (define (r y)
        (set! x y))
    (r 10)
    x)
(must-equal 10 (q 123))

;; include

(must-equal (10 20) (begin (include "..\\test\\include.scm") (list INCLUDE-A include-b)))
(must-raise (assertion-violation) (begin (include "..\\test\\include3.scm") include-c))

(must-equal 10 (let ((a 0) (B 0)) (set! a 1) (set! B 1) (include "..\\test\\include2.scm") a))
(must-equal 20 (let ((a 0) (B 0)) (set! a 1) (set! B 1) (include "..\\test\\include2.scm") B))

;; include-ci

(must-raise (assertion-violation) (begin (include-ci "..\\test\\include4.scm") INCLUDE-E))
(must-equal (10 20) (begin (include-ci "..\\test\\include5.scm") (list include-g include-h)))

(must-equal 10 (let ((a 0) (b 0)) (set! a 1) (set! b 1) (include-ci "..\\test\\include2.scm") a))
(must-equal 20 (let ((a 0) (b 0)) (set! a 1) (set! b 1) (include-ci "..\\test\\include2.scm") b))

;; cond

(define (cadr obj) (car (cdr obj)))

(must-equal greater (cond ((> 3 2) 'greater) ((< 3 2) 'less)))
(must-equal equal (cond ((> 3 3) 'greater) ((< 3 3) 'less) (else 'equal)))
(must-equal 2 (cond ('(1 2 3) => cadr) (else #f)))
;(must-equal 2 (cond ((assv 'b '((a 1) (b 2))) => cadr) (else #f)))

;; case

(must-equal composite (case (* 2 3) ((2 3 5 7) 'prime) ((1 4 6 8 9) 'composite)))
(must-equal c (case (car '(c d)) ((a) 'a) ((b) 'b)))
(must-equal vowel
    (case (car '(i d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant)))
(must-equal semivowel
    (case (car '(w d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant)))
(must-equal consonant
    (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant)))

(must-equal c (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel)
        (else => (lambda (x) x))))
(must-equal (vowel o) (case (car '(o d)) ((a e i o u) => (lambda (x) (list 'vowel x)))
        ((w y) 'semivowel)
        (else => (lambda (x) x))))
(must-equal (semivowel y) (case (car '(y d)) ((a e i o u) 'vowel)
        ((w y) => (lambda (x) (list 'semivowel x)))
        (else => (lambda (x) x))))

(must-equal composite
    (let ((ret (case (* 2 3) ((2 3 5 7) 'prime) ((1 4 6 8 9) 'composite)))) ret))
(must-equal c (let ((ret (case (car '(c d)) ((a) 'a) ((b) 'b)))) ret))
(must-equal vowel
    (let ((ret (case (car '(i d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant))))
        ret))
(must-equal semivowel
    (let ((ret (case (car '(w d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant))))
        ret))
(must-equal consonant
    (let ((ret (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant))))
        ret))

(must-equal c (let ((ret (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel)
        (else => (lambda (x) x))))) ret))
(must-equal (vowel o) (let ((ret (case (car '(o d)) ((a e i o u) => (lambda (x) (list 'vowel x)))
        ((w y) 'semivowel)
        (else => (lambda (x) x))))) ret))
(must-equal (semivowel y) (let ((ret (case (car '(y d)) ((a e i o u) 'vowel)
        ((w y) => (lambda (x) (list 'semivowel x)))
        (else => (lambda (x) x))))) ret))

(must-equal a
    (let ((ret 'nothing)) (case 'a ((a b c d) => (lambda (x) (set! ret x) x))
        ((e f g h) (set! ret 'efgh) ret) (else (set! ret 'else))) ret))
(must-equal efgh
    (let ((ret 'nothing)) (case 'f ((a b c d) => (lambda (x) (set! ret x) x))
        ((e f g h) (set! ret 'efgh) ret) (else (set! ret 'else))) ret))
(must-equal else
    (let ((ret 'nothing)) (case 'z ((a b c d) => (lambda (x) (set! ret x) x))
        ((e f g h) (set! ret 'efgh) ret) (else (set! ret 'else))) ret))

;; and

(must-equal #t (and (= 2 2) (> 2 1)))
(must-equal #f (and (= 2 2) (< 2 1)))
(must-equal (f g) (and 1 2 'c '(f g)))
(must-equal #t (and))

;; or

(must-equal #t (or (= 2 2) (> 2 1)))
(must-equal #t (or (= 2 2) (< 2 1)))
(must-equal #f (or #f #f #f))
(must-equal (b c) (or '(b c) (/ 3 0)))

(must-equal #t (let ((x (or (= 2 2) (> 2 1)))) x))
(must-equal #t (let ((x (or (= 2 2) (< 2 1)))) x))
(must-equal #f (let ((x (or #f #f #f))) x))
(must-equal (b c) (let ((x (or '(b c) (/ 3 0)))) x))

(must-equal 1 (let ((x 0)) (or (begin (set! x 1) #t) (begin (set! x 2) #t)) x))
(must-equal 2 (let ((x 0)) (or (begin (set! x 1) #f) (begin (set! x 2) #t) (/ 3 0)) x))

;; when

(must-equal greater (when (> 3 2) 'greater))

;; unless

(must-equal less (unless (< 3 2) 'less))

;; cond-expand

(must-equal 1
    (cond-expand (no-features 0) (r7rs 1) (i386 2)))
(must-equal 2
    (cond-expand (no-features 0) (no-r7rs 1) (i386 2)))
(must-equal 0
    (cond-expand ((not no-features) 0) (no-r7rs 1) (i386 2)))

(must-equal 1
    (cond-expand ((and r7rs no-features) 0) ((and r7rs i386) 1) (r7rs 2)))
(must-equal 0
    (cond-expand ((or no-features r7rs) 0) ((and r7rs i386) 1) (r7rs 2)))

(must-equal 1
    (cond-expand ((and r7rs no-features) 0) (else 1)))

(must-raise (syntax-violation cond-expand)
    (cond-expand ((and r7rs no-features) 0) (no-features 1)))

(must-equal 1 (cond-expand ((library (scheme base)) 1) (else 2)))
(must-equal 2 (cond-expand ((library (not a library)) 1) (else 2)))
(must-equal 1 (cond-expand ((library (lib ce1)) 1) (else 2)))

(must-equal 1
    (let ((x 0) (y 1) (z 2)) (cond-expand (no-features x) (r7rs y) (i386 z))))
(must-equal 2
    (let ((x 0) (y 1) (z 2)) (cond-expand (no-features x) (no-r7rs y) (i386 z))))
(must-equal 0
    (let ((x 0) (y 1) (z 2)) (cond-expand ((not no-features) x) (no-r7rs y) (i386 z))))

(must-equal 1
    (let ((x 0) (y 1) (z 2))
        (cond-expand ((and r7rs no-features) x) ((and r7rs i386) y) (r7rs z))))
(must-equal 0
    (let ((x 0) (y 1) (z 2)) (cond-expand ((or no-features r7rs) x) ((and r7rs i386) y) (r7rs z))))

(must-equal 1
    (let ((x 0) (y 1)) (cond-expand ((and r7rs no-features) 0) (else 1))))

(must-raise (syntax-violation cond-expand)
    (let ((x 0) (y 1)) (cond-expand ((and r7rs no-features) x) (no-features y))))

(must-equal 1
    (let ((x 1) (y 2)) (cond-expand ((library (scheme base)) x) (else y))))
(must-equal 2
    (let ((x 1) (y 2)) (cond-expand ((library (not a library)) x) (else y))))

(must-equal 1
    (let ((x 1) (y 2)) (cond-expand ((library (lib ce2)) x) (else y))))

;; let

(must-equal 6 (let ((x 2) (y 3)) (* x y)))
(must-equal 35 (let ((x 2) (y 3)) (let ((x 7) (z (+ x y))) (* z x))))

(must-equal 2 (let ((a 2) (b 7)) a))
(must-equal 7 (let ((a 2) (b 7)) b))
(must-equal 2 (let ((a 2) (b 7)) (let ((a a) (b a)) a)))
(must-equal 2 (let ((a 2) (b 7)) (let ((a a) (b a)) b)))

(must-raise (syntax-violation let) (let))
(must-raise (syntax-violation let) (let (x 2) x))
(must-raise (syntax-violation let) (let x x))
(must-raise (syntax-violation let) (let ((x)) x))
(must-raise (syntax-violation let) (let ((x) 2) x))
(must-raise (syntax-violation let) (let ((x 2) y) x))
(must-raise (syntax-violation let) (let ((x 2) . y) x))
(must-raise (syntax-violation let) (let ((x 2) (x 3)) x))
(must-raise (syntax-violation let) (let ((x 2) (y 1) (x 3)) x))
(must-raise (syntax-violation let) (let ((x 2))))
(must-raise (syntax-violation let) (let ((x 2)) . x))
(must-raise (syntax-violation let) (let ((x 2)) y . x))
(must-raise (syntax-violation let) (let (((x y z) 2)) y x))
(must-raise (syntax-violation let) (let ((x 2) ("y" 3)) y))

;; let*

(must-equal 70 (let ((x 2) (y 3)) (let* ((x 7) (z (+ x y))) (* z x))))

(must-equal 2 (let* ((a 2) (b 7)) a))
(must-equal 7 (let* ((a 2) (b 7)) b))
(must-equal 4 (let ((a 2) (b 7)) (let* ((a (+ a a)) (b a)) b)))

(must-raise (syntax-violation let*) (let*))
(must-raise (syntax-violation let*) (let* (x 2) x))
(must-raise (syntax-violation let*) (let* x x))
(must-raise (syntax-violation let*) (let* ((x)) x))
(must-raise (syntax-violation let*) (let* ((x) 2) x))
(must-raise (syntax-violation let*) (let* ((x 2) y) x))
(must-raise (syntax-violation let*) (let* ((x 2) . y) x))
(must-equal 3 (let* ((x 2) (x 3)) x))
(must-equal 3 (let* ((x 2) (y 1) (x 3)) x))
(must-raise (syntax-violation let*) (let* ((x 2))))
(must-raise (syntax-violation let*) (let* ((x 2)) . x))
(must-raise (syntax-violation let*) (let* ((x 2)) y . x))
(must-raise (syntax-violation let*) (let* (((x y z) 2)) y x))
(must-raise (syntax-violation let*) (let* ((x 2) ("y" 3)) y))

;; letrec

(must-equal #t (letrec ((even? (lambda (n) (if (zero? n) #t (odd? (- n 1)))))
                        (odd? (lambda (n) (if (zero? n) #f (even? (- n 1))))))
                    (even? 88)))

(must-raise (syntax-violation letrec) (letrec))
(must-raise (syntax-violation letrec) (letrec (x 2) x))
(must-raise (syntax-violation letrec) (letrec x x))
(must-raise (syntax-violation letrec) (letrec ((x)) x))
(must-raise (syntax-violation letrec) (letrec ((x) 2) x))
(must-raise (syntax-violation letrec) (letrec ((x 2) y) x))
(must-raise (syntax-violation letrec) (letrec ((x 2) . y) x))
(must-raise (syntax-violation letrec) (letrec ((x 2) (x 3)) x))
(must-raise (syntax-violation letrec) (letrec ((x 2) (y 1) (x 3)) x))
(must-raise (syntax-violation letrec) (letrec ((x 2))))
(must-raise (syntax-violation letrec) (letrec ((x 2)) . x))
(must-raise (syntax-violation letrec) (letrec ((x 2)) y . x))
(must-raise (syntax-violation letrec) (letrec (((x y z) 2)) y x))
(must-raise (syntax-violation letrec) (letrec ((x 2) ("y" 3)) y))

;; letrec*

(must-equal 5 (letrec* ((p (lambda (x) (+ 1 (q (- x 1)))))
                        (q (lambda (y) (if (zero? y) 0 (+ 1 (p (- y 1))))))
                    (x (p 5))
                    (y x))
                    y))

(must-equal 45 (let ((x 5))
                (letrec* ((foo (lambda (y) (bar x y)))
                            (bar (lambda (a b) (+ (* a b) a))))
                    (foo (+ x 3)))))

(must-raise (syntax-violation letrec*) (letrec*))
(must-raise (syntax-violation letrec*) (letrec* (x 2) x))
(must-raise (syntax-violation letrec*) (letrec* x x))
(must-raise (syntax-violation letrec*) (letrec* ((x)) x))
(must-raise (syntax-violation letrec*) (letrec* ((x) 2) x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2) y) x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2) . y) x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2) (x 3)) x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2) (y 1) (x 3)) x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2))))
(must-raise (syntax-violation letrec*) (letrec* ((x 2)) . x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2)) y . x))
(must-raise (syntax-violation letrec*) (letrec* (((x y z) 2)) y x))
(must-raise (syntax-violation letrec*) (letrec* ((x 2) ("y" 3)) y))

;; let-values

(must-equal (1 2 3 4) (let-values (((a b) (values 1 2))
                                    ((c d) (values 3 4)))
                                (list a b c d)))
(must-equal (1 2 (3 4)) (let-values (((a b . c) (values 1 2 3 4)))
                                (list a b c)))
(must-equal (x y a b) (let ((a 'a) (b 'b) (x 'x) (y 'y))
                            (let-values (((a b) (values x y))
                                           ((x y) (values a b)))
                                    (list a b x y))))
(must-equal (1 2 3) (let-values ((x (values 1 2 3))) x))

(define (v)
    (define (v1) (v3) (v2) (v2))
    (define (v2) (v3) (v3))
    (define (v3) (values 1 2 3 4))
    (v1))
(must-equal (4 3 2 1) (let-values (((w x y z) (v))) (list z y x w)))

(must-raise (syntax-violation let-values) (let-values))
(must-raise (syntax-violation let-values) (let-values (x 2) x))
(must-raise (syntax-violation let-values) (let-values x x))
(must-raise (syntax-violation let-values) (let-values ((x)) x))
(must-raise (syntax-violation let-values) (let-values ((x) 2) x))
(must-raise (syntax-violation let-values) (let-values ((x 2) y) x))
(must-raise (syntax-violation let-values) (let-values ((x 2) . y) x))
(must-raise (syntax-violation let-values) (let-values ((x 2) (x 3)) x))
(must-raise (syntax-violation let-values) (let-values ((x 2) (y 1) (x 3)) x))
(must-raise (syntax-violation let-values) (let-values ((x 2))))
(must-raise (syntax-violation let-values) (let-values ((x 2)) . x))
(must-raise (syntax-violation let-values) (let-values ((x 2)) y . x))
(must-raise (syntax-violation let-values) (let-values (((x 2 y z) 2)) y x))
(must-raise (syntax-violation let-values) (let-values ((x 2) ("y" 3)) y))

;; let*-values

(must-equal (x y x y) (let ((a 'a) (b 'b) (x 'x) (y 'y))
                            (let*-values (((a b) (values x y))
                                            ((x y) (values a b)))
                                (list a b x y))))
(must-equal ((1 2 3) 4 5) (let*-values ((x (values 1 2 3)) (x (values x 4 5))) x))

(must-raise (syntax-violation let*-values) (let*-values))
(must-raise (syntax-violation let*-values) (let*-values (x 2) x))
(must-raise (syntax-violation let*-values) (let*-values x x))
(must-raise (syntax-violation let*-values) (let*-values ((x)) x))
(must-raise (syntax-violation let*-values) (let*-values ((x) 2) x))
(must-raise (syntax-violation let*-values) (let*-values ((x 2) y) x))
(must-raise (syntax-violation let*-values) (let*-values ((x 2) . y) x))
(must-raise (syntax-violation let*-values) (let*-values ((x 2))))
(must-raise (syntax-violation let*-values) (let*-values ((x 2)) . x))
(must-raise (syntax-violation let*-values) (let*-values ((x 2)) y . x))
(must-raise (syntax-violation let*-values) (let*-values (((x 2 y z) 2)) y x))
(must-raise (syntax-violation let*-values) (let*-values ((x 2) ("y" 3)) y))

;; begin

;; do

(define (range b e)
    (do ((r '() (cons e r))
        (e (- e 1) (- e 1)))
        ((< e b) r)))

(must-equal (3 4) (range 3 5))

(must-equal #(0 1 2 3 4)
    (do ((vec (make-vector 5))
        (i 0 (+ i 1)))
        ((= i 5) vec)
        (vector-set! vec i i)))

(must-equal 25
    (let ((x '(1 3 5 7 9)))
        (do ((x x (cdr x))
            (sum 0 (+ sum (car x))))
            ((null? x) sum))))

;; named let

(must-equal ((6 1 3) (-5 -2))
    (let loop ((numbers '(3 -2 1 6 -5))
            (nonneg '())
            (neg '()))
        (cond ((null? numbers) (list nonneg neg))
            ((>= (car numbers) 0)
                (loop (cdr numbers) (cons (car numbers) nonneg) neg))
            ((< (car numbers) 0)
                (loop (cdr numbers) nonneg (cons (car numbers) neg))))))

;; case-lambda

(define range
    (case-lambda
        ((e) (range 0 e))
        ((b e) (do ((r '() (cons e r))
                    (e (- e 1) (- e 1)))
                    ((< e b) r)))))

(must-equal (0 1 2) (range 3))
(must-equal (3 4) (range 3 5))
(must-raise (assertion-violation case-lambda) (range 1 2 3))

(define cl-test
    (case-lambda
        (() 'none)
        ((a) 'one)
        ((b c) 'two)
        ((b . d) 'rest)))

(must-equal none (cl-test))
(must-equal one (cl-test 0))
(must-equal two (cl-test 0 0))
(must-equal rest (cl-test 0 0 0))

(define cl-test2
    (case-lambda
        ((a b) 'two)
        ((c d e . f) 'rest)))

(must-raise (assertion-violation case-lambda) (cl-test2 0))
(must-equal two (cl-test2 0 0))
(must-equal rest (cl-test2 0 0 0))
(must-equal rest (cl-test2 0 0 0 0))

(must-raise (assertion-violation case-lambda)
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl)))
(must-equal one
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0)))
(must-equal two
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0 0)))
(must-equal rest
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0 0 0)))
(must-equal rest
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0 0 0 0)))

(must-equal one (let ((cl (case-lambda ((a) 'one) ((a b) 'two)))) (eq? cl cl) (cl 1)))

;; parameterize

(define radix
    (make-parameter 10
        (lambda (x)
            (if (and (exact-integer? x) (<= 2 x 16))
                x
                (error "invalid radix")))))

(define (f n) (number->string n (radix)))
(must-equal "12" (f 12))
(must-equal "1100" (parameterize ((radix 2)) (f 12)))
(must-equal "12" (f 12))
(radix 16)
(must-raise (assertion-violation error) (parameterize ((radix 0)) (f 12)))

;; guard

(must-equal 42
    (guard
        (condition
            ((assq 'a condition) => cdr)
            ((assq 'b condition)))
        (raise (list (cons 'a 42)))))

(must-equal (b . 23)
    (guard
        (condition
            ((assq 'a condition) => cdr)
            ((assq 'b condition)))
        (raise (list (cons 'b 23)))))

(must-equal else (guard (excpt ((= excpt 10) (- 10)) (else 'else)) (raise 11)))

(must-equal 121 (with-exception-handler
    (lambda (obj) (* obj obj))
    (lambda ()
        (guard (excpt ((= excpt 10) (- 10)) ((= excpt 12) (- 12))) (raise 11)))))

;; quasiquote

(must-equal (list 3 4) `(list ,(+ 1 2) 4))
(must-equal (list a (quote a)) (let ((name 'a)) `(list ,name ',name)))

(must-equal (a 3 4 5 6 b) `(a ,(+ 1 2) ,@(map abs '(4 -5 6)) b))
(must-equal ((foo 7) . cons) `((foo ,(- 10 3)) ,@(cdr '(c)) . ,(car '(cons))))
(must-equal #(10 5 2 4 3 8) `#(10 5 ,(sqrt 4) ,@(map sqrt '(16 9)) 8))
(must-equal (list foo bar baz) (let ((foo '(foo bar)) (@baz 'baz)) `(list ,@foo , @baz)))

(must-equal (a `(b ,(+ 1 2) ,(foo 4 d) e) f) `(a `(b ,(+ 1 2) ,(foo ,(+ 1 3) d) e) f))
(must-equal (a `(b ,x ,'y d) e) (let ((name1 'x) (name2 'y)) `(a `(b ,,name1 ,',name2 d) e)))

(must-equal (list 3 4) (quasiquote (list (unquote (+ 1 2)) 4)))
(must-equal `(list ,(+ 1 2) 4) '(quasiquote (list (unquote (+ 1 2)) 4)))

;; define

(define add3 (lambda (x) (+ x 3)))
(must-equal 6 (add3 3))

(define first car)
(must-equal 1 (first '(1 2)))

(must-equal 45 (let ((x 5))
                    (define foo (lambda (y) (bar x y)))
                    (define bar (lambda (a b) (+ (* a b) a)))
                (foo (+ x 3))))

;; define-syntax

;; defines-values

;;
;; ---- macros ----
;;

;; syntax-rules

(define-syntax be-like-begin
    (syntax-rules ()
        ((be-like-begin name)
            (define-syntax name
                (syntax-rules ()
                    ((name expr (... ...)) (begin expr (... ...))))))))
(be-like-begin sequence)
(must-equal 4 (sequence 1 2 3 4))
(must-equal 4 ((lambda () (be-like-begin sequence) (sequence 1 2 3 4))))

(must-equal ok (let ((=> #f)) (cond (#t => 'ok))))

;; let-syntax

(must-equal now (let-syntax
                    ((when (syntax-rules ()
                        ((when test stmt1 stmt2 ...)
                            (if test
                                (begin stmt1 stmt2 ...))))))
                    (let ((if #t))
                        (when if (set! if 'now))
                        if)))

(must-equal outer (let ((x 'outer))
                      (let-syntax ((m (syntax-rules () ((m) x))))
                            (let ((x 'inner))
                                (m)))))

;; letrec-syntax

(must-equal 7 (letrec-syntax
                    ((my-or (syntax-rules ()
                    ((my-or) #f)
                    ((my-or e) e)
                    ((my-or e1 e2 ...)
                        (let ((temp e1))
                            (if temp temp (my-or e2 ...)))))))
                (let ((x #f)
                        (y 7)
                        (temp 8)
                        (let odd?)
                        (if even?))
                    (my-or x (let temp) (if y) y))))

;; syntax-error

;;
;; ---- program structure ----
;;

;; define-record-type

(define-record-type <pare>
    (kons x y)
    pare?
    (x kar set-kar!)
    (y kdr))

(must-equal #t (pare? (kons 1 2)))
(must-equal #f (pare? (cons 1 2)))
(must-equal 1 (kar (kons 1 2)))
(must-equal 2 (kdr (kons 1 2)))
(must-equal 3
    (let ((k (kons 1 2)))
        (set-kar! k 3)
        (kar k)))

(must-equal 3
    ((lambda ()
        (define-record-type <pare>
            (kons x y)
            pare?
            (x kar set-kar!)
            (y kdr))
        (let ((k (kons 1 2)))
            (set-kar! k 3)
            (kar k)))))

;; define-library

(must-equal 100 (begin (import (lib a b c)) lib-a-b-c))
(must-equal 10 (begin (import (lib t1)) lib-t1-a))
(must-raise (assertion-violation) lib-t1-b)
(must-equal 20 b-lib-t1)
(must-raise (assertion-violation) lib-t1-c)

(must-equal 10 (begin (import (lib t2)) (lib-t2-a)))
(must-equal 20 (lib-t2-b))
(must-raise (syntax-violation) (import (lib t3)))
(must-raise (syntax-violation) (import (lib t4)))

(must-equal 1000 (begin (import (lib t5)) (lib-t5-b)))
(must-equal 1000 lib-t5-a)

(must-equal 1000 (begin (import (lib t6)) (lib-t6-b)))
(must-equal 1000 (lib-t6-a))

(must-equal 1000 (begin (import (lib t7)) (lib-t7-b)))
(must-equal 1000 lib-t7-a)

(must-equal 1 (begin (import (only (lib t8) lib-t8-a lib-t8-c)) lib-t8-a))
(must-raise (assertion-violation) lib-t8-b)
(must-equal 3 lib-t8-c)
(must-raise (assertion-violation) lib-t8-d)

(must-equal 1 (begin (import (except (lib t9) lib-t9-b lib-t9-d)) lib-t9-a))
(must-raise (assertion-violation) lib-t9-b)
(must-equal 3 lib-t9-c)
(must-raise (assertion-violation) lib-t9-d)

(must-equal 1 (begin (import (prefix (lib t10) x)) xlib-t10-a))
(must-raise (assertion-violation) lib-t10-b)
(must-equal 3 xlib-t10-c)
(must-raise (assertion-violation) lib-t10-d)

(must-equal 1 (begin (import (rename (lib t11) (lib-t11-b b-lib-t11) (lib-t11-d d-lib-t11)))
    lib-t11-a))
(must-raise (assertion-violation) lib-t11-b)
(must-equal 2 b-lib-t11)
(must-equal 3 lib-t11-c)
(must-raise (assertion-violation) lib-t11-d)
(must-equal 4 d-lib-t11)

(must-raise (syntax-violation) (import bad "bad library" name))
(must-raise (syntax-violation)
    (define-library (no ("good") "library") (import (scheme base)) (export +)))

(must-raise (syntax-violation) (import (lib t12)))
(must-raise (syntax-violation) (import (lib t13)))

(must-equal 10 (begin (import (lib t14)) (lib-t14-a 10 20)))
(must-equal 10 (lib-t14-b 10 20))

(must-equal 10 (begin (import (lib t15)) (lib-t15-a 10 20)))
(must-equal 10 (lib-t15-b 10 20))

;;
;; ---- booleans ----
;;

(must-equal #f (not #t))
(must-equal #f (not 3))
(must-equal #f (not (list 3)))
(must-equal #t (not #f))
(must-equal #f (not '()))
(must-equal #f (not (list)))
(must-equal #f (not 'nil))

(must-equal #t (boolean? #f))
(must-equal #f (boolean? 0))
(must-equal #f (boolean? '()))

(must-equal #t (boolean=? #t #t))
(must-equal #t (boolean=? #t #t #t))
(must-equal #t (boolean=? #f #f))
(must-equal #t (boolean=? #f #f #f))
(must-equal #f (boolean=? 1 #t))
(must-equal #f (boolean=? #t #t 0))
(must-equal #f (boolean=? #f 2))

;;
;; ---- pairs ----
;;

;; pair?

(must-equal #t (pair? '(a . b)))
(must-equal #t (pair? '(a b c)))
(must-equal #f (pair? '()))
(must-equal #f (pair? '#(a b)))

;; cons

(must-equal (a) (cons 'a '()))
(must-equal ((a) b c d) (cons '(a) '(b c d)))
(must-equal ("a" b c) (cons "a" '(b c)))
(must-equal (a . 3) (cons 'a 3))
(must-equal ((a b) . c) (cons '(a b) 'c))

(must-equal a (car '(a b c)))
(must-equal (a) (car '((a) b c d)))
(must-equal 1 (car '(1 . 2)))
(must-raise (assertion-violation car) (car '()))

(must-equal (b c d) (cdr '((a) b c d)))
(must-equal 2 (cdr '(1 . 2)))
(must-raise (assertion-violation cdr) (cdr '()))

;; append

(must-equal (x y) (append '(x) '(y)))
(must-equal (a b c d) (append '(a) '(b c d)))
(must-equal (a (b) (c)) (append '(a (b)) '((c))))
(must-equal (a b c . d) (append '(a b) '(c . d)))
(must-equal a (append '() 'a))

;; reverse

(must-equal (c b a) (reverse '(a b c)))
(must-equal ((e (f)) d (b c) a) (reverse '(a (b c) d (e (f)))))

;;
;; ---- characters ----
;;

(must-equal #\x7f #\delete)
(must-equal #\x1B #\escape)

(must-equal #t (char? #\a))
(must-equal #t (char? #\x03BB))
(must-equal #f (char? #x03BB))
(must-equal #f (char? "a"))

(must-raise (assertion-violation char?) (char?))
(must-raise (assertion-violation char?) (char? #\a 10))

(must-equal #t (char=? #\delete #\x7f))
(must-equal #t (char=? #\a #\a))
(must-equal #t (char=? #\a #\a #\a))
(must-equal #f (char=? #\a #\a #\b))

(must-raise (assertion-violation char=?) (char=? #\a))
(must-raise (assertion-violation char=?) (char=? #\a #\a 10))

(must-equal #t (char<? #\a #\b))
(must-equal #t (char<? #\a #\b #\c))
(must-equal #f (char<? #\a #\b #\c #\c))
(must-equal #f (char<? #\b #\b #\c))

(must-raise (assertion-violation char<?) (char<? #\a))
(must-raise (assertion-violation char<?) (char<? #\a #\b 10))

(must-equal #t (char>? #\b #\a))
(must-equal #t (char>? #\c #\b #\a))
(must-equal #f (char>? #\c #\b #\a #\a))
(must-equal #f (char>? #\b #\b #\a))

(must-raise (assertion-violation char>?) (char>? #\a))
(must-raise (assertion-violation char>?) (char>? #\b #\a 10))

(must-equal #t (char<=? #\a #\b #\b #\c #\d))
(must-equal #f (char<=? #\a #\c #\d #\a))

(must-raise (assertion-violation char<=?) (char<=? #\a))
(must-raise (assertion-violation char<=?) (char<=? #\a #\a 10))

(must-equal #t (char>=? #\d #\c #\c #\b #\a))
(must-equal #f (char>=? #\d #\c #\b #\c))

(must-raise (assertion-violation char>=?) (char>=? #\a))
(must-raise (assertion-violation char>=?) (char>=? #\a #\a 10))

(must-equal #t (char-ci=? #\delete #\x7f))
(must-equal #t (char-ci=? #\a #\a))
(must-equal #t (char-ci=? #\a #\A #\a))
(must-equal #f (char-ci=? #\a #\a #\b))

(must-raise (assertion-violation char-ci=?) (char-ci=? #\a))
(must-raise (assertion-violation char-ci=?) (char-ci=? #\a #\a 10))

(must-equal #t (char-ci<? #\a #\b))
(must-equal #t (char-ci<? #\a #\B #\c))
(must-equal #f (char-ci<? #\a #\b #\c #\c))
(must-equal #f (char-ci<? #\b #\B #\c))

(must-raise (assertion-violation char-ci<?) (char-ci<? #\a))
(must-raise (assertion-violation char-ci<?) (char-ci<? #\a #\b 10))

(must-equal #t (char-ci>? #\b #\a))
(must-equal #t (char-ci>? #\c #\B #\a))
(must-equal #f (char-ci>? #\c #\b #\a #\a))
(must-equal #f (char-ci>? #\b #\B #\a))

(must-raise (assertion-violation char-ci>?) (char-ci>? #\a))
(must-raise (assertion-violation char-ci>?) (char-ci>? #\b #\a 10))

(must-equal #t (char-ci<=? #\a #\B #\b #\c #\d))
(must-equal #f (char-ci<=? #\a #\c #\d #\A))

(must-raise (assertion-violation char-ci<=?) (char-ci<=? #\a))
(must-raise (assertion-violation char-ci<=?) (char-ci<=? #\a #\a 10))

(must-equal #t (char-ci>=? #\d #\c #\C #\b #\a))
(must-equal #f (char-ci>=? #\d #\C #\b #\c))

(must-raise (assertion-violation char-ci>=?) (char-ci>=? #\a))
(must-raise (assertion-violation char-ci>=?) (char-ci>=? #\a #\a 10))

(must-equal #f (char-alphabetic? #\tab))
(must-equal #t (char-alphabetic? #\a))
(must-equal #t (char-alphabetic? #\Q))
(must-equal #t (char-alphabetic? #\x1A59))
(must-equal #f (char-alphabetic? #\x1A60))
(must-equal #f (char-alphabetic? #\x2000))

(must-raise (assertion-violation char-alphabetic?) (char-alphabetic?))
(must-raise (assertion-violation char-alphabetic?) (char-alphabetic? 10))
(must-raise (assertion-violation char-alphabetic?) (char-alphabetic? #\a #\a))

(must-equal #t (char-numeric? #\3))
(must-equal #t (char-numeric? #\x0664))
(must-equal #t (char-numeric? #\x0AE6))
(must-equal #f (char-numeric? #\x0EA6))

(must-raise (assertion-violation char-numeric?) (char-numeric?))
(must-raise (assertion-violation char-numeric?) (char-numeric? 10))
(must-raise (assertion-violation char-numeric?) (char-numeric? #\a #\a))

(must-equal #t (char-whitespace? #\ ))
(must-equal #t (char-whitespace? #\x2001))
(must-equal #t (char-whitespace? #\x00A0))
(must-equal #f (char-whitespace? #\a))
(must-equal #f (char-whitespace? #\x0EA6))

(must-raise (assertion-violation char-whitespace?) (char-whitespace?))
(must-raise (assertion-violation char-whitespace?) (char-whitespace? 10))
(must-raise (assertion-violation char-whitespace?) (char-whitespace? #\a #\a))

(must-equal #t (char-upper-case? #\R))
(must-equal #f (char-upper-case? #\r))
(must-equal #t (char-upper-case? #\x1E20))
(must-equal #f (char-upper-case? #\x9999))

(must-raise (assertion-violation char-upper-case?) (char-upper-case?))
(must-raise (assertion-violation char-upper-case?) (char-upper-case? 10))
(must-raise (assertion-violation char-upper-case?) (char-upper-case? #\a #\a))

(must-equal #t (char-lower-case? #\s))
(must-equal #f (char-lower-case? #\S))
(must-equal #t (char-lower-case? #\xA687))
(must-equal #f (char-lower-case? #\x2CED))

(must-raise (assertion-violation char-lower-case?) (char-lower-case?))
(must-raise (assertion-violation char-lower-case?) (char-lower-case? 10))
(must-raise (assertion-violation char-lower-case?) (char-lower-case? #\a #\a))

(must-equal 3 (digit-value #\3))
(must-equal 4 (digit-value #\x0664))
(must-equal 0 (digit-value #\x0AE6))
(must-equal #f (digit-value #\x0EA6))

(must-raise (assertion-violation digit-value) (digit-value))
(must-raise (assertion-violation digit-value) (digit-value 10))
(must-raise (assertion-violation digit-value) (digit-value #\a #\a))

(must-equal #x03BB (char->integer #\x03BB))
(must-equal #xD (char->integer #\return))

(must-raise (assertion-violation char->integer) (char->integer))
(must-raise (assertion-violation char->integer) (char->integer #\a #\a))
(must-raise (assertion-violation char->integer) (char->integer 10))

(must-equal #\newline (integer->char #x0A))

(must-raise (assertion-violation integer->char) (integer->char))
(must-raise (assertion-violation integer->char) (integer->char 10 10))
(must-raise (assertion-violation integer->char) (integer->char #\a))

(must-equal #\A (char-upcase #\a))
(must-equal #\A (char-upcase #\A))
(must-equal #\9 (char-upcase #\9))

(must-raise (assertion-violation char-upcase) (char-upcase))
(must-raise (assertion-violation char-upcase) (char-upcase 10))
(must-raise (assertion-violation char-upcase) (char-upcase #\x10 #\x10))

(must-equal #\a (char-downcase #\a))
(must-equal #\a (char-downcase #\A))
(must-equal #\9 (char-downcase #\9))

(must-raise (assertion-violation char-downcase) (char-downcase))
(must-raise (assertion-violation char-downcase) (char-downcase 10))
(must-raise (assertion-violation char-downcase) (char-downcase #\x10 #\x10))

(must-equal #\x0064 (char-foldcase #\x0044))
(must-equal #\x00FE (char-foldcase #\x00DE))
(must-equal #\x016D (char-foldcase #\x016C))
(must-equal #\x0201 (char-foldcase #\x0200))
(must-equal #\x043B (char-foldcase #\x041B))
(must-equal #\x0511 (char-foldcase #\x0510))
(must-equal #\x1E1F (char-foldcase #\x1E1E))
(must-equal #\x1EF7 (char-foldcase #\x1EF6))
(must-equal #\x2C52 (char-foldcase #\x2C22))
(must-equal #\xA743 (char-foldcase #\xA742))
(must-equal #\xFF4D (char-foldcase #\xFF2D))
(must-equal #\x1043D (char-foldcase #\x10415))

(must-equal #\x0020 (char-foldcase #\x0020))
(must-equal #\x0091 (char-foldcase #\x0091))
(must-equal #\x0765 (char-foldcase #\x0765))
(must-equal #\x6123 (char-foldcase #\x6123))

(must-raise (assertion-violation char-foldcase) (char-foldcase))
(must-raise (assertion-violation char-foldcase) (char-foldcase 10))
(must-raise (assertion-violation char-foldcase) (char-foldcase #\x10 #\x10))

;;
;; ---- strings ----
;;

(must-equal #t (string? ""))
(must-equal #t (string? "123"))
(must-equal #f (string? #\a))
(must-equal #f (string? 'abc))

(must-raise (assertion-violation string?) (string?))
(must-raise (assertion-violation string?) (string? #\a 10))

(must-equal "" (make-string 0))
(must-equal 10 (string-length (make-string 10)))
(must-equal "aaaaaaaaaaaaaaaa" (make-string 16 #\a))

(must-raise (assertion-violation make-string) (make-string))
(must-raise (assertion-violation make-string) (make-string #\a))
(must-raise (assertion-violation make-string) (make-string 10 10))
(must-raise (assertion-violation make-string) (make-string 10 #\a 10))

(must-equal "" (string))
(must-equal "1234" (string #\1 #\2 #\3 #\4))

(must-raise (assertion-violation string) (string 1))
(must-raise (assertion-violation string) (string #\1 1))

(must-equal 0 (string-length ""))
(must-equal 4 (string-length "1234"))

(must-raise (assertion-violation string-length) (string-length))
(must-raise (assertion-violation string-length) (string-length #\a))
(must-raise (assertion-violation string-length) (string-length "" #\a))

(must-equal #\3 (string-ref "123456" 2))

(must-raise (assertion-violation string-ref) (string-ref ""))
(must-raise (assertion-violation string-ref) (string-ref "" 2 2))
(must-raise (assertion-violation string-ref) (string-ref "123" 3))
(must-raise (assertion-violation string-ref) (string-ref "123" -1))
(must-raise (assertion-violation string-ref) (string-ref #(1 2 3) 1))

(must-equal "*?*"
    (let ((s (make-string 3 #\*)))
        (string-set! s 1 #\?)
        s))

(define s (string #\1 #\2 #\3 #\4))
(must-raise (assertion-violation string-set!) (string-set! s 10))
(must-raise (assertion-violation string-set!) (string-set! s 10 #\a 10))
(must-raise (assertion-violation string-set!) (string-set! #\a 10 #\a))
(must-raise (assertion-violation string-set!) (string-set! s #t #\a))
(must-raise (assertion-violation string-set!) (string-set! s 10 'a))

(must-equal #t (string=? "aaaa" (make-string 4 #\a)))
(must-equal #t (string=? "\t\"\\" "\x9;\x22;\x5C;"))
(must-equal #t (string=? "aaa" "aaa" "aaa"))
(must-equal #f (string=? "aaa" "aaa" "bbb"))

(must-raise (assertion-violation string=?) (string=? "a"))
(must-raise (assertion-violation string=?) (string=? "a" "a" 10))

(must-equal #t (string<? "aaaa" (make-string 4 #\b)))
(must-equal #t (string<? "\t\"\\" "\x9;\x22;\x5C;c"))
(must-equal #t (string<? "aaa" "aab" "caa"))
(must-equal #f (string<? "aaa" "bbb" "bbb"))

(must-raise (assertion-violation string<?) (string<? "a"))
(must-raise (assertion-violation string<?) (string<? "a" "b" 10))

(must-equal #t (string>? "cccc" (make-string 4 #\b)))
(must-equal #t (string>? "\t\"\\c" "\x9;\x22;\x5C;"))
(must-equal #t (string>? "aac" "aab" "aaa"))
(must-equal #f (string>? "ccc" "bbb" "bbb"))

(must-raise (assertion-violation string>?) (string>? "a"))
(must-raise (assertion-violation string>?) (string>? "c" "b" 10))

(must-equal #t (string<=? "aaaa" (make-string 4 #\b) "bbbb"))
(must-equal #t (string<=? "\t\"\\" "\x9;\x22;\x5C;"))
(must-equal #t (string<=? "aaa" "aaa" "aab" "caa"))
(must-equal #f (string<=? "aaa" "bbb" "bbb" "a"))

(must-raise (assertion-violation string<=?) (string<=? "a"))
(must-raise (assertion-violation string<=?) (string<=? "a" "b" 10))

(must-equal #t (string>=? "cccc" (make-string 4 #\b) "bbbb"))
(must-equal #t (string>=? "\t\"\\c" "\x9;\x22;\x5C;"))
(must-equal #t (string>=? "aac" "aab" "aaa"))
(must-equal #f (string>=? "ccc" "bbb" "bbb" "ddd"))

(must-raise (assertion-violation string>=?) (string>=? "a"))
(must-raise (assertion-violation string>=?) (string>=? "c" "b" 10))

(must-equal #t (string-ci=? "aaaa" (make-string 4 #\A)))
(must-equal #t (string-ci=? "\t\"\\" "\x9;\x22;\x5C;"))
(must-equal #t (string-ci=? "aaA" "aAa" "Aaa"))
(must-equal #f (string-ci=? "aaa" "aaA" "Bbb"))

(must-raise (assertion-violation string-ci=?) (string-ci=? "a"))
(must-raise (assertion-violation string-ci=?) (string-ci=? "a" "a" 10))

(must-equal #t (string-ci<? "aAAa" (make-string 4 #\b)))
(must-equal #t (string-ci<? "\t\"\\" "\x9;\x22;\x5C;c"))
(must-equal #t (string-ci<? "aAa" "aaB" "cAa"))
(must-equal #f (string-ci<? "aaa" "bbb" "bBb"))

(must-raise (assertion-violation string-ci<?) (string-ci<? "a"))
(must-raise (assertion-violation string-ci<?) (string-ci<? "a" "b" 10))

(must-equal #t (string-ci>? "cccc" (make-string 4 #\B)))
(must-equal #t (string-ci>? "\t\"\\c" "\x9;\x22;\x5C;"))
(must-equal #t (string-ci>? "Aac" "aAb" "aaA"))
(must-equal #f (string-ci>? "ccC" "Bbb" "bbB"))

(must-raise (assertion-violation string-ci>?) (string-ci>? "a"))
(must-raise (assertion-violation string-ci>?) (string-ci>? "c" "b" 10))

(must-equal #t (string-ci<=? "aaAa" (make-string 4 #\b) "bBBb"))
(must-equal #t (string-ci<=? "\t\"\\" "\x9;\x22;\x5C;"))
(must-equal #t (string-ci<=? "aaA" "Aaa" "aAb" "caa"))
(must-equal #f (string-ci<=? "aaa" "bbb" "BBB" "a"))

(must-raise (assertion-violation string-ci<=?) (string-ci<=? "a"))
(must-raise (assertion-violation string-ci<=?) (string-ci<=? "a" "b" 10))

(must-equal #t (string-ci>=? "cccc" (make-string 4 #\B) "bbbb"))
(must-equal #t (string-ci>=? "\t\"\\c" "\x9;\x22;\x5C;"))
(must-equal #t (string-ci>=? "aac" "AAB" "aaa"))
(must-equal #f (string-ci>=? "ccc" "BBB" "bbb" "ddd"))

(must-raise (assertion-violation string-ci>=?) (string-ci>=? "a"))
(must-raise (assertion-violation string-ci>=?) (string-ci>=? "c" "b" 10))

(must-equal "AAA" (string-upcase "aaa"))
(must-equal "AAA" (string-upcase "aAa"))
(must-equal "\x0399;\x0308;\x0301;\x03A5;\x0308;\x0301;\x1FBA;\x0399;"
        (string-upcase "\x0390;\x03B0;\x1FB2;"))

(must-raise (assertion-violation string-upcase) (string-upcase))
(must-raise (assertion-violation string-upcase) (string-upcase #\a))
(must-raise (assertion-violation string-upcase) (string-upcase "a" "a"))

(must-equal "aaa" (string-downcase "AAA"))
(must-equal "aaa" (string-downcase "aAa"))
(must-equal "a\x0069;\x0307;a" (string-downcase "A\x0130;a"))

(must-raise (assertion-violation string-downcase) (string-downcase))
(must-raise (assertion-violation string-downcase) (string-downcase #\a))
(must-raise (assertion-violation string-downcase) (string-downcase "a" "a"))

(must-equal #t (string=? (string-foldcase "AAA") (string-foldcase "aaa")))
(must-equal #t (string=? (string-foldcase "AAA") (string-foldcase "aAa")))
(must-equal #t (string=? (string-foldcase "\x1E9A;") "\x0061;\x02BE;"))
(must-equal #t (string=? (string-foldcase "\x1F52;\x1F54;\x1F56;")
        "\x03C5;\x0313;\x0300;\x03C5;\x0313;\x0301;\x03C5;\x0313;\x0342;"))

(must-raise (assertion-violation string-foldcase) (string-foldcase))
(must-raise (assertion-violation string-foldcase) (string-foldcase #\a))
(must-raise (assertion-violation string-foldcase) (string-foldcase "a" "a"))

(must-equal "123abcdEFGHI" (string-append "123" "abcd" "EFGHI"))

(must-raise (assertion-violation string-append) (string-append #\a))
(must-raise (assertion-violation string-append) (string-append "a" #\a))

(must-equal (#\1 #\2 #\3 #\4) (string->list "1234"))
(must-equal (#\b #\c #\d) (string->list "abcdefg" 1 4))
(must-equal (#\w #\x #\y #\z) (string->list "qrstuvwxyz" 6))

(must-raise (assertion-violation string->list) (string->list))
(must-raise (assertion-violation string->list) (string->list #\a))
(must-raise (assertion-violation string->list) (string->list "123" -1))
(must-raise (assertion-violation string->list) (string->list "123" #\a))
(must-raise (assertion-violation string->list) (string->list "123" 1 0))
(must-raise (assertion-violation string->list) (string->list "123" 1 4))
(must-raise (assertion-violation string->list) (string->list "123" 1 #t))
(must-raise (assertion-violation string->list) (string->list "123" 1 3 3))

(must-equal "abc" (list->string '(#\a #\b #\c)))
(must-equal "" (list->string '()))

(must-raise (assertion-violation list->string) (list->string))
(must-raise (assertion-violation) (list->string "abc"))

(must-equal "234" (substring "12345" 1 4))

(must-equal "12345" (string-copy "12345"))
(must-equal "2345" (string-copy "12345" 1))
(must-equal "23" (string-copy "12345" 1 3))

(must-raise (assertion-violation string-copy) (string-copy))
(must-raise (assertion-violation string-copy) (string-copy #\a))
(must-raise (assertion-violation string-copy) (string-copy "abc" -1))
(must-raise (assertion-violation string-copy) (string-copy "abc" #t))
(must-raise (assertion-violation string-copy) (string-copy "abc" 3))
(must-raise (assertion-violation string-copy) (string-copy "abc" 1 0))
(must-raise (assertion-violation string-copy) (string-copy "abc" 1 4))
(must-raise (assertion-violation string-copy) (string-copy "abc" 1 2 3))

(define a "12345")
(define b (string-copy "abcde"))
(string-copy! b 1 a 0 2)
(must-equal "a12de" b)
(must-equal "abcde" (let ((s (make-string 5))) (string-copy! s 0 "abcde") s))
(must-equal "0abc0" (let ((s (make-string 5 #\0))) (string-copy! s 1 "abc") s))

(must-raise (assertion-violation string-copy!) (string-copy! (make-string 5) 0))
(must-raise (assertion-violation string-copy!) (string-copy! #\a 0 "abcde"))

;;
;; ---- vectors ----
;;

(must-equal #t (vector? #()))
(must-equal #t (vector? #(a b c)))
(must-equal #f (vector? #u8()))
(must-equal #f (vector? 12))
(must-raise (assertion-violation vector?) (vector?))
(must-raise (assertion-violation vector?) (vector? #() #()))

(must-equal #() (make-vector 0))
(must-equal #(a a a) (make-vector 3 'a))
(must-raise (assertion-violation make-vector) (make-vector))
(must-raise (assertion-violation make-vector) (make-vector -1))
(must-raise (assertion-violation make-vector) (make-vector 1 1 1))

(must-equal #(a b c) (vector 'a 'b 'c))
(must-equal #() (vector))

(must-equal 0 (vector-length #()))
(must-equal 3 (vector-length #(a b c)))
(must-raise (assertion-violation vector-length) (vector-length))
(must-raise (assertion-violation vector-length) (vector-length #u8()))
(must-raise (assertion-violation vector-length) (vector-length #() #()))

(must-equal 8 (vector-ref #(1 1 2 3 5 8 13 21) 5))
(must-raise (assertion-violation vector-ref) (vector-ref))
(must-raise (assertion-violation vector-ref) (vector-ref #(1 2 3)))
(must-raise (assertion-violation vector-ref) (vector-ref #(1 2 3) -1))
(must-raise (assertion-violation vector-ref) (vector-ref #(1 2 3) 3))
(must-raise (assertion-violation vector-ref) (vector-ref #(1 2 3) 1 1))
(must-raise (assertion-violation vector-ref) (vector-ref 1 1))

(must-equal #(0 ("Sue" "Sue") "Anna")
    (let ((vec (vector 0 '(2 2 2 2) "Anna")))
        (vector-set! vec 1 '("Sue" "Sue"))
        vec))

(define v (vector 1 2 3))
(must-raise (assertion-violation vector-set!) (vector-set!))
(must-raise (assertion-violation vector-set!) (vector-set! v))
(must-raise (assertion-violation vector-set!) (vector-set! v 1))
(must-raise (assertion-violation vector-set!) (vector-set! v 1 1 1))
(must-raise (assertion-violation vector-set!) (vector-set! 1 1 1))
(must-raise (assertion-violation vector-set!) (vector-set! v -1 1 1))
(must-raise (assertion-violation vector-set!) (vector-set! v 3 1 1))

(must-equal (dah dah didah) (vector->list '#(dah dah didah)))
(must-equal (dah) (vector->list '#(dah dah didah) 1 2))

(must-raise (assertion-violation vector->list) (vector->list))
(must-raise (assertion-violation vector->list) (vector->list #u8()))
(must-raise (assertion-violation vector->list) (vector->list '()))
(must-raise (assertion-violation vector->list) (vector->list #(1 2 3 4) #f))
(must-raise (assertion-violation vector->list) (vector->list #(1 2 3 4) -1))
(must-raise (assertion-violation vector->list) (vector->list #(1 2 3 4) 4))
(must-raise (assertion-violation vector->list) (vector->list #(1 2 3 4) 1 0))
(must-raise (assertion-violation vector->list) (vector->list #(1 2 3 4) 1 5))
(must-raise (assertion-violation vector->list) (vector->list #(1 2 3 4) 1 2 3))

(must-equal #(dididit dah) (list->vector '(dididit dah)))

(must-equal "123" (vector->string #(#\1 #\2 #\3)))
(must-equal "def" (vector->string #(#\a #\b #\c #\d #\e #\f #\g #\h) 3 6))
(must-equal "gh" (vector->string #(#\a #\b #\c #\d #\e #\f #\g #\h) 6 8))

(must-raise (assertion-violation vector->string) (vector->string))
(must-raise (assertion-violation vector->string) (vector->string '()))
(must-raise (assertion-violation vector->string) (vector->string #(#\a #\b #\c) #f))
(must-raise (assertion-violation vector->string) (vector->string #(#\a #\b #\c) -1))
(must-raise (assertion-violation vector->string) (vector->string #(#\a #\b #\c) 4))
(must-raise (assertion-violation vector->string) (vector->string #(#\a #\b #\c) 0 #f))
(must-raise (assertion-violation vector->string) (vector->string #(#\a #\b #\c) 0 4))
(must-raise (assertion-violation vector->string) (vector->string #(#\a #\b #\c) 1 2 3))

(must-equal #(#\A #\B #\C) (string->vector "ABC"))

(must-raise (assertion-violation string->vector) (string->vector))
(must-raise (assertion-violation string->vector) (string->vector '()))
(must-raise (assertion-violation string->vector) (string->vector "abc" #f))
(must-raise (assertion-violation string->vector) (string->vector "abc" -1))
(must-raise (assertion-violation string->vector) (string->vector "abc" 4))
(must-raise (assertion-violation string->vector) (string->vector "abc" 0 #f))
(must-raise (assertion-violation string->vector) (string->vector "abc" 0 4))
(must-raise (assertion-violation string->vector) (string->vector "abc" 1 2 3))

(define a #(1 8 2 8))
(define b (vector-copy a))
(must-equal #(1 8 2 8) b)
(vector-set! b 0 3)
(must-equal #(3 8 2 8) b)
(define c (vector-copy b 1 3))
(must-equal #(8 2) c)

(define v (vector 1 2 3 4))
(must-raise (assertion-violation vector-copy) (vector-copy))
(must-raise (assertion-violation vector-copy) (vector-copy 1))
(must-raise (assertion-violation vector-copy) (vector-copy v 1 2 1))
(must-raise (assertion-violation vector-copy) (vector-copy v -1 2))
(must-raise (assertion-violation vector-copy) (vector-copy v 3 2))
(must-raise (assertion-violation vector-copy) (vector-copy v 1 5))

(define a (vector 1 2 3 4 5))
(define b (vector 10 20 30 40 50))
(vector-copy! b 1 a 0 2)
(must-equal #(10 1 2 40 50) b)

(define x (vector 'a 'b 'c 'd 'e 'f 'g))
(vector-copy! x 1 x 0 3)
(must-equal #(a a b c e f g) x)

(define x (vector 'a 'b 'c 'd 'e 'f 'g))
(vector-copy! x 1 x 3 6)
(must-equal #(a d e f e f g) x)

(must-raise (assertion-violation vector-copy!) (vector-copy! a 0))
(must-raise (assertion-violation vector-copy!) (vector-copy! a 0 b 1 1 1))
(must-raise (assertion-violation vector-copy!) (vector-copy! 1 0 b))
(must-raise (assertion-violation vector-copy!) (vector-copy! a 0 1))
(must-raise (assertion-violation vector-copy!) (vector-copy! a -1 b))
(must-raise (assertion-violation vector-copy!) (vector-copy! a 3 b))
(must-raise (assertion-violation vector-copy!) (vector-copy! a 0 b -1))
(must-raise (assertion-violation vector-copy!) (vector-copy! a 0 b 1 0))
(must-raise (assertion-violation vector-copy!) (vector-copy! a 0 b 1 6))

(must-equal #(a b c d e f) (vector-append #(a b c) #(d e f)))
(must-raise (assertion-violation vector-append) (vector-append 1))
(must-raise (assertion-violation vector-append) (vector-append #(1 2) 1))

(define a (vector 1 2 3 4 5))
(vector-fill! a 'smash 2 4)
(must-equal #(1 2 smash smash 5) a)

(define v (vector 1 2 3 4))
(must-raise (assertion-violation vector-fill!) (vector-fill! 1))
(must-raise (assertion-violation vector-fill!) (vector-fill! 1 #f))
(must-raise (assertion-violation vector-fill!) (vector-fill! v #f 1 2 1))
(must-raise (assertion-violation vector-fill!) (vector-fill! v #f -1 2))
(must-raise (assertion-violation vector-fill!) (vector-fill! v #f 3 2))
(must-raise (assertion-violation vector-fill!) (vector-fill! v #f 1 5))

;;
;; ---- bytevectors ----

(must-equal #t (bytevector? #u8()))
(must-equal #t (bytevector? #u8(1 2)))
(must-equal #f (bytevector? #(1 2)))
(must-equal #f (bytevector? 12))
(must-raise (assertion-violation bytevector?) (bytevector?))
(must-raise (assertion-violation bytevector?) (bytevector? #u8() #u8()))

(must-equal #u8(12 12) (make-bytevector 2 12))
(must-equal #u8() (make-bytevector 0))
(must-equal 10 (bytevector-length (make-bytevector 10)))
(must-raise (assertion-violation make-bytevector) (make-bytevector))
(must-raise (assertion-violation make-bytevector) (make-bytevector -1))
(must-raise (assertion-violation make-bytevector) (make-bytevector 1 -1))
(must-raise (assertion-violation make-bytevector) (make-bytevector 1 256))
(must-raise (assertion-violation make-bytevector) (make-bytevector 1 1 1))

(must-equal #u8(1 3 5 1 3 5) (bytevector 1 3 5 1 3 5))
(must-equal #u8() (bytevector))
(must-raise (assertion-violation bytevector) (bytevector -1))
(must-raise (assertion-violation bytevector) (bytevector 256))
(must-raise (assertion-violation bytevector) (bytevector 10 20 -1))
(must-raise (assertion-violation bytevector) (bytevector 10 20 256 30))

(must-equal 0 (bytevector-length #u8()))
(must-equal 4 (bytevector-length #u8(1 2 3 4)))
(must-raise (assertion-violation bytevector-length) (bytevector-length))
(must-raise (assertion-violation bytevector-length) (bytevector-length 10))
(must-raise (assertion-violation bytevector-length) (bytevector-length #() #()))
(must-raise (assertion-violation bytevector-length) (bytevector-length #() 10))

(must-equal 8 (bytevector-u8-ref #u8(1 1 2 3 5 8 13 21) 5))
(must-raise (assertion-violation bytevector-u8-ref) (bytevector-u8-ref 1 1))
(must-raise (assertion-violation bytevector-u8-ref) (bytevector-u8-ref #(1 2 3)))
(must-raise (assertion-violation bytevector-u8-ref) (bytevector-u8-ref #(1 2 3) 1 1))
(must-raise (assertion-violation bytevector-u8-ref) (bytevector-u8-ref #(1 2 3) -1))
(must-raise (assertion-violation bytevector-u8-ref) (bytevector-u8-ref #(1 2 3) 3))

(must-equal #u8(1 3 3 4)
    (let ((bv (bytevector 1 2 3 4)))
        (bytevector-u8-set! bv 1 3)
        bv))

(define bv (bytevector 1 2 3 4))
(must-raise (assertion-violation bytevector-u8-set!) (bytevector-u8-set! 1 1 1))
(must-raise (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv 1))
(must-raise (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv 1 1 1))
(must-raise (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv -1 1))
(must-raise (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv 4 1))
(must-raise (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv 1 -1))
(must-raise (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv 1 256))

(define a #u8(1 2 3 4 5))
(must-equal #u8(3 4) (bytevector-copy a 2 4))

(define bv (bytevector 1 2 3 4))
(must-raise (assertion-violation bytevector-copy) (bytevector-copy))
(must-raise (assertion-violation bytevector-copy) (bytevector-copy 1))
(must-raise (assertion-violation bytevector-copy) (bytevector-copy bv 1 2 1))
(must-raise (assertion-violation bytevector-copy) (bytevector-copy bv -1 2))
(must-raise (assertion-violation bytevector-copy) (bytevector-copy bv 3 2))
(must-raise (assertion-violation bytevector-copy) (bytevector-copy bv 1 5))

(define a (bytevector 1 2 3 4 5))
(define b (bytevector 10 20 30 40 50))
(bytevector-copy! b 1 a 0 2)
(must-equal #u8(10 1 2 40 50) b)

(must-raise (assertion-violation bytevector-copy!) (bytevector-copy! a 0))
(must-raise (assertion-violation bytevector-copy!) (bytevector-copy! a 0 b 1 1 1))
(must-raise (assertion-violation bytevector-copy!) (bytevector-copy! 1 0 b))
(must-raise (assertion-violation bytevector-copy!) (bytevector-copy! a 0 1))
(must-raise (assertion-violation bytevector-copy!) (bytevector-copy! a -1 b))
(must-raise (assertion-violation bytevector-copy!) (bytevector-copy! a 3 b))
(must-raise (assertion-violation bytevector-copy!) (bytevector-copy! a 0 b -1))
(must-raise (assertion-violation bytevector-copy!) (bytevector-copy! a 0 b 1 0))
(must-raise (assertion-violation bytevector-copy!) (bytevector-copy! a 0 b 1 6))

(must-equal #u8(0 1 2 3 4 5) (bytevector-append #u8(0 1 2) #u8(3 4 5)))
(must-raise (assertion-violation bytevector-append) (bytevector-append 1))
(must-raise (assertion-violation bytevector-append) (bytevector-append #u8(1 2) 1))

(must-equal "ABCDE" (utf8->string #u8(65 66 67 68 69)))
(must-equal "CDE" (utf8->string #u8(65 66 67 68 69) 2))
(must-equal "BCD" (utf8->string #u8(65 66 67 68 69) 1 4))
(must-raise (assertion-violation utf8->string) (utf8->string))
(must-raise (assertion-violation utf8->string) (utf8->string 1))
(must-raise (assertion-violation utf8->string) (utf8->string #u8(65 66 67) 1 2 2))
(must-raise (assertion-violation utf8->string) (utf8->string #u8(65 66 67) -1))
(must-raise (assertion-violation utf8->string) (utf8->string #u8(65 66 67) 3))
(must-raise (assertion-violation utf8->string) (utf8->string #u8(65 66 67) 2 1))
(must-raise (assertion-violation utf8->string) (utf8->string #u8(65 66 67) 0 4))

(must-equal #u8(207 187) (string->utf8 (utf8->string #u8(207 187))))
(must-raise (assertion-violation string->utf8) (string->utf8))
(must-raise (assertion-violation string->utf8) (string->utf8 1))
(must-raise (assertion-violation string->utf8) (string->utf8 "ABC" 1 2 2))
(must-raise (assertion-violation string->utf8) (string->utf8 "ABC" -1))
(must-raise (assertion-violation string->utf8) (string->utf8 "ABC" 3))
(must-raise (assertion-violation string->utf8) (string->utf8 "ABC" 2 1))
(must-raise (assertion-violation string->utf8) (string->utf8 "ABC" 0 4))

;;
;; ---- control features ----
;;

;; procedure?

;; apply

(must-equal 7 (apply + (list 3 4)))

(define compose
    (lambda (f g)
        (lambda args
            (f (apply g args)))))
(must-equal 30 ((compose sqrt *) 12 75))

(must-equal 15 (apply + '(1 2 3 4 5)))
(must-equal 15 (apply + 1 '(2 3 4 5)))
(must-equal 15 (apply + 1 2 '(3 4 5)))
(must-equal 15 (apply + 1 2 3 4 5 '()))

(must-raise (assertion-violation apply) (apply +))
(must-raise (assertion-violation apply) (apply + '(1 2 . 3)))
(must-raise (assertion-violation apply) (apply + 1))

;; map

(define (cadr obj) (car (cdr obj)))
(must-equal (b e h) (map cadr '((a b) (d e) (g h))))
(must-equal (1 4 27 256 3125) (map (lambda (n) (expt n n)) '(1 2 3 4 5)))
(must-equal (5 7 9) (map + '(1 2 3) '(4 5 6 7)))

;; string-map

;; vector-map

;; for-each

(must-equal #(0 1 4 9 16)
    (let ((v (make-vector 5)))
        (for-each (lambda (i) (vector-set! v i (* i i))) '(0 1 2 3 4))
        v))

;; string-for-each

;; vector-for-each

;; call-with-current-continuation
;; call/cc

(must-equal -3 (call-with-current-continuation
    (lambda (exit)
        (for-each (lambda (x)
            (if (negative? x)
                (exit x)))
            '(54 0 37 -3 245 19))
        #t)))

(define list-length
    (lambda (obj)
        (call-with-current-continuation
            (lambda (return)
                (letrec ((r
                    (lambda (obj)
                        (cond ((null? obj) 0)
                            ((pair? obj) (+ (r (cdr obj)) 1))
                            (else (return #f))))))
                    (r obj))))))

(must-raise (assertion-violation call-with-current-continuation) (call/cc))
(must-raise (assertion-violation) (call/cc #t))
(must-raise (assertion-violation call-with-current-continuation) (call/cc #t #t))
(must-raise (assertion-violation call-with-current-continuation) (call/cc (lambda (e) e) #t))

(must-equal 4 (list-length '(1 2 3 4)))
(must-equal #f (list-length '(a b . c)))

(define (cc-values . things)
    (call/cc (lambda (cont) (apply cont things))))

(must-equal (1 2 3 4) (let-values (((a b) (cc-values 1 2))
                                    ((c d) (cc-values 3 4)))
                                (list a b c d)))
(define (v)
    (define (v1) (v3) (v2) (v2))
    (define (v2) (v3) (v3))
    (define (v3) (cc-values 1 2 3 4))
    (v1))
(must-equal (4 3 2 1) (let-values (((w x y z) (v))) (list z y x w)))

(must-equal 5 (call-with-values (lambda () (cc-values 4 5)) (lambda (a b) b)))

;; values

(define (v0) (values))
(define (v1) (values 1))
(define (v2) (values 1 2))

(must-equal 1 (+ (v1) 0))
(must-raise (assertion-violation values) (+ (v0) 0))
(must-raise (assertion-violation values) (+ (v2) 0))
(must-equal 1 (begin (v0) 1))
(must-equal 1 (begin (v1) 1))
(must-equal 1 (begin (v2) 1))

;; call-with-values

(must-equal 5 (call-with-values (lambda () (values 4 5)) (lambda (a b) b)))
(must-equal 4 (call-with-values (lambda () (values 4 5)) (lambda (a b) a)))
(must-equal -1 (call-with-values * -))

;; dynamic-wind

(must-equal (connect talk1 disconnect connect talk2 disconnect)
    (let ((path '())
            (c #f))
        (let ((add (lambda (s) (set! path (cons s path)))))
            (dynamic-wind
                (lambda () (add 'connect))
                (lambda ()
                    (add (call-with-current-continuation
                            (lambda (c0)
                                (set! c c0)
                                'talk1))))
                (lambda () (add 'disconnect)))
            (if (< (length path) 4)
                (c 'talk2)
                (reverse path)))))

;;
;; ---- exceptions ----

;; with-exception-handler

(define e #f)
(must-equal exception (call-with-current-continuation
    (lambda (k)
        (with-exception-handler
            (lambda (x) (set! e x) (k 'exception))
            (lambda () (+ 1 (raise 'an-error)))))))
(must-equal an-error e)

(must-equal (another-error)
    (guard (o ((eq? o 10) 10) (else (list o)))
        (with-exception-handler
            (lambda (x) (set! e x))
            (lambda ()
                (+ 1 (raise 'another-error))))))
(must-equal another-error e)

(must-equal 65
    (with-exception-handler
        (lambda (con)
            (cond
                ((string? con) (set! e con))
                (else (set! e "a warning has been issued")))
            42)
        (lambda ()
            (+ (raise-continuable "should be a number") 23))))
(must-equal "should be a number" e)

;;
;; ---- input and output ----
;;

;; write

(define (cddr o) (cdr (cdr o)))
(must-equal "#0=(a b c . #0#)"
    (let ((p (open-output-string)))
        (let ((x (list 'a 'b 'c)))
            (set-cdr! (cddr x) x)
            (write x p)
            (get-output-string p))))

(must-equal "#0=(val1 . #0#)"
    (let ((p (open-output-string)))
        (let ((a (cons 'val1 'val2)))
           (set-cdr! a a)
           (write a p)
           (get-output-string p))))

(must-equal "((a b c) a b c)"
    (let ((p (open-output-string)))
        (let ((x (list 'a 'b 'c)))
            (write (cons x x) p)
            (get-output-string p))))

;; write-shared

(must-equal "(#0=(a b c) . #0#)"
    (let ((p (open-output-string)))
        (let ((x (list 'a 'b 'c)))
            (write-shared (cons x x) p)
            (get-output-string p))))
