;;;
;;; R7RS
;;;

(import (scheme base))
(import (scheme case-lambda))
(import (scheme char))
(import (scheme eval))
(import (scheme file))
(import (scheme inexact))
(import (scheme lazy))
(import (scheme load))
(import (scheme process-context))
(import (scheme read))
(import (scheme repl))
(import (scheme time))
(import (scheme write))

(cond-expand ((library (chibi test)) (import (chibi test))) (else 'nothing))

(define-syntax check-equal
    (syntax-rules () ((check-equal expect expr) (test-propagate-info #f 'expect expr ()))))

(define-syntax check-syntax
    (syntax-rules () ((check-syntax error expr) (test-error (eval '(lambda () expr))))))

(define-syntax check-error
    (syntax-rules () ((check-error error expr) (test-error expr))))

;;
;; ---- identifiers ----
;;

(check-equal "..." (symbol->string '...))
(check-equal "+" (symbol->string '+))
(check-equal "+soup+" (symbol->string '+soup+))
(check-equal "<=?" (symbol->string '<=?))
(check-equal "->string" (symbol->string '->string))
(check-equal "a34kTMNs" (symbol->string 'a34kTMNs))
(check-equal "lambda" (symbol->string 'lambda))
(check-equal "list->vector" (symbol->string 'list->vector))
(check-equal "q" (symbol->string 'q))
(check-equal "V17a" (symbol->string 'V17a))
(check-equal "two words" (symbol->string '|two words|))
(check-equal "two words" (symbol->string '|two\x20;words|))
(check-equal "the-word-recursion-has-many-meanings"
        (symbol->string 'the-word-recursion-has-many-meanings))
(check-equal "\x3BB;" (symbol->string '|\x3BB;|))
(check-equal "" (symbol->string '||))
(check-equal ".." (symbol->string '..))

(check-equal "ABC" (symbol->string 'ABC))

#!fold-case

(check-equal "abc" (symbol->string 'ABC))

#!no-fold-case

(check-equal "ABC" (symbol->string 'ABC))

;;
;; ---- comments ----
;;

#;(bad food)
#;
    
(really bad food)

#| (bad code) #| in bad code |# |#

;;
;; ---- expressions ----
;;

;; quote

(check-equal a (quote a))
(check-equal #(a b c) (quote #(a b c)))
(check-equal (+ 1 2) (quote (+ 1 2)))

(check-equal a 'a)
(check-equal #(a b c) '#(a b c))
(check-equal () '())
(check-equal (+ 1 2) '(+ 1 2))
(check-equal (quote a) '(quote a))
(check-equal (quote a) ''a)

(check-equal 145932 '145932)
(check-equal 145932 145932)
(check-equal "abc" '"abc")
(check-equal "abc" "abc")
(check-equal #\a '#\a)
(check-equal #\a #\a)
(check-equal #(a 10) '#(a 10))
(check-equal #(a 10) #(a 10))
(check-equal #u8(64 65) '#u8(64 65))
(check-equal #u8(64 65) #u8(64 65))
(check-equal #t '#t)
(check-equal #t #t)
(check-equal #t #true)
(check-equal #f #false)

(check-equal #(a 10) ((lambda () '#(a 10))))
(check-equal #(a 10) ((lambda () #(a 10))))

(check-syntax (syntax-violation quote) (quote))
(check-syntax (syntax-violation quote) (quote . a))
(check-syntax (syntax-violation quote) (quote  a b))
(check-syntax (syntax-violation quote) (quote a . b))

;; procedure call

(check-equal 7 (+ 3 4))
(check-equal 12 ((if #f + *) 3 4))

(check-equal 0 (+))
(check-equal 12 (+ 12))
(check-equal 19 (+ 12 7))
(check-equal 23 (+ 12 7 4))
(check-syntax (syntax-violation procedure-call) (+ 12 . 7))

;; lambda

(check-equal 8 ((lambda (x) (+ x x)) 4))

(define reverse-subtract (lambda (x y) (- y x)))
(check-equal 3 (reverse-subtract 7 10))

(define add4 (let ((x 4)) (lambda (y) (+ x y))))
(check-equal 10 (add4 6))

(check-equal (3 4 5 6) ((lambda x x) 3 4 5 6))
(check-equal (5 6) ((lambda (x y . z) z) 3 4 5 6))

(check-equal 4 ((lambda () 4)))

(check-syntax (syntax-violation lambda) (lambda (x x) x))
(check-syntax (syntax-violation lambda) (lambda (x y z x) x))
(check-syntax (syntax-violation lambda) (lambda (z x y . x) x))
(check-syntax (syntax-violation lambda) (lambda (x 13 x) x))
(check-syntax (syntax-violation lambda) (lambda (x)))
(check-syntax (syntax-violation lambda) (lambda (x) . x))

(check-equal 12 ((lambda (a b)
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
(check-equal 11110 (q 10000))

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
(check-equal 4 (q 2))

(define (l x y) x)
(check-error (assertion-violation l) (l 3))
(check-error (assertion-violation l) (l 3 4 5))

(define (make-counter n)
    (lambda () n))
(define counter (make-counter 10))
(check-equal 10 (counter))

(check-equal 200
    (letrec ((recur (lambda (x max) (if (= x max) (+ x max) (recur (+ x 1) max))))) (recur 1 100)))

;; if

(check-equal yes (if (> 3 2) 'yes 'no))
(check-equal no (if (> 2 3) 'yes 'no))
(check-equal 1 (if (> 3 2) (- 3 2) (+ 3 2)))

(check-equal true (if #t 'true 'false))
(check-equal false (if #f 'true 'false))
(check-equal 11 (+ 1 (if #t 10 20)))
(check-equal 21 (+ 1 (if #f 10 20)))
(check-equal 11 (+ 1 (if #t 10)))
(check-error (assertion-violation +) (+ 1 (if #f 10)))

;; set!

(define x 2)
(check-equal 3 (+ x 1))
(set! x 4)
(check-equal 5 (+ x 1))

(define (make-counter n)
    (lambda () (set! n (+ n 1)) n))
(define c1 (make-counter 0))
(check-equal 1 (c1))
(check-equal 2 (c1))
(check-equal 3 (c1))
(define c2 (make-counter 100))
(check-equal 101 (c2))
(check-equal 102 (c2))
(check-equal 103 (c2))
(check-equal 4 (c1))

(define (q x)
    (set! x 10) x)
(check-equal 10 (q 123))

(define (q x)
    (define (r y)
        (set! x y))
    (r 10)
    x)
(check-equal 10 (q 123))

;; include

(check-equal (10 20) (begin (include "include.scm") (list INCLUDE-A include-b)))
(check-error (assertion-violation) (begin (include "include3.scm") include-c))

(check-equal 10 (let ((a 0) (B 0)) (set! a 1) (set! B 1) (include "include2.scm") a))
(check-equal 20 (let ((a 0) (B 0)) (set! a 1) (set! B 1) (include "include2.scm") B))

;; include-ci

(check-error (assertion-violation) (begin (include-ci "include4.scm") INCLUDE-E))
(check-equal (10 20) (begin (include-ci "include5.scm") (list include-g include-h)))

(check-equal 10 (let ((a 0) (b 0)) (set! a 1) (set! b 1) (include-ci "include2.scm") a))
(check-equal 20 (let ((a 0) (b 0)) (set! a 1) (set! b 1) (include-ci "include2.scm") b))

;; cond

(define (cadr obj) (car (cdr obj)))

(check-equal greater (cond ((> 3 2) 'greater) ((< 3 2) 'less)))
(check-equal equal (cond ((> 3 3) 'greater) ((< 3 3) 'less) (else 'equal)))
(check-equal 2 (cond ('(1 2 3) => cadr) (else #f)))
(check-equal 2 (cond ((assv 'b '((a 1) (b 2))) => cadr) (else #f)))

;; case

(check-equal composite (case (* 2 3) ((2 3 5 7) 'prime) ((1 4 6 8 9) 'composite)))
(check-equal c (case (car '(c d)) ((a) 'a) ((b) 'b)))
(check-equal vowel
    (case (car '(i d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant)))
(check-equal semivowel
    (case (car '(w d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant)))
(check-equal consonant
    (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant)))

(check-equal c (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel)
        (else => (lambda (x) x))))
(check-equal (vowel o) (case (car '(o d)) ((a e i o u) => (lambda (x) (list 'vowel x)))
        ((w y) 'semivowel)
        (else => (lambda (x) x))))
(check-equal (semivowel y) (case (car '(y d)) ((a e i o u) 'vowel)
        ((w y) => (lambda (x) (list 'semivowel x)))
        (else => (lambda (x) x))))

(check-equal composite
    (let ((ret (case (* 2 3) ((2 3 5 7) 'prime) ((1 4 6 8 9) 'composite)))) ret))
(check-equal c (let ((ret (case (car '(c d)) ((a) 'a) ((b) 'b)))) ret))
(check-equal vowel
    (let ((ret (case (car '(i d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant))))
        ret))
(check-equal semivowel
    (let ((ret (case (car '(w d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant))))
        ret))
(check-equal consonant
    (let ((ret (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel) (else 'consonant))))
        ret))

(check-equal c (let ((ret (case (car '(c d)) ((a e i o u) 'vowel) ((w y) 'semivowel)
        (else => (lambda (x) x))))) ret))
(check-equal (vowel o) (let ((ret (case (car '(o d)) ((a e i o u) => (lambda (x) (list 'vowel x)))
        ((w y) 'semivowel)
        (else => (lambda (x) x))))) ret))
(check-equal (semivowel y) (let ((ret (case (car '(y d)) ((a e i o u) 'vowel)
        ((w y) => (lambda (x) (list 'semivowel x)))
        (else => (lambda (x) x))))) ret))

(check-equal a
    (let ((ret 'nothing)) (case 'a ((a b c d) => (lambda (x) (set! ret x) x))
        ((e f g h) (set! ret 'efgh) ret) (else (set! ret 'else))) ret))
(check-equal efgh
    (let ((ret 'nothing)) (case 'f ((a b c d) => (lambda (x) (set! ret x) x))
        ((e f g h) (set! ret 'efgh) ret) (else (set! ret 'else))) ret))
(check-equal else
    (let ((ret 'nothing)) (case 'z ((a b c d) => (lambda (x) (set! ret x) x))
        ((e f g h) (set! ret 'efgh) ret) (else (set! ret 'else))) ret))

;; and

(check-equal #t (and (= 2 2) (> 2 1)))
(check-equal #f (and (= 2 2) (< 2 1)))
(check-equal (f g) (and 1 2 'c '(f g)))
(check-equal #t (and))

;; or

(check-equal #t (or (= 2 2) (> 2 1)))
(check-equal #t (or (= 2 2) (< 2 1)))
(check-equal #f (or #f #f #f))
(check-equal (b c) (or '(b c) (/ 3 0)))

(check-equal #t (let ((x (or (= 2 2) (> 2 1)))) x))
(check-equal #t (let ((x (or (= 2 2) (< 2 1)))) x))
(check-equal #f (let ((x (or #f #f #f))) x))
(check-equal (b c) (let ((x (or '(b c) (/ 3 0)))) x))

(check-equal 1 (let ((x 0)) (or (begin (set! x 1) #t) (begin (set! x 2) #t)) x))
(check-equal 2 (let ((x 0)) (or (begin (set! x 1) #f) (begin (set! x 2) #t) (/ 3 0)) x))

;; when

(check-equal greater (when (> 3 2) 'greater))

;; unless

(check-equal less (unless (< 3 2) 'less))

;; cond-expand

(check-equal 1
    (cond-expand (no-features 0) (r7rs 1) (full-unicode 2)))
(check-equal 2
    (cond-expand (no-features 0) (no-r7rs 1) (full-unicode 2)))
(check-equal 0
    (cond-expand ((not no-features) 0) (no-r7rs 1) (full-unicode 2)))

(check-equal 1
    (cond-expand ((and r7rs no-features) 0) ((and r7rs full-unicode) 1) (r7rs 2)))
(check-equal 0
    (cond-expand ((or no-features r7rs) 0) ((and r7rs full-unicode) 1) (r7rs 2)))

(check-equal 1
    (cond-expand ((and r7rs no-features) 0) (else 1)))

(check-syntax (syntax-violation cond-expand)
    (cond-expand ((and r7rs no-features) 0) (no-features 1)))

(check-equal 1 (cond-expand ((library (scheme base)) 1) (else 2)))
(check-equal 2 (cond-expand ((library (not a library)) 1) (else 2)))
(check-equal 1 (cond-expand ((library (lib ce1)) 1) (else 2)))

(check-equal 1
    (let ((x 0) (y 1) (z 2)) (cond-expand (no-features x) (r7rs y) (full-unicode z))))
(check-equal 2
    (let ((x 0) (y 1) (z 2)) (cond-expand (no-features x) (no-r7rs y) (full-unicode z))))
(check-equal 0
    (let ((x 0) (y 1) (z 2)) (cond-expand ((not no-features) x) (no-r7rs y) (full-unicode z))))

(check-equal 1
    (let ((x 0) (y 1) (z 2))
        (cond-expand ((and r7rs no-features) x) ((and r7rs full-unicode) y) (r7rs z))))
(check-equal 0
    (let ((x 0) (y 1) (z 2)) (cond-expand ((or no-features r7rs) x) ((and r7rs full-unicode) y) (r7rs z))))

(check-equal 1
    (let ((x 0) (y 1)) (cond-expand ((and r7rs no-features) 0) (else 1))))

(check-syntax (syntax-violation cond-expand)
    (let ((x 0) (y 1)) (cond-expand ((and r7rs no-features) x) (no-features y))))

(check-equal 1
    (let ((x 1) (y 2)) (cond-expand ((library (scheme base)) x) (else y))))
(check-equal 2
    (let ((x 1) (y 2)) (cond-expand ((library (not a library)) x) (else y))))

(check-equal 1
    (let ((x 1) (y 2)) (cond-expand ((library (lib ce2)) x) (else y))))

;; let

(check-equal 6 (let ((x 2) (y 3)) (* x y)))
(check-equal 35 (let ((x 2) (y 3)) (let ((x 7) (z (+ x y))) (* z x))))

(check-equal 2 (let ((a 2) (b 7)) a))
(check-equal 7 (let ((a 2) (b 7)) b))
(check-equal 2 (let ((a 2) (b 7)) (let ((a a) (b a)) a)))
(check-equal 2 (let ((a 2) (b 7)) (let ((a a) (b a)) b)))

(check-syntax (syntax-violation let) (let))
(check-syntax (syntax-violation let) (let (x 2) x))
(check-syntax (syntax-violation let) (let x x))
(check-syntax (syntax-violation let) (let ((x)) x))
(check-syntax (syntax-violation let) (let ((x) 2) x))
(check-syntax (syntax-violation let) (let ((x 2) y) x))
(check-syntax (syntax-violation let) (let ((x 2) . y) x))
(check-syntax (syntax-violation let) (let ((x 2) (x 3)) x))
(check-syntax (syntax-violation let) (let ((x 2) (y 1) (x 3)) x))
(check-syntax (syntax-violation let) (let ((x 2))))
(check-syntax (syntax-violation let) (let ((x 2)) . x))
(check-syntax (syntax-violation let) (let ((x 2)) y . x))
(check-syntax (syntax-violation let) (let (((x y z) 2)) y x))
(check-syntax (syntax-violation let) (let ((x 2) ("y" 3)) y))

;; let*

(check-equal 70 (let ((x 2) (y 3)) (let* ((x 7) (z (+ x y))) (* z x))))

(check-equal 2 (let* ((a 2) (b 7)) a))
(check-equal 7 (let* ((a 2) (b 7)) b))
(check-equal 4 (let ((a 2) (b 7)) (let* ((a (+ a a)) (b a)) b)))

(check-syntax (syntax-violation let*) (let*))
(check-syntax (syntax-violation let*) (let* (x 2) x))
(check-syntax (syntax-violation let*) (let* x x))
(check-syntax (syntax-violation let*) (let* ((x)) x))
(check-syntax (syntax-violation let*) (let* ((x) 2) x))
(check-syntax (syntax-violation let*) (let* ((x 2) y) x))
(check-syntax (syntax-violation let*) (let* ((x 2) . y) x))
(check-equal 3 (let* ((x 2) (x 3)) x))
(check-equal 3 (let* ((x 2) (y 1) (x 3)) x))
(check-syntax (syntax-violation let*) (let* ((x 2))))
(check-syntax (syntax-violation let*) (let* ((x 2)) . x))
(check-syntax (syntax-violation let*) (let* ((x 2)) y . x))
(check-syntax (syntax-violation let*) (let* (((x y z) 2)) y x))
(check-syntax (syntax-violation let*) (let* ((x 2) ("y" 3)) y))

;; letrec

(check-equal #t (letrec ((even? (lambda (n) (if (zero? n) #t (odd? (- n 1)))))
                        (odd? (lambda (n) (if (zero? n) #f (even? (- n 1))))))
                    (even? 88)))

(check-syntax (syntax-violation letrec) (letrec))
(check-syntax (syntax-violation letrec) (letrec (x 2) x))
(check-syntax (syntax-violation letrec) (letrec x x))
(check-syntax (syntax-violation letrec) (letrec ((x)) x))
(check-syntax (syntax-violation letrec) (letrec ((x) 2) x))
(check-syntax (syntax-violation letrec) (letrec ((x 2) y) x))
(check-syntax (syntax-violation letrec) (letrec ((x 2) . y) x))
(check-syntax (syntax-violation letrec) (letrec ((x 2) (x 3)) x))
(check-syntax (syntax-violation letrec) (letrec ((x 2) (y 1) (x 3)) x))
(check-syntax (syntax-violation letrec) (letrec ((x 2))))
(check-syntax (syntax-violation letrec) (letrec ((x 2)) . x))
(check-syntax (syntax-violation letrec) (letrec ((x 2)) y . x))
(check-syntax (syntax-violation letrec) (letrec (((x y z) 2)) y x))
(check-syntax (syntax-violation letrec) (letrec ((x 2) ("y" 3)) y))

;; letrec*

(check-equal 5 (letrec* ((p (lambda (x) (+ 1 (q (- x 1)))))
                        (q (lambda (y) (if (zero? y) 0 (+ 1 (p (- y 1))))))
                    (x (p 5))
                    (y x))
                    y))

(check-equal 45 (let ((x 5))
                (letrec* ((foo (lambda (y) (bar x y)))
                            (bar (lambda (a b) (+ (* a b) a))))
                    (foo (+ x 3)))))

(check-syntax (syntax-violation letrec*) (letrec*))
(check-syntax (syntax-violation letrec*) (letrec* (x 2) x))
(check-syntax (syntax-violation letrec*) (letrec* x x))
(check-syntax (syntax-violation letrec*) (letrec* ((x)) x))
(check-syntax (syntax-violation letrec*) (letrec* ((x) 2) x))
(check-syntax (syntax-violation letrec*) (letrec* ((x 2) y) x))
(check-syntax (syntax-violation letrec*) (letrec* ((x 2) . y) x))
(check-syntax (syntax-violation letrec*) (letrec* ((x 2) (x 3)) x))
(check-syntax (syntax-violation letrec*) (letrec* ((x 2) (y 1) (x 3)) x))
(check-syntax (syntax-violation letrec*) (letrec* ((x 2))))
(check-syntax (syntax-violation letrec*) (letrec* ((x 2)) . x))
(check-syntax (syntax-violation letrec*) (letrec* ((x 2)) y . x))
(check-syntax (syntax-violation letrec*) (letrec* (((x y z) 2)) y x))
(check-syntax (syntax-violation letrec*) (letrec* ((x 2) ("y" 3)) y))

;; let-values

(check-equal (1 2 3 4) (let-values (((a b) (values 1 2))
                                    ((c d) (values 3 4)))
                                (list a b c d)))
(check-equal (1 2 (3 4)) (let-values (((a b . c) (values 1 2 3 4)))
                                (list a b c)))
(check-equal (x y a b) (let ((a 'a) (b 'b) (x 'x) (y 'y))
                            (let-values (((a b) (values x y))
                                           ((x y) (values a b)))
                                    (list a b x y))))
(check-equal (1 2 3) (let-values ((x (values 1 2 3))) x))

(define (v)
    (define (v1) (v3) (v2) (v2))
    (define (v2) (v3) (v3))
    (define (v3) (values 1 2 3 4))
    (v1))
(check-equal (4 3 2 1) (let-values (((w x y z) (v))) (list z y x w)))

(check-syntax (syntax-violation let-values) (let-values))
(check-syntax (syntax-violation let-values) (let-values (x 2) x))
(check-syntax (syntax-violation let-values) (let-values x x))
(check-syntax (syntax-violation let-values) (let-values ((x)) x))
(check-syntax (syntax-violation let-values) (let-values ((x) 2) x))
(check-syntax (syntax-violation let-values) (let-values ((x 2) y) x))
(check-syntax (syntax-violation let-values) (let-values ((x 2) . y) x))
(check-syntax (syntax-violation let-values) (let-values ((x 2) (x 3)) x))
(check-syntax (syntax-violation let-values) (let-values ((x 2) (y 1) (x 3)) x))
(check-syntax (syntax-violation let-values) (let-values ((x 2))))
(check-syntax (syntax-violation let-values) (let-values ((x 2)) . x))
(check-syntax (syntax-violation let-values) (let-values ((x 2)) y . x))
(check-syntax (syntax-violation let-values) (let-values (((x 2 y z) 2)) y x))
(check-syntax (syntax-violation let-values) (let-values ((x 2) ("y" 3)) y))

;; let*-values

(check-equal (x y x y) (let ((a 'a) (b 'b) (x 'x) (y 'y))
                            (let*-values (((a b) (values x y))
                                            ((x y) (values a b)))
                                (list a b x y))))
(check-equal ((1 2 3) 4 5) (let*-values ((x (values 1 2 3)) (x (values x 4 5))) x))

(check-syntax (syntax-violation let*-values) (let*-values))
(check-syntax (syntax-violation let*-values) (let*-values (x 2) x))
(check-syntax (syntax-violation let*-values) (let*-values x x))
(check-syntax (syntax-violation let*-values) (let*-values ((x)) x))
(check-syntax (syntax-violation let*-values) (let*-values ((x) 2) x))
(check-syntax (syntax-violation let*-values) (let*-values ((x 2) y) x))
(check-syntax (syntax-violation let*-values) (let*-values ((x 2) . y) x))
(check-syntax (syntax-violation let*-values) (let*-values ((x 2))))
(check-syntax (syntax-violation let*-values) (let*-values ((x 2)) . x))
(check-syntax (syntax-violation let*-values) (let*-values ((x 2)) y . x))
(check-syntax (syntax-violation let*-values) (let*-values (((x 2 y z) 2)) y x))
(check-syntax (syntax-violation let*-values) (let*-values ((x 2) ("y" 3)) y))

;; begin

;; do

(define (range b e)
    (do ((r '() (cons e r))
        (e (- e 1) (- e 1)))
        ((< e b) r)))

(check-equal (3 4) (range 3 5))

(check-equal #(0 1 2 3 4)
    (do ((vec (make-vector 5))
        (i 0 (+ i 1)))
        ((= i 5) vec)
        (vector-set! vec i i)))

(check-equal 25
    (let ((x '(1 3 5 7 9)))
        (do ((x x (cdr x))
            (sum 0 (+ sum (car x))))
            ((null? x) sum))))

;; delay

(check-equal 3 (force (delay (+ 1 2))))
(check-equal (3 3) (let ((p (delay (+ 1 2))))
    (list (force p) (force p))))

(define integers
    (letrec ((next (lambda (n) (delay (cons n (next (+ n 1)))))))
        (next 0)))
(define (head stream) (car (force stream)))
(define (tail stream) (cdr (force stream)))
(check-equal 2 (head (tail (tail integers))))

(define (stream-filter p? s)
    (delay-force
        (if (null? (force s))
            (delay '())
            (let ((h (car (force s)))
                    (t (cdr (force s))))
                (if (p? h)
                    (delay (cons h (stream-filter p? t)))
                    (stream-filter p? t))))))
(check-equal 5 (head (tail (tail (stream-filter odd? integers)))))

(define count 0)
(define p
    (delay
        (begin
            (set! count (+ count 1))
            (if (> count x)
                count
                (force p)))))
(define x 5)
(check-equal 6 (force p))
(set! x 10)
(check-equal 6 (force p))

(check-equal 10 (force (delay (+ 1 2 3 4))))
(check-equal 10 (force (+ 1 2 3 4)))

(check-error (assertion-violation force) (force))
(check-error (assertion-violation force) (force 1 2))

(check-equal #t (promise? (delay (+ 1 2 3 4))))
(check-equal #f (promise? (+ 1 2 3 4)))

(check-error (assertion-violation promise?) (promise?))
(check-error (assertion-violation promise?) (promise? 1 2))

(check-equal #t (promise? (make-promise 10)))

(define p (delay (+ 1 2 3 4)))
(check-equal #t (eq? p (make-promise p)))

(check-error (assertion-violation make-promise) (make-promise))
(check-error (assertion-violation make-promise) (make-promise 1 2))

;; named let

(check-equal ((6 1 3) (-5 -2))
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

(check-equal (0 1 2) (range 3))
(check-equal (3 4) (range 3 5))
(check-error (assertion-violation case-lambda) (range 1 2 3))

(define cl-tst
    (case-lambda
        (() 'none)
        ((a) 'one)
        ((b c) 'two)
        ((b . d) 'rest)))

(check-equal none (cl-tst))
(check-equal one (cl-tst 0))
(check-equal two (cl-tst 0 0))
(check-equal rest (cl-tst 0 0 0))

(define cl-tst2
    (case-lambda
        ((a b) 'two)
        ((c d e . f) 'rest)))

(check-error (assertion-violation case-lambda) (cl-tst2 0))
(check-equal two (cl-tst2 0 0))
(check-equal rest (cl-tst2 0 0 0))
(check-equal rest (cl-tst2 0 0 0 0))

(check-error (assertion-violation case-lambda)
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl)))
(check-equal one
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0)))
(check-equal two
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0 0)))
(check-equal rest
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0 0 0)))
(check-equal rest
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (cl 0 0 0 0)))

(check-equal one (let ((cl (case-lambda ((a) 'one) ((a b) 'two)))) (eq? cl cl) (cl 1)))

;; parameterize

(define radix
    (make-parameter 10
        (lambda (x)
            (if (and (exact-integer? x) (<= 2 x 16))
                x
                (error "invalid radix")))))

(define (f n) (number->string n (radix)))
(check-equal "12" (f 12))
(check-equal "1100" (parameterize ((radix 2)) (f 12)))
(check-equal "12" (f 12))
(radix 16)
(check-error (assertion-violation error) (parameterize ((radix 0)) (f 12)))

;; guard

(check-equal 42
    (guard
        (condition
            ((assq 'a condition) => cdr)
            ((assq 'b condition)))
        (raise (list (cons 'a 42)))))

(check-equal (b . 23)
    (guard
        (condition
            ((assq 'a condition) => cdr)
            ((assq 'b condition)))
        (raise (list (cons 'b 23)))))

(check-equal else (guard (excpt ((= excpt 10) (- 10)) (else 'else)) (raise 11)))

(check-equal 121 (with-exception-handler
    (lambda (obj) (* obj obj))
    (lambda ()
        (guard (excpt ((= excpt 10) (- 10)) ((= excpt 12) (- 12))) (raise 11)))))

;; quasiquote

(check-equal (list 3 4) `(list ,(+ 1 2) 4))
(check-equal (list a (quote a)) (let ((name 'a)) `(list ,name ',name)))

(check-equal (a 3 4 5 6 b) `(a ,(+ 1 2) ,@(map abs '(4 -5 6)) b))
(check-equal ((foo 7) . cons) `((foo ,(- 10 3)) ,@(cdr '(c)) . ,(car '(cons))))
(check-equal #(10 5 2 4 3 8) `#(10 5 ,(sqrt 4) ,@(map sqrt '(16 9)) 8))
(check-equal (list foo bar baz) (let ((foo '(foo bar)) (@baz 'baz)) `(list ,@foo , @baz)))

(check-equal (a `(b ,(+ 1 2) ,(foo 4 d) e) f) `(a `(b ,(+ 1 2) ,(foo ,(+ 1 3) d) e) f))
(check-equal (a `(b ,x ,'y d) e) (let ((name1 'x) (name2 'y)) `(a `(b ,,name1 ,',name2 d) e)))

(check-equal (list 3 4) (quasiquote (list (unquote (+ 1 2)) 4)))
(check-equal `(list ,(+ 1 2) 4) '(quasiquote (list (unquote (+ 1 2)) 4)))

;; define

(define add3 (lambda (x) (+ x 3)))
(check-equal 6 (add3 3))

(define first car)
(check-equal 1 (first '(1 2)))

(check-equal 45 (let ((x 5))
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
(check-equal 4 (sequence 1 2 3 4))
(check-equal 4 ((lambda () (be-like-begin sequence) (sequence 1 2 3 4))))

(check-equal ok (let ((=> #f)) (cond (#t => 'ok))))

(define-syntax tst-sr-1
    (syntax-rules ()
        ((_ _ _ expr) expr)))

(check-equal 10 (tst-sr-1 ignore both (+ 1 2 3 4)))

(define-syntax tst-sr-2
    (syntax-rules (_)
        ((_ _ expr) expr)))

(check-equal 10 (tst-sr-2 _ (+ 1 2 3 4)))

;; let-syntax

(check-equal now (let-syntax
                    ((when (syntax-rules ()
                        ((when tst stmt1 stmt2 ...)
                            (if tst
                                (begin stmt1 stmt2 ...))))))
                    (let ((if #t))
                        (when if (set! if 'now))
                        if)))

(check-equal outer (let ((x 'outer))
                      (let-syntax ((m (syntax-rules () ((m) x))))
                            (let ((x 'inner))
                                (m)))))

;; letrec-syntax

(check-equal 7 (letrec-syntax
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

(check-equal #t (pare? (kons 1 2)))
(check-equal #f (pare? (cons 1 2)))
(check-equal 1 (kar (kons 1 2)))
(check-equal 2 (kdr (kons 1 2)))
(check-equal 3
    (let ((k (kons 1 2)))
        (set-kar! k 3)
        (kar k)))

(check-equal 3
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

(check-equal 100 (begin (import (lib a b c)) lib-a-b-c))
(check-equal 10 (begin (import (lib t1)) lib-t1-a))
(check-error (assertion-violation) lib-t1-b)
(check-equal 20 b-lib-t1)
(check-error (assertion-violation) lib-t1-c)

(check-equal 10 (begin (import (lib t2)) (lib-t2-a)))
(check-equal 20 (lib-t2-b))
(check-syntax (syntax-violation) (import (lib t3)))
(check-syntax (syntax-violation) (import (lib t4)))

(check-equal 1000 (begin (import (lib t5)) (lib-t5-b)))
(check-equal 1000 lib-t5-a)

(check-equal 1000 (begin (import (lib t6)) (lib-t6-b)))
(check-equal 1000 (lib-t6-a))

;(check-equal 1000 (begin (import (lib t7)) (lib-t7-b)))
;(check-equal 1000 lib-t7-a)

(check-equal 1 (begin (import (only (lib t8) lib-t8-a lib-t8-c)) lib-t8-a))
(check-error (assertion-violation) lib-t8-b)
(check-equal 3 lib-t8-c)
(check-error (assertion-violation) lib-t8-d)

(check-equal 1 (begin (import (except (lib t9) lib-t9-b lib-t9-d)) lib-t9-a))
(check-error (assertion-violation) lib-t9-b)
(check-equal 3 lib-t9-c)
(check-error (assertion-violation) lib-t9-d)

(check-equal 1 (begin (import (prefix (lib t10) x)) xlib-t10-a))
(check-error (assertion-violation) lib-t10-b)
(check-equal 3 xlib-t10-c)
(check-error (assertion-violation) lib-t10-d)

(check-equal 1 (begin (import (rename (lib t11) (lib-t11-b b-lib-t11) (lib-t11-d d-lib-t11)))
    lib-t11-a))
(check-error (assertion-violation) lib-t11-b)
(check-equal 2 b-lib-t11)
(check-equal 3 lib-t11-c)
(check-error (assertion-violation) lib-t11-d)
(check-equal 4 d-lib-t11)

(check-syntax (syntax-violation) (import bad "bad library" name))
(check-syntax (syntax-violation)
    (define-library (no ("good") "library") (import (scheme base)) (export +)))

(check-syntax (syntax-violation) (import (lib t12)))
(check-syntax (syntax-violation) (import (lib t13)))

(check-equal 10 (begin (import (lib t14)) (lib-t14-a 10 20)))
(check-equal 10 (lib-t14-b 10 20))

(check-equal 10 (begin (import (lib t15)) (lib-t15-a 10 20)))
(check-equal 10 (lib-t15-b 10 20))

;;
;; ---- equivalence predicates ----
;;

(check-equal #t (eqv? 'a 'a))
(check-equal #f (eqv? 'a 'b))
(check-equal #t (eqv? 2 2))
;(check-equal #f (eqv? 2 2.0))
(check-equal #t (eqv? '() '()))
(check-equal #t (eqv? 100000000 100000000))
;(check-equal #f (eqv? 0.0 +nan.0))
(check-equal #f (eqv? (cons 1 2) (cons 1 2)))
(check-equal #f (eqv? (lambda () 1) (lambda () 2)))
(check-equal #t (let ((p (lambda (x) x))) (eqv? p p)))
(check-equal #f (eqv? #f 'nil))

(define gen-counter
    (lambda ()
        (let ((n 0))
            (lambda () (set! n (+ n 1)) n))))
(check-equal #t (let ((g (gen-counter))) (eqv? g g)))
(check-equal #f (eqv? (gen-counter) (gen-counter)))

(define gen-loser
    (lambda ()
        (let ((n 0))
            (lambda () (set! n (+ n 1)) 27))))
(check-equal #t (let ((g (gen-loser))) (eqv? g g)))

(check-equal #f (letrec ((f (lambda () (if (eqv? f g) 'f 'both)))
        (g (lambda () (if (eqv? f g) 'g 'both)))) (eqv? f g)))
(check-equal #t (let ((x '(a))) (eqv? x x)))

(check-error (assertion-violation eqv?) (eqv? 1))
(check-error (assertion-violation eqv?) (eqv? 1 2 3))

(check-equal #t (eq? 'a 'a))
(check-equal #f (eq? (list 'a) (list 'a)))
(check-equal #t (eq? '() '()))
(check-equal #t (eq? car car))
(check-equal #t (let ((x '(a))) (eq? x x)))
(check-equal #t (let ((x '#())) (eq? x x)))
(check-equal #t (let ((p (lambda (x) x))) (eq? p p)))

(check-error (assertion-violation eq?) (eq? 1))
(check-error (assertion-violation eq?) (eq? 1 2 3))

(check-equal #t (equal? 'a 'a))
(check-equal #t (equal? '(a) '(a)))
(check-equal #t (equal? '(a (b) c) '(a (b) c)))
(check-equal #t (equal? "abc" "abc"))
(check-equal #t (equal? 2 2))
(check-equal #t (equal? (make-vector 5 'a) (make-vector 5 'a)))
;(check-equal #t (equal? '#1=(a b . #1#) '#2=(a b a b . #2#)))

(check-error (assertion-violation equal?) (equal? 1))
(check-error (assertion-violation equal?) (equal? 1 2 3))

;;
;; ---- booleans ----
;;

(check-equal #f (not #t))
(check-equal #f (not 3))
(check-equal #f (not (list 3)))
(check-equal #t (not #f))
(check-equal #f (not '()))
(check-equal #f (not (list)))
(check-equal #f (not 'nil))

(check-error (assertion-violation not) (not))
(check-error (assertion-violation not) (not 1 2))

(check-equal #t (boolean? #f))
(check-equal #f (boolean? 0))
(check-equal #f (boolean? '()))

(check-error (assertion-violation boolean?) (boolean?))
(check-error (assertion-violation boolean?) (boolean? 1 2))

(check-equal #t (boolean=? #t #t))
(check-equal #t (boolean=? #t #t #t))
(check-equal #t (boolean=? #f #f))
(check-equal #t (boolean=? #f #f #f))
(check-equal #f (boolean=? #f #t))
(check-equal #f (boolean=? #t #t #f))

(check-error (assertion-violation boolean=?) (boolean=? #f))
(check-error (assertion-violation boolean=?) (boolean=? #f 1))
(check-error (assertion-violation boolean=?) (boolean=? #f #f 1))

;;
;; ---- pairs and lists ----
;;

(check-equal #t (pair? '(a . b)))
(check-equal #t (pair? '(a b c)))
(check-equal #f (pair? '()))
(check-equal #f (pair? '#(a b)))

(check-error (assertion-violation pair?) (pair?))
(check-error (assertion-violation pair?) (pair? 1 2))

(check-equal (a) (cons 'a '()))
(check-equal ((a) b c d) (cons '(a) '(b c d)))
(check-equal ("a" b c) (cons "a" '(b c)))
(check-equal (a . 3) (cons 'a 3))
(check-equal ((a b) . c) (cons '(a b) 'c))

(check-error (assertion-violation cons) (cons 1))
(check-error (assertion-violation cons) (cons 1 2 3))

(check-equal a (car '(a b c)))
(check-equal (a) (car '((a) b c d)))
(check-equal 1 (car '(1 . 2)))

(check-error (assertion-violation car) (car '()))
(check-error (assertion-violation car) (car))
(check-error (assertion-violation car) (car '(a) 2))

(check-equal (b c d) (cdr '((a) b c d)))
(check-equal 2 (cdr '(1 . 2)))

(check-error (assertion-violation cdr) (cdr '()))
(check-error (assertion-violation cdr) (cdr))
(check-error (assertion-violation cdr) (cdr '(a) 2))

(check-equal b (let ((c (cons 'a 'a))) (set-car! c 'b) (car c)))

(check-error (assertion-violation set-car!) (set-car! (cons 1 2)))
(check-error (assertion-violation set-car!) (set-car! 1 2))
(check-error (assertion-violation set-car!) (set-car! (cons 1 2) 2 3))

(check-equal b (let ((c (cons 'a 'a))) (set-cdr! c 'b) (cdr c)))

(check-error (assertion-violation set-cdr!) (set-cdr! (cons 1 2)))
(check-error (assertion-violation set-cdr!) (set-cdr! 1 2))
(check-error (assertion-violation set-cdr!) (set-cdr! (cons 1 2) 2 3))

(check-equal #t (null? '()))
(check-equal #f (null? 'nil))
(check-equal #f (null? #f))

(check-error (assertion-violation null?) (null?))
(check-error (assertion-violation null?) (null? 1 2))

(check-equal #t (list? '(a b c)))
(check-equal #t (list? '()))
(check-equal #f (list? '(a . b)))
(check-equal #f
    (let ((x (list 'a)))
        (set-cdr! x x)
        (list? x)))

(check-error (assertion-violation list?) (list?))
(check-error (assertion-violation list?) (list? 1 2))

(check-equal (3 3) (make-list 2 3))

(check-error (assertion-violation make-list) (make-list))
(check-error (assertion-violation make-list) (make-list -1))
(check-error (assertion-violation make-list) (make-list 'ten))
(check-error (assertion-violation make-list) (make-list 1 2 3))

(check-equal (a 7 c) (list 'a (+ 3 4) 'c))
(check-equal () (list))

(check-equal 3 (length '(a b c)))
(check-equal 3 (length '(a (b) (c d e))))
(check-equal 0 (length '()))

(check-error (assertion-violation length) (length))
(check-error (assertion-violation length)
    (let ((x (list 'a)))
        (set-cdr! x x)
        (length x)))
(check-error (assertion-violation length) (length '() 2))

(check-equal (x y) (append '(x) '(y)))
(check-equal (a b c d) (append '(a) '(b c d)))
(check-equal (a (b) (c)) (append '(a (b)) '((c))))
(check-equal (a b c . d) (append '(a b) '(c . d)))
(check-equal a (append '() 'a))

(check-error (assertion-violation append) (append))
(check-error (assertion-violation append)
    (let ((x (list 'a)))
        (set-cdr! x x)
        (append x 'a)))

(check-equal (c b a) (reverse '(a b c)))
(check-equal ((e (f)) d (b c) a) (reverse '(a (b c) d (e (f)))))

(check-error (assertion-violation reverse) (reverse))
(check-error (assertion-violation reverse) (reverse '(a b c) 2))
(check-error (assertion-violation reverse)
    (let ((x (list 'a)))
        (set-cdr! x x)
        (reverse x)))

(check-equal (1 2 3) (list-tail '(1 2 3) 0))
(check-equal (2 3) (list-tail '(1 2 3) 1))
(check-equal (3) (list-tail '(1 2 3) 2))
(check-equal () (list-tail '(1 2 3) 3))

(check-error (assertion-violation list-tail) (list-tail '(1 2 3)))
(check-error (assertion-violation list-tail) (list-tail '(1 2 3) 2 3))
(check-error (assertion-violation list-tail) (list-tail 'a 1))
(check-error (assertion-violation list-tail) (list-tail '(a) -1))
(check-error (assertion-violation list-tail) (list-tail '(1 2 3) 4))
(check-error (assertion-violation list-tail)
    (let ((x (list 'a)))
        (set-cdr! x x)
        (list-tail x 1)))

(check-equal c (list-ref '(a b c d) 2))

(check-error (assertion-violation list-ref) (list-ref '(1 2 3)))
(check-error (assertion-violation list-ref) (list-ref '(1 2 3) 2 3))
(check-error (assertion-violation list-ref) (list-ref '(1 2 3) -1))
(check-error (assertion-violation list-ref) (list-ref '(1 2 3) 3))
(check-error (assertion-violation list-ref)
    (let ((x (list 'a)))
        (set-cdr! x x)
        (list-ref x 1)))

(check-equal (a b #\c d)
    (let ((lst (list 'a 'b 'c 'd)))
        (list-set! lst 2 #\c)
        lst))
(check-equal (one two three)
    (let ((ls (list 'one 'two 'five!)))
        (list-set! ls 2 'three)
        ls))

(check-error (assertion-violation list-set!) (list-set! (list 1 2 3) 1))
(check-error (assertion-violation list-set!) (list-set! (list 1 2 3) 2 3 4))
(check-error (assertion-violation list-set!) (list-set! (list 1 2 3) -1 3))
(check-error (assertion-violation list-set!) (list-set! (list 1 2 3) 3 3))
(check-error (assertion-violation list-set!)
    (let ((x (list 'a)))
        (set-cdr! x x)
        (list-set! x 1 #t)))

(check-equal (a b c) (memq 'a '(a b c)))
(check-equal (b c) (memq 'b '(a b c)))
(check-equal #f (memq 'a '(b c d)))
(check-equal #f (memq (list 'a) '(b (a) c)))

(check-error (assertion-violation memq) (memq 'a))
(check-error (assertion-violation memq) (memq 'a '(a b c) 3))
(check-error (assertion-violation memq) (memq 'a 'b))
(check-error (assertion-violation memq)
    (let ((x (list 'a)))
        (set-cdr! x x)
        (memq 'a x)))

(check-equal (a b c) (memv 'a '(a b c)))
(check-equal (b c) (memv 'b '(a b c)))
(check-equal #f (memv 'a '(b c d)))
(check-equal #f (memv (list 'a) '(b (a) c)))
(check-equal (101 102) (memv 101 '(100 101 102)))

(check-error (assertion-violation memv) (memv 'a))
(check-error (assertion-violation memv) (memv 'a '(a b c) 3))
(check-error (assertion-violation memv) (memv 'a 'b))
(check-error (assertion-violation memv)
    (let ((x (list 'a)))
        (set-cdr! x x)
        (memv 'a x)))

(check-equal ((a) c) (member (list 'a) '(b (a) c)))
(check-equal ("b" "c") (member "B" '("a" "b" "c") string-ci=?))

(check-error (assertion-violation case-lambda) (member 'a))
(check-error (assertion-violation case-lambda) (member 'a '(a b c) 3 4))
(check-error (assertion-violation %member) (member 'a 'b))
(check-error (assertion-violation %member)
    (let ((x (list 'a)))
        (set-cdr! x x)
        (member 'a x)))

(check-error (assertion-violation member) (member 'a 'b string=?))
(check-error (assertion-violation member)
    (let ((x (list 'a)))
        (set-cdr! x x)
        (member 'a x string=?)))

(define e '((a 1) (b 2) (c 3)))
(check-equal (a 1) (assq 'a e))
(check-equal (b 2) (assq 'b e))
(check-equal #f (assq 'd e))
(check-equal #f (assq (list 'a) '(((a)) ((b)) ((c)))))

(check-error (assertion-violation assq) (assq 'a))
(check-error (assertion-violation assq) (assq 'a '(a b c) 3))
(check-error (assertion-violation assq) (assq 'a 'b))
(check-error (assertion-violation assq)
    (let ((x (list '(a))))
        (set-cdr! x x)
        (assq 'a x)))

(check-equal (5 7) (assv 5 '((2 3) (5 7) (11 13))))
(check-equal (a 1) (assv 'a e))
(check-equal (b 2) (assv 'b e))
(check-equal #f (assv 'd e))
(check-equal #f (assv (list 'a) '(((a)) ((b)) ((c)))))

(check-error (assertion-violation assv) (assv 'a))
(check-error (assertion-violation assv) (assv 'a '(a b c) 3))
(check-error (assertion-violation assv) (assv 'a 'b))
(check-error (assertion-violation assv)
    (let ((x (list '(a))))
        (set-cdr! x x)
        (assv 'a x)))

(check-equal ((a)) (assoc (list 'a) '(((a)) ((b)) ((c)))))
(check-equal #f (assoc (list 'd) '(((a)) ((b)) ((c)))))
;(check-equal (2 4) (assoc 2.0 '((1 1) (2 4) (3 9))))
(check-equal ("B" . b) (assoc "b" (list '("A" . a) '("B" . b) '("C" . c)) string-ci=?))
(check-equal #f (assoc "d" (list '("A" . a) '("B" . b) '("C" . c)) string-ci=?))

(check-error (assertion-violation case-lambda) (assoc 'a))
(check-error (assertion-violation case-lambda) (assoc 'a '(a b c) 3 4))
(check-error (assertion-violation %assoc) (assoc 'a 'b))
(check-error (assertion-violation %assoc)
    (let ((x (list '(a))))
        (set-cdr! x x)
        (assoc 'a x)))

(check-error (assertion-violation assoc) (assoc 'a 'b string=?))
(check-error (assertion-violation assoc)
    (let ((x (list '(a))))
        (set-cdr! x x)
        (assoc 'a x string=?)))

(define a '(1 8 2 8))
(define b (list-copy a))
(set-car! b 3)
(check-equal (3 8 2 8) b)
(check-equal (1 8 2 8) a)
(check-equal () (list-copy '()))
(check-equal 123 (list-copy 123))
(check-equal (a b c d) (list-copy '(a b c d)))

;;
;; ---- symbols ----
;;

(check-equal #t (symbol? 'foo))
(check-equal #t (symbol? (car '(a b))))
(check-equal #f (symbol? "bar"))
(check-equal #t (symbol? 'nil))
(check-equal #f (symbol? '()))
(check-equal #f (symbol? #f))

(check-error (assertion-violation symbol?) (symbol?))
(check-error (assertion-violation symbol?) (symbol? 1 2))

(check-equal #t (symbol=? 'a 'a (string->symbol "a")))
(check-equal #f (symbol=? 'a 'b))

(check-error (assertion-violation symbol=?) (symbol=? 'a))
(check-error (assertion-violation symbol=?) (symbol=? 'a 1))
(check-error (assertion-violation symbol=?) (symbol=? 'a 'a 1))

(check-equal "flying-fish" (symbol->string 'flying-fish))
(check-equal "Martin" (symbol->string 'Martin))
(check-equal "Malvina" (symbol->string (string->symbol "Malvina")))

(check-error (assertion-violation symbol->string) (symbol->string))
(check-error (assertion-violation symbol->string) (symbol->string "a string"))
(check-error (assertion-violation symbol->string) (symbol->string 'a 'a))

(check-equal mISSISSIppi (string->symbol "mISSISSIppi"))
(check-equal #t (eqv? 'bitBlt (string->symbol "bitBlt")))
(check-equal #t (eqv? 'LollyPop (string->symbol (symbol->string 'LollyPop))))
(check-equal #t (string=? "K. Harper, M.D." (symbol->string (string->symbol "K. Harper, M.D."))))

(check-error (assertion-violation string->symbol) (string->symbol))
(check-error (assertion-violation string->symbol) (string->symbol 'a))
(check-error (assertion-violation string->symbol) (string->symbol "a" "a"))

(check-equal #t (eq? '|H\x65;llo| 'Hello))
(check-equal #t (eq? '|\x9;\x9;| '|\t\t|))

;;
;; ---- characters ----
;;

(check-equal #\x7f #\delete)
(check-equal #\x1B #\escape)

(check-equal #t (char? #\a))
(check-equal #t (char? #\x03BB))
(check-equal #f (char? #x03BB))
(check-equal #f (char? "a"))

(check-error (assertion-violation char?) (char?))
(check-error (assertion-violation char?) (char? #\a 10))

(check-equal #t (char=? #\delete #\x7f))
(check-equal #t (char=? #\a #\a))
(check-equal #t (char=? #\a #\a #\a))
(check-equal #f (char=? #\a #\a #\b))

(check-error (assertion-violation char=?) (char=? #\a))
(check-error (assertion-violation char=?) (char=? #\a #\a 10))

(check-equal #t (char<? #\a #\b))
(check-equal #t (char<? #\a #\b #\c))
(check-equal #f (char<? #\a #\b #\c #\c))
(check-equal #f (char<? #\b #\b #\c))

(check-error (assertion-violation char<?) (char<? #\a))
(check-error (assertion-violation char<?) (char<? #\a #\b 10))

(check-equal #t (char>? #\b #\a))
(check-equal #t (char>? #\c #\b #\a))
(check-equal #f (char>? #\c #\b #\a #\a))
(check-equal #f (char>? #\b #\b #\a))

(check-error (assertion-violation char>?) (char>? #\a))
(check-error (assertion-violation char>?) (char>? #\b #\a 10))

(check-equal #t (char<=? #\a #\b #\b #\c #\d))
(check-equal #f (char<=? #\a #\c #\d #\a))

(check-error (assertion-violation char<=?) (char<=? #\a))
(check-error (assertion-violation char<=?) (char<=? #\a #\a 10))

(check-equal #t (char>=? #\d #\c #\c #\b #\a))
(check-equal #f (char>=? #\d #\c #\b #\c))

(check-error (assertion-violation char>=?) (char>=? #\a))
(check-error (assertion-violation char>=?) (char>=? #\a #\a 10))

(check-equal #t (char-ci=? #\delete #\x7f))
(check-equal #t (char-ci=? #\a #\a))
(check-equal #t (char-ci=? #\a #\A #\a))
(check-equal #f (char-ci=? #\a #\a #\b))

(check-error (assertion-violation char-ci=?) (char-ci=? #\a))
(check-error (assertion-violation char-ci=?) (char-ci=? #\a #\a 10))

(check-equal #t (char-ci<? #\a #\b))
(check-equal #t (char-ci<? #\a #\B #\c))
(check-equal #f (char-ci<? #\a #\b #\c #\c))
(check-equal #f (char-ci<? #\b #\B #\c))

(check-error (assertion-violation char-ci<?) (char-ci<? #\a))
(check-error (assertion-violation char-ci<?) (char-ci<? #\a #\b 10))

(check-equal #t (char-ci>? #\b #\a))
(check-equal #t (char-ci>? #\c #\B #\a))
(check-equal #f (char-ci>? #\c #\b #\a #\a))
(check-equal #f (char-ci>? #\b #\B #\a))

(check-error (assertion-violation char-ci>?) (char-ci>? #\a))
(check-error (assertion-violation char-ci>?) (char-ci>? #\b #\a 10))

(check-equal #t (char-ci<=? #\a #\B #\b #\c #\d))
(check-equal #f (char-ci<=? #\a #\c #\d #\A))

(check-error (assertion-violation char-ci<=?) (char-ci<=? #\a))
(check-error (assertion-violation char-ci<=?) (char-ci<=? #\a #\a 10))

(check-equal #t (char-ci>=? #\d #\c #\C #\b #\a))
(check-equal #f (char-ci>=? #\d #\C #\b #\c))

(check-error (assertion-violation char-ci>=?) (char-ci>=? #\a))
(check-error (assertion-violation char-ci>=?) (char-ci>=? #\a #\a 10))

(check-equal #f (char-alphabetic? #\tab))
(check-equal #t (char-alphabetic? #\a))
(check-equal #t (char-alphabetic? #\Q))
(check-equal #t (char-alphabetic? #\x1A59))
(check-equal #f (char-alphabetic? #\x1A60))
(check-equal #f (char-alphabetic? #\x2000))

(check-error (assertion-violation char-alphabetic?) (char-alphabetic?))
(check-error (assertion-violation char-alphabetic?) (char-alphabetic? 10))
(check-error (assertion-violation char-alphabetic?) (char-alphabetic? #\a #\a))

(check-equal #t (char-numeric? #\3))
(check-equal #t (char-numeric? #\x0664))
(check-equal #t (char-numeric? #\x0AE6))
(check-equal #f (char-numeric? #\x0EA6))

(check-error (assertion-violation char-numeric?) (char-numeric?))
(check-error (assertion-violation char-numeric?) (char-numeric? 10))
(check-error (assertion-violation char-numeric?) (char-numeric? #\a #\a))

(check-equal #t (char-whitespace? #\ ))
(check-equal #t (char-whitespace? #\x2001))
(check-equal #t (char-whitespace? #\x00A0))
(check-equal #f (char-whitespace? #\a))
(check-equal #f (char-whitespace? #\x0EA6))

(check-error (assertion-violation char-whitespace?) (char-whitespace?))
(check-error (assertion-violation char-whitespace?) (char-whitespace? 10))
(check-error (assertion-violation char-whitespace?) (char-whitespace? #\a #\a))

(check-equal #t (char-upper-case? #\R))
(check-equal #f (char-upper-case? #\r))
(check-equal #t (char-upper-case? #\x1E20))
(check-equal #f (char-upper-case? #\x9999))

(check-error (assertion-violation char-upper-case?) (char-upper-case?))
(check-error (assertion-violation char-upper-case?) (char-upper-case? 10))
(check-error (assertion-violation char-upper-case?) (char-upper-case? #\a #\a))

(check-equal #t (char-lower-case? #\s))
(check-equal #f (char-lower-case? #\S))
(check-equal #t (char-lower-case? #\xA687))
(check-equal #f (char-lower-case? #\x2CED))

(check-error (assertion-violation char-lower-case?) (char-lower-case?))
(check-error (assertion-violation char-lower-case?) (char-lower-case? 10))
(check-error (assertion-violation char-lower-case?) (char-lower-case? #\a #\a))

(check-equal 3 (digit-value #\3))
(check-equal 4 (digit-value #\x0664))
(check-equal 0 (digit-value #\x0AE6))
(check-equal #f (digit-value #\x0EA6))

(check-error (assertion-violation digit-value) (digit-value))
(check-error (assertion-violation digit-value) (digit-value 10))
(check-error (assertion-violation digit-value) (digit-value #\a #\a))

(check-equal #x03BB (char->integer #\x03BB))
(check-equal #xD (char->integer #\return))

(check-error (assertion-violation char->integer) (char->integer))
(check-error (assertion-violation char->integer) (char->integer #\a #\a))
(check-error (assertion-violation char->integer) (char->integer 10))

(check-equal #\newline (integer->char #x0A))

(check-error (assertion-violation integer->char) (integer->char))
(check-error (assertion-violation integer->char) (integer->char 10 10))
(check-error (assertion-violation integer->char) (integer->char #\a))

(check-equal #\A (char-upcase #\a))
(check-equal #\A (char-upcase #\A))
(check-equal #\9 (char-upcase #\9))

(check-error (assertion-violation char-upcase) (char-upcase))
(check-error (assertion-violation char-upcase) (char-upcase 10))
(check-error (assertion-violation char-upcase) (char-upcase #\x10 #\x10))

(check-equal #\a (char-downcase #\a))
(check-equal #\a (char-downcase #\A))
(check-equal #\9 (char-downcase #\9))

(check-error (assertion-violation char-downcase) (char-downcase))
(check-error (assertion-violation char-downcase) (char-downcase 10))
(check-error (assertion-violation char-downcase) (char-downcase #\x10 #\x10))

(check-equal #\x0064 (char-foldcase #\x0044))
(check-equal #\x00FE (char-foldcase #\x00DE))
(check-equal #\x016D (char-foldcase #\x016C))
(check-equal #\x0201 (char-foldcase #\x0200))
(check-equal #\x043B (char-foldcase #\x041B))
(check-equal #\x0511 (char-foldcase #\x0510))
(check-equal #\x1E1F (char-foldcase #\x1E1E))
(check-equal #\x1EF7 (char-foldcase #\x1EF6))
(check-equal #\x2C52 (char-foldcase #\x2C22))
(check-equal #\xA743 (char-foldcase #\xA742))
(check-equal #\xFF4D (char-foldcase #\xFF2D))
(check-equal #\x1043D (char-foldcase #\x10415))

(check-equal #\x0020 (char-foldcase #\x0020))
(check-equal #\x0091 (char-foldcase #\x0091))
(check-equal #\x0765 (char-foldcase #\x0765))
(check-equal #\x6123 (char-foldcase #\x6123))

(check-error (assertion-violation char-foldcase) (char-foldcase))
(check-error (assertion-violation char-foldcase) (char-foldcase 10))
(check-error (assertion-violation char-foldcase) (char-foldcase #\x10 #\x10))

;;
;; ---- strings ----
;;

(check-equal #t (string? ""))
(check-equal #t (string? "123"))
(check-equal #f (string? #\a))
(check-equal #f (string? 'abc))

(check-error (assertion-violation string?) (string?))
(check-error (assertion-violation string?) (string? #\a 10))

(check-equal "" (make-string 0))
(check-equal 10 (string-length (make-string 10)))
(check-equal "aaaaaaaaaaaaaaaa" (make-string 16 #\a))

(check-error (assertion-violation make-string) (make-string))
(check-error (assertion-violation make-string) (make-string #\a))
(check-error (assertion-violation make-string) (make-string 10 10))
(check-error (assertion-violation make-string) (make-string 10 #\a 10))

(check-equal "" (string))
(check-equal "1234" (string #\1 #\2 #\3 #\4))

(check-error (assertion-violation string) (string 1))
(check-error (assertion-violation string) (string #\1 1))

(check-equal 0 (string-length ""))
(check-equal 4 (string-length "1234"))

(check-error (assertion-violation string-length) (string-length))
(check-error (assertion-violation string-length) (string-length #\a))
(check-error (assertion-violation string-length) (string-length "" #\a))

(check-equal #\3 (string-ref "123456" 2))

(check-error (assertion-violation string-ref) (string-ref ""))
(check-error (assertion-violation string-ref) (string-ref "" 2 2))
(check-error (assertion-violation string-ref) (string-ref "123" 4))
(check-error (assertion-violation string-ref) (string-ref "123" -1))
(check-error (assertion-violation string-ref) (string-ref #(1 2 3) 1))

(check-equal "*?*"
    (let ((s (make-string 3 #\*)))
        (string-set! s 1 #\?)
        s))

(define s (string #\1 #\2 #\3 #\4))
(check-error (assertion-violation string-set!) (string-set! s 10))
(check-error (assertion-violation string-set!) (string-set! s 10 #\a 10))
(check-error (assertion-violation string-set!) (string-set! #\a 10 #\a))
(check-error (assertion-violation string-set!) (string-set! s #t #\a))
(check-error (assertion-violation string-set!) (string-set! s 10 'a))

(check-equal #t (string=? "aaaa" (make-string 4 #\a)))
(check-equal #t (string=? "\t\"\\" "\x9;\x22;\x5C;"))
(check-equal #t (string=? "aaa" "aaa" "aaa"))
(check-equal #f (string=? "aaa" "aaa" "bbb"))

(check-error (assertion-violation string=?) (string=? "a"))
(check-error (assertion-violation string=?) (string=? "a" "a" 10))

(check-equal #t (string<? "aaaa" (make-string 4 #\b)))
(check-equal #t (string<? "\t\"\\" "\x9;\x22;\x5C;c"))
(check-equal #t (string<? "aaa" "aab" "caa"))
(check-equal #f (string<? "aaa" "bbb" "bbb"))

(check-error (assertion-violation string<?) (string<? "a"))
(check-error (assertion-violation string<?) (string<? "a" "b" 10))

(check-equal #t (string>? "cccc" (make-string 4 #\b)))
(check-equal #t (string>? "\t\"\\c" "\x9;\x22;\x5C;"))
(check-equal #t (string>? "aac" "aab" "aaa"))
(check-equal #f (string>? "ccc" "bbb" "bbb"))

(check-error (assertion-violation string>?) (string>? "a"))
(check-error (assertion-violation string>?) (string>? "c" "b" 10))

(check-equal #t (string<=? "aaaa" (make-string 4 #\b) "bbbb"))
(check-equal #t (string<=? "\t\"\\" "\x9;\x22;\x5C;"))
(check-equal #t (string<=? "aaa" "aaa" "aab" "caa"))
(check-equal #f (string<=? "aaa" "bbb" "bbb" "a"))

(check-error (assertion-violation string<=?) (string<=? "a"))
(check-error (assertion-violation string<=?) (string<=? "a" "b" 10))

(check-equal #t (string>=? "cccc" (make-string 4 #\b) "bbbb"))
(check-equal #t (string>=? "\t\"\\c" "\x9;\x22;\x5C;"))
(check-equal #t (string>=? "aac" "aab" "aaa"))
(check-equal #f (string>=? "ccc" "bbb" "bbb" "ddd"))

(check-error (assertion-violation string>=?) (string>=? "a"))
(check-error (assertion-violation string>=?) (string>=? "c" "b" 10))

(check-equal #t (string-ci=? "aaaa" (make-string 4 #\A)))
(check-equal #t (string-ci=? "\t\"\\" "\x9;\x22;\x5C;"))
(check-equal #t (string-ci=? "aaA" "aAa" "Aaa"))
(check-equal #f (string-ci=? "aaa" "aaA" "Bbb"))

(check-error (assertion-violation string-ci=?) (string-ci=? "a"))
(check-error (assertion-violation string-ci=?) (string-ci=? "a" "a" 10))

(check-equal #t (string-ci<? "aAAa" (make-string 4 #\b)))
(check-equal #t (string-ci<? "\t\"\\" "\x9;\x22;\x5C;c"))
(check-equal #t (string-ci<? "aAa" "aaB" "cAa"))
(check-equal #f (string-ci<? "aaa" "bbb" "bBb"))

(check-error (assertion-violation string-ci<?) (string-ci<? "a"))
(check-error (assertion-violation string-ci<?) (string-ci<? "a" "b" 10))

(check-equal #t (string-ci>? "cccc" (make-string 4 #\B)))
(check-equal #t (string-ci>? "\t\"\\c" "\x9;\x22;\x5C;"))
(check-equal #t (string-ci>? "Aac" "aAb" "aaA"))
(check-equal #f (string-ci>? "ccC" "Bbb" "bbB"))

(check-error (assertion-violation string-ci>?) (string-ci>? "a"))
(check-error (assertion-violation string-ci>?) (string-ci>? "c" "b" 10))

(check-equal #t (string-ci<=? "aaAa" (make-string 4 #\b) "bBBb"))
(check-equal #t (string-ci<=? "\t\"\\" "\x9;\x22;\x5C;"))
(check-equal #t (string-ci<=? "aaA" "Aaa" "aAb" "caa"))
(check-equal #f (string-ci<=? "aaa" "bbb" "BBB" "a"))

(check-error (assertion-violation string-ci<=?) (string-ci<=? "a"))
(check-error (assertion-violation string-ci<=?) (string-ci<=? "a" "b" 10))

(check-equal #t (string-ci>=? "cccc" (make-string 4 #\B) "bbbb"))
(check-equal #t (string-ci>=? "\t\"\\c" "\x9;\x22;\x5C;"))
(check-equal #t (string-ci>=? "aac" "AAB" "aaa"))
(check-equal #f (string-ci>=? "ccc" "BBB" "bbb" "ddd"))

(check-error (assertion-violation string-ci>=?) (string-ci>=? "a"))
(check-error (assertion-violation string-ci>=?) (string-ci>=? "c" "b" 10))

(check-equal "AAA" (string-upcase "aaa"))
(check-equal "AAA" (string-upcase "aAa"))
(check-equal "\x0399;\x0308;\x0301;\x03A5;\x0308;\x0301;\x1FBA;\x0399;"
        (string-upcase "\x0390;\x03B0;\x1FB2;"))

(check-error (assertion-violation string-upcase) (string-upcase))
(check-error (assertion-violation string-upcase) (string-upcase #\a))
(check-error (assertion-violation string-upcase) (string-upcase "a" "a"))

(check-equal "aaa" (string-downcase "AAA"))
(check-equal "aaa" (string-downcase "aAa"))
(check-equal "a\x0069;\x0307;a" (string-downcase "A\x0130;a"))

(check-error (assertion-violation string-downcase) (string-downcase))
(check-error (assertion-violation string-downcase) (string-downcase #\a))
(check-error (assertion-violation string-downcase) (string-downcase "a" "a"))

(check-equal #t (string=? (string-foldcase "AAA") (string-foldcase "aaa")))
(check-equal #t (string=? (string-foldcase "AAA") (string-foldcase "aAa")))
(check-equal #t (string=? (string-foldcase "\x1E9A;") "\x0061;\x02BE;"))
(check-equal #t (string=? (string-foldcase "\x1F52;\x1F54;\x1F56;")
        "\x03C5;\x0313;\x0300;\x03C5;\x0313;\x0301;\x03C5;\x0313;\x0342;"))

(check-error (assertion-violation string-foldcase) (string-foldcase))
(check-error (assertion-violation string-foldcase) (string-foldcase #\a))
(check-error (assertion-violation string-foldcase) (string-foldcase "a" "a"))

(check-equal "123abcdEFGHI" (string-append "123" "abcd" "EFGHI"))

(check-error (assertion-violation string-append) (string-append #\a))
(check-error (assertion-violation string-append) (string-append "a" #\a))

(check-equal (#\1 #\2 #\3 #\4) (string->list "1234"))
(check-equal (#\b #\c #\d) (string->list "abcdefg" 1 4))
(check-equal (#\w #\x #\y #\z) (string->list "qrstuvwxyz" 6))

(check-error (assertion-violation string->list) (string->list))
(check-error (assertion-violation string->list) (string->list #\a))
(check-error (assertion-violation string->list) (string->list "123" -1))
(check-error (assertion-violation string->list) (string->list "123" #\a))
(check-error (assertion-violation string->list) (string->list "123" 1 0))
(check-error (assertion-violation string->list) (string->list "123" 1 4))
(check-error (assertion-violation string->list) (string->list "123" 1 #t))
(check-error (assertion-violation string->list) (string->list "123" 1 3 3))

(check-equal "abc" (list->string '(#\a #\b #\c)))
(check-equal "" (list->string '()))

(check-error (assertion-violation list->string) (list->string))
(check-error (assertion-violation) (list->string "abc"))

(check-equal "234" (substring "12345" 1 4))

(check-equal "12345" (string-copy "12345"))
(check-equal "2345" (string-copy "12345" 1))
(check-equal "23" (string-copy "12345" 1 3))

(check-error (assertion-violation string-copy) (string-copy))
(check-error (assertion-violation string-copy) (string-copy #\a))
(check-error (assertion-violation string-copy) (string-copy "abc" -1))
(check-error (assertion-violation string-copy) (string-copy "abc" #t))
(check-error (assertion-violation string-copy) (string-copy "abc" 4))
(check-error (assertion-violation string-copy) (string-copy "abc" 1 0))
(check-error (assertion-violation string-copy) (string-copy "abc" 1 4))
(check-error (assertion-violation string-copy) (string-copy "abc" 1 2 3))

(define a "12345")
(define b (string-copy "abcde"))
(string-copy! b 1 a 0 2)
(check-equal "a12de" b)
(check-equal "abcde" (let ((s (make-string 5))) (string-copy! s 0 "abcde") s))
(check-equal "0abc0" (let ((s (make-string 5 #\0))) (string-copy! s 1 "abc") s))

(check-error (assertion-violation string-copy!) (string-copy! (make-string 5) 0))
(check-error (assertion-violation string-copy!) (string-copy! #\a 0 "abcde"))

;;
;; ---- vectors ----
;;

(check-equal #t (vector? #()))
(check-equal #t (vector? #(a b c)))
(check-equal #f (vector? #u8()))
(check-equal #f (vector? 12))
(check-error (assertion-violation vector?) (vector?))
(check-error (assertion-violation vector?) (vector? #() #()))

(check-equal #() (make-vector 0))
(check-equal #(a a a) (make-vector 3 'a))
(check-error (assertion-violation make-vector) (make-vector))
(check-error (assertion-violation make-vector) (make-vector -1))
(check-error (assertion-violation make-vector) (make-vector 1 1 1))

(check-equal #(a b c) (vector 'a 'b 'c))
(check-equal #() (vector))

(check-equal 0 (vector-length #()))
(check-equal 3 (vector-length #(a b c)))
(check-error (assertion-violation vector-length) (vector-length))
(check-error (assertion-violation vector-length) (vector-length #u8()))
(check-error (assertion-violation vector-length) (vector-length #() #()))

(check-equal 8 (vector-ref #(1 1 2 3 5 8 13 21) 5))
(check-error (assertion-violation vector-ref) (vector-ref))
(check-error (assertion-violation vector-ref) (vector-ref #(1 2 3)))
(check-error (assertion-violation vector-ref) (vector-ref #(1 2 3) -1))
(check-error (assertion-violation vector-ref) (vector-ref #(1 2 3) 4))
(check-error (assertion-violation vector-ref) (vector-ref #(1 2 3) 1 1))
(check-error (assertion-violation vector-ref) (vector-ref 1 1))

(check-equal #(0 ("Sue" "Sue") "Anna")
    (let ((vec (vector 0 '(2 2 2 2) "Anna")))
        (vector-set! vec 1 '("Sue" "Sue"))
        vec))

(define v (vector 1 2 3))
(check-error (assertion-violation vector-set!) (vector-set!))
(check-error (assertion-violation vector-set!) (vector-set! v))
(check-error (assertion-violation vector-set!) (vector-set! v 1))
(check-error (assertion-violation vector-set!) (vector-set! v 1 1 1))
(check-error (assertion-violation vector-set!) (vector-set! 1 1 1))
(check-error (assertion-violation vector-set!) (vector-set! v -1 1 1))
(check-error (assertion-violation vector-set!) (vector-set! v 3 1 1))

(check-equal (dah dah didah) (vector->list '#(dah dah didah)))
(check-equal (dah) (vector->list '#(dah dah didah) 1 2))

(check-error (assertion-violation vector->list) (vector->list))
(check-error (assertion-violation vector->list) (vector->list #u8()))
(check-error (assertion-violation vector->list) (vector->list '()))
(check-error (assertion-violation vector->list) (vector->list #(1 2 3 4) #f))
(check-error (assertion-violation vector->list) (vector->list #(1 2 3 4) -1))
(check-error (assertion-violation vector->list) (vector->list #(1 2 3 4) 5))
(check-error (assertion-violation vector->list) (vector->list #(1 2 3 4) 1 0))
(check-error (assertion-violation vector->list) (vector->list #(1 2 3 4) 1 5))
(check-error (assertion-violation vector->list) (vector->list #(1 2 3 4) 1 2 3))

(check-equal #(dididit dah) (list->vector '(dididit dah)))

(check-equal "123" (vector->string #(#\1 #\2 #\3)))
(check-equal "def" (vector->string #(#\a #\b #\c #\d #\e #\f #\g #\h) 3 6))
(check-equal "gh" (vector->string #(#\a #\b #\c #\d #\e #\f #\g #\h) 6 8))

(check-error (assertion-violation vector->string) (vector->string))
(check-error (assertion-violation vector->string) (vector->string '()))
(check-error (assertion-violation vector->string) (vector->string #(#\a #\b #\c) #f))
(check-error (assertion-violation vector->string) (vector->string #(#\a #\b #\c) -1))
(check-error (assertion-violation vector->string) (vector->string #(#\a #\b #\c) 4))
(check-error (assertion-violation vector->string) (vector->string #(#\a #\b #\c) 0 #f))
(check-error (assertion-violation vector->string) (vector->string #(#\a #\b #\c) 0 4))
(check-error (assertion-violation vector->string) (vector->string #(#\a #\b #\c) 1 2 3))

(check-equal #(#\A #\B #\C) (string->vector "ABC"))

(check-error (assertion-violation string->vector) (string->vector))
(check-error (assertion-violation string->vector) (string->vector '()))
(check-error (assertion-violation string->vector) (string->vector "abc" #f))
(check-error (assertion-violation string->vector) (string->vector "abc" -1))
(check-error (assertion-violation string->vector) (string->vector "abc" 4))
(check-error (assertion-violation string->vector) (string->vector "abc" 0 #f))
(check-error (assertion-violation string->vector) (string->vector "abc" 0 4))
(check-error (assertion-violation string->vector) (string->vector "abc" 1 2 3))

(define a #(1 8 2 8))
(define b (vector-copy a))
(check-equal #(1 8 2 8) b)
(vector-set! b 0 3)
(check-equal #(3 8 2 8) b)
(define c (vector-copy b 1 3))
(check-equal #(8 2) c)

(define v (vector 1 2 3 4))
(check-error (assertion-violation vector-copy) (vector-copy))
(check-error (assertion-violation vector-copy) (vector-copy 1))
(check-error (assertion-violation vector-copy) (vector-copy v 1 2 1))
(check-error (assertion-violation vector-copy) (vector-copy v -1 2))
(check-error (assertion-violation vector-copy) (vector-copy v 3 2))
(check-error (assertion-violation vector-copy) (vector-copy v 1 5))

(define a (vector 1 2 3 4 5))
(define b (vector 10 20 30 40 50))
(vector-copy! b 1 a 0 2)
(check-equal #(10 1 2 40 50) b)

(define x (vector 'a 'b 'c 'd 'e 'f 'g))
(vector-copy! x 1 x 0 3)
(check-equal #(a a b c e f g) x)

(define x (vector 'a 'b 'c 'd 'e 'f 'g))
(vector-copy! x 1 x 3 6)
(check-equal #(a d e f e f g) x)

(check-error (assertion-violation vector-copy!) (vector-copy! a 0))
(check-error (assertion-violation vector-copy!) (vector-copy! a 0 b 1 1 1))
(check-error (assertion-violation vector-copy!) (vector-copy! 1 0 b))
(check-error (assertion-violation vector-copy!) (vector-copy! a 0 1))
(check-error (assertion-violation vector-copy!) (vector-copy! a -1 b))
(check-error (assertion-violation vector-copy!) (vector-copy! a 3 b))
(check-error (assertion-violation vector-copy!) (vector-copy! a 0 b -1))
(check-error (assertion-violation vector-copy!) (vector-copy! a 0 b 1 0))
(check-error (assertion-violation vector-copy!) (vector-copy! a 0 b 1 6))

(check-equal #(a b c d e f) (vector-append #(a b c) #(d e f)))
(check-error (assertion-violation vector-append) (vector-append 1))
(check-error (assertion-violation vector-append) (vector-append #(1 2) 1))

(define a (vector 1 2 3 4 5))
(vector-fill! a 'smash 2 4)
(check-equal #(1 2 smash smash 5) a)

(define v (vector 1 2 3 4))
(check-error (assertion-violation vector-fill!) (vector-fill! 1))
(check-error (assertion-violation vector-fill!) (vector-fill! 1 #f))
(check-error (assertion-violation vector-fill!) (vector-fill! v #f 1 2 1))
(check-error (assertion-violation vector-fill!) (vector-fill! v #f -1 2))
(check-error (assertion-violation vector-fill!) (vector-fill! v #f 3 2))
(check-error (assertion-violation vector-fill!) (vector-fill! v #f 1 5))

;;
;; ---- bytevectors ----

(check-equal #t (bytevector? #u8()))
(check-equal #t (bytevector? #u8(1 2)))
(check-equal #f (bytevector? #(1 2)))
(check-equal #f (bytevector? 12))
(check-error (assertion-violation bytevector?) (bytevector?))
(check-error (assertion-violation bytevector?) (bytevector? #u8() #u8()))

(check-equal #u8(12 12) (make-bytevector 2 12))
(check-equal #u8() (make-bytevector 0))
(check-equal 10 (bytevector-length (make-bytevector 10)))
(check-error (assertion-violation make-bytevector) (make-bytevector))
(check-error (assertion-violation make-bytevector) (make-bytevector -1))
(check-error (assertion-violation make-bytevector) (make-bytevector 1 -1))
(check-error (assertion-violation make-bytevector) (make-bytevector 1 256))
(check-error (assertion-violation make-bytevector) (make-bytevector 1 1 1))

(check-equal #u8(1 3 5 1 3 5) (bytevector 1 3 5 1 3 5))
(check-equal #u8() (bytevector))
(check-error (assertion-violation bytevector) (bytevector -1))
(check-error (assertion-violation bytevector) (bytevector 256))
(check-error (assertion-violation bytevector) (bytevector 10 20 -1))
(check-error (assertion-violation bytevector) (bytevector 10 20 256 30))

(check-equal 0 (bytevector-length #u8()))
(check-equal 4 (bytevector-length #u8(1 2 3 4)))
(check-error (assertion-violation bytevector-length) (bytevector-length))
(check-error (assertion-violation bytevector-length) (bytevector-length 10))
(check-error (assertion-violation bytevector-length) (bytevector-length #() #()))
(check-error (assertion-violation bytevector-length) (bytevector-length #() 10))

(check-equal 8 (bytevector-u8-ref #u8(1 1 2 3 5 8 13 21) 5))
(check-error (assertion-violation bytevector-u8-ref) (bytevector-u8-ref 1 1))
(check-error (assertion-violation bytevector-u8-ref) (bytevector-u8-ref #(1 2 3)))
(check-error (assertion-violation bytevector-u8-ref) (bytevector-u8-ref #(1 2 3) 1 1))
(check-error (assertion-violation bytevector-u8-ref) (bytevector-u8-ref #(1 2 3) -1))
(check-error (assertion-violation bytevector-u8-ref) (bytevector-u8-ref #(1 2 3) 3))

(check-equal #u8(1 3 3 4)
    (let ((bv (bytevector 1 2 3 4)))
        (bytevector-u8-set! bv 1 3)
        bv))

(define bv (bytevector 1 2 3 4))
(check-error (assertion-violation bytevector-u8-set!) (bytevector-u8-set! 1 1 1))
(check-error (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv 1))
(check-error (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv 1 1 1))
(check-error (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv -1 1))
(check-error (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv 5 1))
(check-error (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv 1 -1))
(check-error (assertion-violation bytevector-u8-set!) (bytevector-u8-set! bv 1 256))

(define a #u8(1 2 3 4 5))
(check-equal #u8(3 4) (bytevector-copy a 2 4))

(define bv (bytevector 1 2 3 4))
(check-error (assertion-violation bytevector-copy) (bytevector-copy))
(check-error (assertion-violation bytevector-copy) (bytevector-copy 1))
(check-error (assertion-violation bytevector-copy) (bytevector-copy bv 1 2 1))
(check-error (assertion-violation bytevector-copy) (bytevector-copy bv -1 2))
(check-error (assertion-violation bytevector-copy) (bytevector-copy bv 3 2))
(check-error (assertion-violation bytevector-copy) (bytevector-copy bv 1 5))

(define a (bytevector 1 2 3 4 5))
(define b (bytevector 10 20 30 40 50))
(bytevector-copy! b 1 a 0 2)
(check-equal #u8(10 1 2 40 50) b)

(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a 0))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a 0 b 1 1 1))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! 1 0 b))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a 0 1))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a -1 b))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a 3 b))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a 0 b -1))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a 0 b 1 0))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a 0 b 1 6))

(check-equal #u8(0 1 2 3 4 5) (bytevector-append #u8(0 1 2) #u8(3 4 5)))
(check-error (assertion-violation bytevector-append) (bytevector-append 1))
(check-error (assertion-violation bytevector-append) (bytevector-append #u8(1 2) 1))

(check-equal "ABCDE" (utf8->string #u8(65 66 67 68 69)))
(check-equal "CDE" (utf8->string #u8(65 66 67 68 69) 2))
(check-equal "BCD" (utf8->string #u8(65 66 67 68 69) 1 4))
(check-error (assertion-violation utf8->string) (utf8->string))
(check-error (assertion-violation utf8->string) (utf8->string 1))
(check-error (assertion-violation utf8->string) (utf8->string #u8(65 66 67) 1 2 2))
(check-error (assertion-violation utf8->string) (utf8->string #u8(65 66 67) -1))
(check-error (assertion-violation utf8->string) (utf8->string #u8(65 66 67) 4))
(check-error (assertion-violation utf8->string) (utf8->string #u8(65 66 67) 2 1))
(check-error (assertion-violation utf8->string) (utf8->string #u8(65 66 67) 0 4))

(check-equal #u8(207 187) (string->utf8 (utf8->string #u8(207 187))))
(check-error (assertion-violation string->utf8) (string->utf8))
(check-error (assertion-violation string->utf8) (string->utf8 1))
(check-error (assertion-violation string->utf8) (string->utf8 "ABC" 1 2 2))
(check-error (assertion-violation string->utf8) (string->utf8 "ABC" -1))
(check-error (assertion-violation string->utf8) (string->utf8 "ABC" 4))
(check-error (assertion-violation string->utf8) (string->utf8 "ABC" 2 1))
(check-error (assertion-violation string->utf8) (string->utf8 "ABC" 0 4))

;;
;; ---- control features ----
;;

;; procedure?

(check-equal #t (procedure? car))
(check-equal #f (procedure? 'car))
(check-equal #t (procedure? (lambda (x) (* x x))))
(check-equal #f (procedure? '(lambda (x) (* x x))))
(check-equal #t (call-with-current-continuation procedure?))

(check-error (assertion-violation procedure?) (procedure?))
(check-error (assertion-violation procedure?) (procedure? 1 2))

;; apply

(check-equal 7 (apply + (list 3 4)))

(define compose
    (lambda (f g)
        (lambda args
            (f (apply g args)))))
(check-equal 30 ((compose sqrt *) 12 75))

(check-equal 15 (apply + '(1 2 3 4 5)))
(check-equal 15 (apply + 1 '(2 3 4 5)))
(check-equal 15 (apply + 1 2 '(3 4 5)))
(check-equal 15 (apply + 1 2 3 4 5 '()))

(check-error (assertion-violation apply) (apply +))
(check-error (assertion-violation apply) (apply + '(1 2 . 3)))
(check-error (assertion-violation apply) (apply + 1))

;; map

(check-equal (b e h) (map cadr '((a b) (d e) (g h))))
(check-equal (1 4 27 256 3125) (map (lambda (n) (expt n n)) '(1 2 3 4 5)))
(check-equal (5 7 9) (map + '(1 2 3) '(4 5 6 7)))

(check-error (assertion-violation map) (map car))
(check-error (assertion-violation apply) (map 12 '(1 2 3 4)))
(check-error (assertion-violation car) (map car '(1 2 3 4) '(a b c d)))

;; string-map

(check-equal "abdegh" (string-map char-foldcase "AbdEgH"))

(check-equal "IBM" (string-map (lambda (c) (integer->char (+ 1 (char->integer c)))) "HAL"))
(check-equal "StUdLyCaPs" (string-map (lambda (c k) ((if (eqv? k #\u) char-upcase char-downcase) c))
    "studlycaps xxx" "ululululul"))

(check-error (assertion-violation string-map) (string-map char-foldcase))
(check-error (assertion-violation string-map) (string-map char-foldcase '(#\a #\b #\c)))
(check-error (assertion-violation apply) (string-map 12 "1234"))
(check-error (assertion-violation char-foldcase) (string-map char-foldcase "1234" "abcd"))

;; vector-map

(check-equal #(b e h) (vector-map cadr '#((a b) (d e) (g h))))
(check-equal #(1 4 27 256 3125) (vector-map (lambda (n) (expt n n)) '#(1 2 3 4 5)))
(check-equal #(5 7 9) (vector-map + '#(1 2 3) '#(4 5 6 7)))

(check-error (assertion-violation vector-map) (vector-map +))
(check-error (assertion-violation apply) (vector-map 12 #(1 2 3 4)))
(check-error (assertion-violation not) (vector-map not #(#t #f #t) #(#f #t #f)))

;; for-each

(check-equal #(0 1 4 9 16)
    (let ((v (make-vector 5)))
        (for-each (lambda (i) (vector-set! v i (* i i))) '(0 1 2 3 4))
        v))

(check-error (assertion-violation for-each) (for-each +))
(check-error (assertion-violation apply) (for-each 12 '(1 2 3 4)))
(check-error (assertion-violation not) (for-each not '(#t #t #t) '(#f #f #f)))

;; string-for-each

(check-equal (101 100 99 98 97)
    (let ((v '()))
        (string-for-each (lambda (c) (set! v (cons (char->integer c) v))) "abcde")
        v))

(check-error (assertion-violation string-for-each) (string-for-each char-foldcase))
(check-error (assertion-violation apply) (string-for-each 12 "1234"))
(check-error (assertion-violation char-foldcase) (string-for-each char-foldcase "abc" "123"))

;; vector-for-each

(check-equal (0 1 4 9 16)
    (let ((v (make-list 5)))
        (vector-for-each (lambda (i) (list-set! v i (* i i))) '#(0 1 2 3 4))
        v))

(check-error (assertion-violation vector-for-each) (vector-for-each +))
(check-error (assertion-violation apply) (vector-for-each 12 #(1 2 3 4)))
(check-error (assertion-violation not) (vector-for-each not #(#t #t #t) #(#f #f #f)))

;; call-with-current-continuation
;; call/cc

(check-equal -3 (call-with-current-continuation
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

(check-error (assertion-violation call-with-current-continuation) (call/cc))
(check-error (assertion-violation) (call/cc #t))
(check-error (assertion-violation call-with-current-continuation) (call/cc #t #t))
(check-error (assertion-violation call-with-current-continuation) (call/cc (lambda (e) e) #t))

(check-equal 4 (list-length '(1 2 3 4)))
(check-equal #f (list-length '(a b . c)))

(define (cc-values . things)
    (call/cc (lambda (cont) (apply cont things))))

(check-equal (1 2 3 4) (let-values (((a b) (cc-values 1 2))
                                    ((c d) (cc-values 3 4)))
                                (list a b c d)))
(define (v)
    (define (v1) (v3) (v2) (v2))
    (define (v2) (v3) (v3))
    (define (v3) (cc-values 1 2 3 4))
    (v1))
(check-equal (4 3 2 1) (let-values (((w x y z) (v))) (list z y x w)))

(check-equal 5 (call-with-values (lambda () (cc-values 4 5)) (lambda (a b) b)))

;; values

(define (v0) (values))
(define (v1) (values 1))
(define (v2) (values 1 2))

(check-equal 1 (+ (v1) 0))
(check-error (assertion-violation values) (+ (v0) 0))
(check-error (assertion-violation values) (+ (v2) 0))
(check-equal 1 (begin (v0) 1))
(check-equal 1 (begin (v1) 1))
(check-equal 1 (begin (v2) 1))

;; call-with-values

(check-equal 5 (call-with-values (lambda () (values 4 5)) (lambda (a b) b)))
(check-equal 4 (call-with-values (lambda () (values 4 5)) (lambda (a b) a)))
(check-equal -1 (call-with-values * -))

(check-error (assertion-violation call-with-values) (call-with-values *))
(check-error (assertion-violation call-with-values) (call-with-values * - +))
(check-error (assertion-violation apply) (call-with-values * 10))
(check-error (assertion-violation call-with-values) (call-with-values 10 -))

;; dynamic-wind

(check-equal (connect talk1 disconnect connect talk2 disconnect)
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

(check-error (assertion-violation dynamic-wind) (dynamic-wind (lambda () 'one) (lambda () 'two)))
(check-error (assertion-violation dynamic-wind) (dynamic-wind (lambda () 'one) (lambda () 'two)
        (lambda () 'three) (lambda () 'four)))
(check-error (assertion-violation dynamic-wind) (dynamic-wind 10 (lambda () 'thunk)
        (lambda () 'after)))
(check-error (assertion-violation) (dynamic-wind (lambda () 'before) 10
        (lambda () 'after)))
(check-error (assertion-violation dynamic-wind) (dynamic-wind (lambda () 'before)
        (lambda () 'thunk) 10))
(check-error (assertion-violation) (dynamic-wind (lambda (n) 'before)
        (lambda () 'thunk) (lambda () 'after)))
(check-error (assertion-violation) (dynamic-wind (lambda () 'before)
        (lambda (n) 'thunk) (lambda () 'after)))
(check-error (assertion-violation) (dynamic-wind (lambda () 'before)
        (lambda () 'thunk) (lambda (n) 'after)))

;;
;; ---- exceptions ----
;;

;; with-exception-handler

(define e #f)
(check-equal exception (call-with-current-continuation
    (lambda (k)
        (with-exception-handler
            (lambda (x) (set! e x) (k 'exception))
            (lambda () (+ 1 (raise 'an-error)))))))
(check-equal an-error e)

(check-equal (another-error)
    (guard (o ((eq? o 10) 10) (else (list o)))
        (with-exception-handler
            (lambda (x) (set! e x))
            (lambda ()
                (+ 1 (raise 'another-error))))))
(check-equal another-error e)

(check-equal 65
    (with-exception-handler
        (lambda (con)
            (cond
                ((string? con) (set! e con))
                (else (set! e "a warning has been issued")))
            42)
        (lambda ()
            (+ (raise-continuable "should be a number") 23))))
(check-equal "should be a number" e)

(check-error (assertion-violation with-exception-handler)
        (with-exception-handler (lambda (obj) 'handler)))
(check-error (assertion-violation with-exception-handler)
        (with-exception-handler (lambda (obj) 'handler) (lambda () 'thunk) (lambda () 'extra)))
(check-error (assertion-violation with-exception-handler)
        (with-exception-handler 10 (lambda () 'thunk)))
(check-error (assertion-violation)
        (with-exception-handler (lambda (obj) 'handler) 10))
(check-error (assertion-violation)
        (with-exception-handler (lambda (obj) 'handler) (lambda (oops) 'thunk)))

(check-error (assertion-violation raise) (raise))
(check-error (assertion-violation raise) (raise 1 2))

(check-error (assertion-violation raise-continuable) (raise-continuable))
(check-error (assertion-violation raise-continuable) (raise-continuable 1 2))

(define (null-list? l)
    (cond ((pair? l) #f)
        ((null? l) #t)
        (else (error "null-list?: argument out of domain" l))))

(check-error (assertion-violation error) (null-list? #\a))
(check-error (assertion-violation error) (error))
(check-error (assertion-violation error) (error #\a))

(check-equal #t (error-object?
    (call/cc (lambda (cont)
        (with-exception-handler (lambda (obj) (cont obj)) (lambda () (error "tsting")))))))
(check-equal #f (error-object? 'error))

(check-error (assertion-violation error-object?) (error-object?))
(check-error (assertion-violation error-object?) (error-object? 1 2))

(check-equal "tsting" (error-object-message
    (call/cc (lambda (cont)
        (with-exception-handler (lambda (obj) (cont obj)) (lambda () (error "tsting")))))))

(check-error (assertion-violation error-object-message) (error-object-message))
(check-error (assertion-violation error-object-message) (error-object-message 1 2))

(check-equal (a b c) (error-object-irritants
    (call/cc (lambda (cont)
        (with-exception-handler
                (lambda (obj) (cont obj)) (lambda () (error "tsting" 'a 'b 'c)))))))

(check-error (assertion-violation error-object-irritants) (error-object-irritants))
(check-error (assertion-violation error-object-irritants) (error-object-irritants 1 2))

(check-equal #f (read-error? 'read))
(check-equal #f
    (guard (obj
        ((read-error? obj) #t)
        (else #f))
        (+ 'a 'b)))

(check-error (assertion-violation read-error?) (read-error?))
(check-error (assertion-violation read-error?) (read-error? 1 2))

(check-equal #f (file-error? 'file))
(check-equal #f
    (guard (obj
        ((file-error? obj) #t)
        (else #f))
        (+ 'a 'b)))

(check-error (assertion-violation file-error?) (file-error?))
(check-error (assertion-violation file-error?) (file-error? 1 2))

;;
;; ---- environments and evaluation ----
;;

(check-error (assertion-violation environment) (environment '|scheme base|))
(check-equal 10 (eval '(+ 1 2 3 4) (environment '(scheme base))))
(check-error (assertion-violation define) (eval '(define x 10) (environment '(scheme base))))

;;
;; ---- ports ----
;;

(check-equal #t
    (let ((p (open-input-file "r7rs.scm")))
        (call-with-port p (lambda (p) (input-port? p)))))
(check-equal #t
    (let ((p (open-input-file "r7rs.scm")))
        (call-with-port p (lambda (p) (input-port-open? p)))))
(check-equal #f
    (let ((p (open-input-file "r7rs.scm")))
        (call-with-port p (lambda (p) (read p)))
        (input-port-open? p)))
(check-equal (import (scheme base))
    (let ((p (open-input-file "r7rs.scm")))
        (call-with-port p (lambda (p) (read p)))))

(check-error (assertion-violation call-with-port) (call-with-port (open-input-file "r7rs.scm")))
(check-error (assertion-violation close-port) (call-with-port 'port (lambda (p) p)))

(check-equal #t (call-with-input-file "r7rs.scm" (lambda (p) (input-port? p))))
(check-equal #t (call-with-input-file "r7rs.scm" (lambda (p) (input-port-open? p))))
(check-equal (import (scheme base)) (call-with-input-file "r7rs.scm" (lambda (p) (read p))))

(check-error (assertion-violation call-with-input-file) (call-with-input-file "r7rs.scm"))
(check-error (assertion-violation open-binary-input-file)
        (call-with-input-file 'r7rs.scm (lambda (p) p)))

(check-equal #t (call-with-output-file "output.txt" (lambda (p) (output-port? p))))
(check-equal #t (call-with-output-file "output.txt" (lambda (p) (output-port-open? p))))
(check-equal (hello world)
    (begin
        (call-with-output-file "output.txt" (lambda (p) (write '(hello world) p)))
        (call-with-input-file "output.txt" (lambda (p) (read p)))))

(check-equal #f (input-port? "port"))
(check-equal #t (input-port? (current-input-port)))
(check-equal #f (input-port? (current-output-port)))

(check-error (assertion-violation input-port?) (input-port?))
(check-error (assertion-violation input-port?) (input-port? 'port 'port))

(check-equal #f (output-port? "port"))
(check-equal #f (output-port? (current-input-port)))
(check-equal #t (output-port? (current-output-port)))

(check-error (assertion-violation output-port?) (output-port?))
(check-error (assertion-violation output-port?) (output-port? 'port 'port))

(check-equal #f (textual-port? "port"))
(check-equal #t (textual-port? (current-input-port)))
(check-equal #t (textual-port? (current-output-port)))
(check-equal #f (call-with-port (open-binary-input-file "r7rs.scm") (lambda (p) (textual-port? p))))
(check-equal #f
    (call-with-port (open-binary-output-file "output.txt") (lambda (p) (textual-port? p))))

(check-error (assertion-violation textual-port?) (textual-port?))
(check-error (assertion-violation textual-port?) (textual-port? 'port 'port))

(check-equal #f (binary-port? "port"))
(check-equal #f (binary-port? (current-input-port)))
(check-equal #f (binary-port? (current-output-port)))
(check-equal #t (call-with-port (open-binary-input-file "r7rs.scm") (lambda (p) (binary-port? p))))
(check-equal #t
    (call-with-port (open-binary-output-file "output.txt") (lambda (p) (binary-port? p))))

(check-error (assertion-violation binary-port?) (binary-port?))
(check-error (assertion-violation binary-port?) (binary-port? 'port 'port))

(check-equal #f (port? "port"))
(check-equal #t (port? (current-input-port)))
(check-equal #t (port? (current-output-port)))
(check-equal #t (call-with-port (open-binary-input-file "r7rs.scm") (lambda (p) (port? p))))
(check-equal #t
    (call-with-port (open-binary-output-file "output.txt") (lambda (p) (port? p))))

(check-error (assertion-violation port?) (port?))
(check-error (assertion-violation port?) (port? 'port 'port))

(check-equal #t (input-port-open? (current-input-port)))
(check-equal #t
    (let ((p (open-input-file "r7rs.scm")))
        (call-with-port p (lambda (p) (input-port-open? p)))))
(check-equal #f
    (let ((p (open-output-file "output.txt")))
        (call-with-port p (lambda (p) (input-port-open? p)))))
(check-equal #f
    (let ((p (open-input-file "r7rs.scm")))
        (call-with-port p (lambda (p) p))
        (input-port-open? p)))

(check-error (assertion-violation input-port-open?) (input-port-open?))
(check-error (assertion-violation input-port-open?) (input-port-open? 'port))
(check-error (assertion-violation input-port-open?) (input-port-open? (current-input-port) 2))

(check-equal #t (output-port-open? (current-output-port)))
(check-equal #f
    (let ((p (open-input-file "r7rs.scm")))
        (call-with-port p (lambda (p) (output-port-open? p)))))
(check-equal #t
    (let ((p (open-output-file "output.txt")))
        (call-with-port p (lambda (p) (output-port-open? p)))))
(check-equal #f
    (let ((p (open-output-file "output.txt")))
        (call-with-port p (lambda (p) p))
        (output-port-open? p)))

(check-error (assertion-violation output-port-open?) (output-port-open?))
(check-error (assertion-violation output-port-open?) (output-port-open? 'port))
(check-error (assertion-violation output-port-open?) (output-port-open? (current-output-port) 2))

(check-equal #t (port? (current-input-port)))
(check-equal #t (input-port? (current-input-port)))
(check-equal #t (textual-port? (current-input-port)))
(check-equal #t
    (call-with-input-file "r7rs.scm"
        (lambda (p)
            (parameterize ((current-input-port p)) (eq? p (current-input-port))))))

(check-equal #t (port? (current-output-port)))
(check-equal #t (output-port? (current-output-port)))
(check-equal #t (textual-port? (current-output-port)))
(check-equal #t
    (call-with-output-file "output.txt"
        (lambda (p)
            (parameterize ((current-output-port p)) (eq? p (current-output-port))))))

(check-equal #t (port? (current-error-port)))
(check-equal #t (output-port? (current-error-port)))
(check-equal #t (textual-port? (current-error-port)))
(check-equal #t
    (call-with-output-file "output.txt"
        (lambda (p)
            (parameterize ((current-error-port p)) (eq? p (current-error-port))))))

(check-equal #t
    (let ((p (current-input-port)))
        (with-input-from-file "r7rs.scm" (lambda () (input-port-open? (current-input-port))))
        (eq? p (current-input-port))))
(check-equal #f
    (let ((p (current-input-port)))
        (with-input-from-file "r7rs.scm" (lambda () (eq? p (current-input-port))))))
(check-equal #f
    (let ((p #f))
        (with-input-from-file "r7rs.scm" (lambda () (set! p (current-input-port))))
        (input-port-open? p)))

(check-error (assertion-violation with-input-from-file) (with-input-from-file "r7rs.scm"))
(check-error (assertion-violation with-input-from-file)
    (with-input-from-file "r7rs.scm" (lambda () (current-input-port)) 3))

(check-equal #t
    (let ((p (current-output-port)))
        (with-output-to-file "output.txt" (lambda () (output-port-open? (current-output-port))))
        (eq? p (current-output-port))))
(check-equal #f
    (let ((p (current-output-port)))
        (with-output-to-file "output.txt" (lambda () (eq? p (current-output-port))))))
(check-equal #f
    (let ((p #f))
        (with-output-to-file "output.txt" (lambda () (set! p (current-output-port))))
        (output-port-open? p)))

(check-error (assertion-violation with-output-to-file) (with-output-to-file "output.txt"))
(check-error (assertion-violation with-output-to-file)
    (with-output-to-file "output.txt" (lambda () (current-output-port)) 3))

(check-equal #t
    (let* ((p (open-input-file "r7rs.scm"))
        (val (input-port-open? p)))
        (close-port p)
        val))
(check-equal #t
    (let* ((p (open-input-file "r7rs.scm")))
        (close-port p)
        (input-port? p)))

(check-error (assertion-violation open-input-file) (open-input-file))
(check-error (assertion-violation open-binary-input-file) (open-input-file 'r7rs.scm))
(check-error (assertion-violation open-input-file) (open-input-file "r7rs.scm" 2))

(check-equal #t
    (let* ((p (open-binary-input-file "r7rs.scm"))
        (val (input-port-open? p)))
        (close-port p)
        val))
(check-equal #t
    (let* ((p (open-binary-input-file "r7rs.scm")))
        (close-port p)
        (input-port? p)))
(check-equal #t
    (guard (obj
        ((file-error? obj) #t)
        (else #f))
        (open-binary-input-file
            (cond-expand
                (windows "not-a-directory\\not-a-file.txt")
                (else "not-a-directory/not-a-file.txt")))))

(check-error (assertion-violation open-binary-input-file) (open-binary-input-file))
(check-error (assertion-violation open-binary-input-file) (open-binary-input-file 'r7rs.scm))
(check-error (assertion-violation open-binary-input-file) (open-binary-input-file "r7rs.scm" 2))

(check-equal #t
    (let* ((p (open-output-file "output.txt"))
        (val (output-port-open? p)))
        (close-port p)
        val))
(check-equal #t
    (let* ((p (open-output-file "output.txt")))
        (close-port p)
        (output-port? p)))

(check-error (assertion-violation open-output-file) (open-output-file))
(check-error (assertion-violation open-binary-output-file) (open-output-file 'output.txt))
(check-error (assertion-violation open-output-file) (open-output-file "output.txt" 2))

(check-equal #t
    (let* ((p (open-binary-output-file "output.txt"))
        (val (output-port-open? p)))
        (close-port p)
        val))
(check-equal #t
    (let* ((p (open-binary-output-file "output.txt")))
        (close-port p)
        (output-port? p)))
(check-equal #t
    (guard (obj
        ((file-error? obj) #t)
        (else #f))
        (open-binary-output-file
            (cond-expand
                (windows "not-a-directory\\not-a-file.txt")
                (else "not-a-directory/not-a-file.txt")))))

(check-error (assertion-violation open-binary-output-file) (open-binary-output-file))
(check-error (assertion-violation open-binary-output-file) (open-binary-output-file 'output.txt))
(check-error (assertion-violation open-binary-output-file) (open-binary-output-file "output.txt" 2))

(check-equal #f
    (let ((p (open-input-file "r7rs.scm")))
        (close-port p)
        (input-port-open? p)))

(check-error (assertion-violation close-port) (close-port))
(check-error (assertion-violation close-port) (close-port 'port))

(check-equal #f
    (let ((p (open-input-file "r7rs.scm")))
        (close-input-port p)
        (input-port-open? p)))
(check-error (assertion-violation close-input-port)
    (let ((p (open-output-file "output.txt")))
        (close-input-port p)))

(check-error (assertion-violation close-input-port) (close-input-port))
(check-error (assertion-violation close-input-port) (close-input-port 'port))

(check-equal #f
    (let ((p (open-output-file "output.txt")))
        (close-output-port p)
        (output-port-open? p)))
(check-error (assertion-violation close-output-port)
    (let ((p (open-input-file "r7rs.scm")))
        (close-output-port p)))

(check-error (assertion-violation close-output-port) (close-output-port))
(check-error (assertion-violation close-output-port) (close-output-port 'port))

(check-equal (hello world) (read (open-input-string "(hello world)")))

(check-error (assertion-violation open-input-string) (open-input-string))
(check-error (assertion-violation open-input-string) (open-input-string 'string))
(check-error (assertion-violation open-input-string) (open-input-string "a string" 2))

(check-equal "(hello world)"
    (let ((p (open-output-string)))
        (write '(hello world) p)
        (close-port p)
        (get-output-string p)))

(check-error (assertion-violation open-output-string) (open-output-string "a string"))
(check-error (assertion-violation get-output-string) (get-output-string))
(check-error (assertion-violation get-output-string)
    (let ((p (open-output-file "output2.txt")))
        (get-output-string p)))

(check-equal "piece by piece by piece."
    (parameterize ((current-output-port (open-output-string)))
        (display "piece")
        (display " by piece ")
        (display "by piece.")
        (get-output-string (current-output-port))))

(check-equal #u8(1 2 3 4 5) (read-bytevector 5 (open-input-bytevector #u8(1 2 3 4 5))))

(check-error (assertion-violation open-input-bytevector) (open-input-bytevector))
(check-error (assertion-violation open-input-bytevector) (open-input-bytevector #(1 2 3 4 5)))
(check-error (assertion-violation open-input-bytevector) (open-input-bytevector #u8(1 2) 3))

(check-equal #u8(1 2 3 4)
    (let ((p (open-output-bytevector)))
        (write-u8 1 p)
        (write-u8 2 p)
        (write-u8 3 p)
        (write-u8 4 p)
        (close-output-port p)
        (get-output-bytevector p)))

(check-error (assertion-violation open-output-bytevector) (open-output-bytevector #u8(1 2 3)))
(check-error (assertion-violation get-output-bytevector) (get-output-bytevector))
(check-error (assertion-violation get-output-bytevector)
    (let ((p (open-binary-output-file "output2.txt")))
        (get-output-bytevector p)))

;;
;; ---- input ----
;;

(check-equal (hello world)
    (parameterize ((current-input-port (open-input-string "(hello world)")))
        (read)))
(check-equal (hello world) (read (open-input-string "(hello world)")))
(check-equal #t
    (guard (obj
        ((read-error? obj) #t)
        (else #f))
        (read (open-input-string "(hello world"))))

(check-error (assertion-violation read) (read 'port))
(check-error (assertion-violation read) (read (open-input-bytevector #u8(1 2 3))))
(check-error (assertion-violation read) (read (current-input-port) 2))
(check-error (assertion-violation read) (read (current-output-port)))

(check-equal #\A (read-char (open-input-string "ABCD")))
(check-equal #\D
    (parameterize ((current-input-port (open-input-string "ABCDEFG")))
        (read-char)
        (read-char)
        (read-char)
        (peek-char)
        (read-char)))

(check-error (assertion-violation read-char) (read-char 'port))
(check-error (assertion-violation read-char) (read-char (open-input-bytevector #u8(1 2 3))))
(check-error (assertion-violation read-char) (read-char (current-input-port) 2))
(check-error (assertion-violation read-char) (read-char (current-output-port)))

(check-equal #\A (peek-char (open-input-string "ABCD")))
(check-equal #\D
    (parameterize ((current-input-port (open-input-string "ABCDEFG")))
        (read-char)
        (read-char)
        (read-char)
        (peek-char)))

(check-error (assertion-violation peek-char) (peek-char 'port))
(check-error (assertion-violation peek-char) (peek-char (open-input-bytevector #u8(1 2 3))))
(check-error (assertion-violation peek-char) (peek-char (current-input-port) 2))
(check-error (assertion-violation peek-char) (peek-char (current-output-port)))

(check-equal "abcd" (read-line (open-input-string "abcd\nABCD\n")))
(check-equal "ABCD"
    (let ((p (open-input-string "abcd\n\nABCD\n")))
        (read-line p)
        (read-line p)
        (read-line p)))

(check-error (assertion-violation read-line) (read-line 'port))
(check-error (assertion-violation read-line) (read-line (open-input-bytevector #u8(1 2 3))))
(check-error (assertion-violation read-line) (read-line (current-input-port) 2))
(check-error (assertion-violation read-line) (read-line (current-output-port)))

(check-equal #t (eof-object? (read (open-input-string ""))))
(check-equal #f (eof-object? 'eof))

(check-error (assertion-violation eof-object?) (eof-object?))
(check-error (assertion-violation eof-object?) (eof-object? 1 2))

(check-equal #t (eof-object? (eof-object)))

(check-error (assertion-violation eof-object) (eof-object 1))

(check-equal #t (char-ready? (open-input-string "abc")))

(check-error (assertion-violation char-ready?) (char-ready? 'port))
(check-error (assertion-violation char-ready?) (char-ready? (open-input-bytevector #u8(1 2 3))))
(check-error (assertion-violation char-ready?) (char-ready? (current-input-port) 2))
(check-error (assertion-violation char-ready?) (char-ready? (current-output-port)))

(check-equal "abcd" (read-string 4 (open-input-string "abcdefgh")))
(check-equal "efg"
    (let ((p (open-input-string "abcdefg")))
        (read-string 4 p)
        (read-string 4 p)))

(check-equal #t
    (eof-object?
        (let ((p (open-input-string "abcdefg")))
            (read-string 4 p)
            (read-string 4 p)
            (read-string 4 p))))

(check-error (assertion-violation read-string) (read-string -1 (current-output-port)))
(check-error (assertion-violation read-string) (read-string 1 'port))
(check-error (assertion-violation read-string) (read-string 1 (open-input-bytevector #u8(1 2 3))))
(check-error (assertion-violation read-string) (read-string 1 (current-input-port) 2))
(check-error (assertion-violation read-string) (read-string 1 (current-output-port)))

(check-equal 1 (read-u8 (open-input-bytevector #u8(1 2 3 4 5))))

(check-error (assertion-violation read-u8) (read-u8 'port))
(check-error (assertion-violation read-u8) (read-u8 (open-input-string "1234")))
(check-error (assertion-violation read-u8) (read-u8 (open-input-bytevector #u8(1 2 3)) 2))
(check-error (assertion-violation read-u8) (read-u8 (current-output-port)))

(check-equal 1 (peek-u8 (open-input-bytevector #u8(1 2 3 4 5))))

(check-error (assertion-violation peek-u8) (peek-u8 'port))
(check-error (assertion-violation peek-u8) (peek-u8 (open-input-string "1234")))
(check-error (assertion-violation peek-u8) (peek-u8 (open-input-bytevector #u8(1 2 3)) 2))
(check-error (assertion-violation peek-u8) (peek-u8 (current-output-port)))

(check-equal #t (u8-ready? (open-input-bytevector #u8(1 2 3 4 5))))

(check-error (assertion-violation u8-ready?) (u8-ready? 'port))
(check-error (assertion-violation u8-ready?) (u8-ready? (open-input-string "1234")))
(check-error (assertion-violation u8-ready?) (u8-ready? (open-input-bytevector #u8(1 2 3)) 2))
(check-error (assertion-violation u8-ready?) (u8-ready? (current-output-port)))

(check-equal #u8(1 2 3 4) (read-bytevector 4 (open-input-bytevector #u8(1 2 3 4 5 6 7 8))))
(check-equal #u8(1 2 3 4) (read-bytevector 8 (open-input-bytevector #u8(1 2 3 4))))

(check-error (assertion-violation read-bytevector)
        (read-bytevector -1 (open-input-bytevector #u8(1 2 3))))
(check-error (assertion-violation read-bytevector) (read-bytevector 1 'port))
(check-error (assertion-violation read-bytevector) (read-bytevector 1 (open-input-string "1234")))
(check-error (assertion-violation read-bytevector)
        (read-bytevector 1 (open-input-bytevector #u8(1 2 3)) 2))
(check-error (assertion-violation read-bytevector) (read-bytevector 1 (current-output-port)))

(check-equal #u8(1 2 3 4)
    (let ((bv (make-bytevector 4 0)))
        (read-bytevector! bv (open-input-bytevector #u8(1 2 3 4 5 6 7 8)))
        bv))
(check-equal #u8(0 1 2 3)
    (let ((bv (make-bytevector 4 0)))
        (read-bytevector! bv (open-input-bytevector #u8(1 2 3 4 5 6 7 8)) 1)
        bv))
(check-equal #u8(0 1 2 0)
    (let ((bv (make-bytevector 4 0)))
        (read-bytevector! bv (open-input-bytevector #u8(1 2 3 4 5 6 7 8)) 1 3)
        bv))

(define bv (make-bytevector 4 0))
(check-error (assertion-violation read-bytevector!) (read-bytevector! #(1 2 3)))
(check-error (assertion-violation read-bytevector!) (read-bytevector! bv 'port))
(check-error (assertion-violation read-bytevector!) (read-bytevector! bv (current-input-port)))
(check-error (assertion-violation read-bytevector!)
    (read-bytevector! bv (open-input-bytevector #u8()) -1))
(check-error (assertion-violation read-bytevector!)
    (read-bytevector! bv (open-input-bytevector #u8()) 1 0))
(check-error (assertion-violation read-bytevector!)
    (read-bytevector! bv (open-input-bytevector #u8()) 1 5))

;;
;; ---- output ----
;;

(check-equal "#0=(a b c . #0#)"
    (let ((p (open-output-string)))
        (let ((x (list 'a 'b 'c)))
            (set-cdr! (cddr x) x)
            (write x p)
            (get-output-string p))))

(check-equal "#0=(val1 . #0#)"
    (let ((p (open-output-string)))
        (let ((a (cons 'val1 'val2)))
           (set-cdr! a a)
           (write a p)
           (get-output-string p))))

(check-equal "((a b c) a b c)"
    (let ((p (open-output-string)))
        (let ((x (list 'a 'b 'c)))
            (write (cons x x) p)
            (get-output-string p))))

(check-equal "(#0=(a b c) . #0#)"
    (let ((p (open-output-string)))
        (let ((x (list 'a 'b 'c)))
            (write-shared (cons x x) p)
            (get-output-string p))))

(check-equal "#0=(#\\a \"b\" c . #0#)"
    (let ((p (open-output-string)))
        (let ((x (list #\a "b" 'c)))
            (set-cdr! (cddr x) x)
            (write x p)
            (get-output-string p))))

(check-equal "#0=(a b c . #0#)"
    (let ((p (open-output-string)))
        (let ((x (list #\a "b" 'c)))
            (set-cdr! (cddr x) x)
            (display x p)
            (get-output-string p))))

(check-equal "((a b c) a b c)"
    (let ((p (open-output-string)))
        (let ((x (list 'a 'b 'c)))
            (write-simple (cons x x) p)
            (get-output-string p))))

(check-error (assertion-violation write) (write))
(check-error (assertion-violation write) (write #f (current-input-port)))
(check-error (assertion-violation write) (write #f (current-output-port) 3))
(check-error (assertion-violation write) (write #f (open-output-bytevector)))

(check-error (assertion-violation write-shared) (write-shared))
(check-error (assertion-violation write-shared) (write-shared #f (current-input-port)))
(check-error (assertion-violation write-shared) (write-shared #f (current-output-port) 3))
(check-error (assertion-violation write-shared) (write-shared #f (open-output-bytevector)))

(check-error (assertion-violation write-simple) (write-simple))
(check-error (assertion-violation write-simple) (write-simple #f (current-input-port)))
(check-error (assertion-violation write-simple) (write-simple #f (current-output-port) 3))
(check-error (assertion-violation write-simple) (write-simple #f (open-output-bytevector)))

(check-error (assertion-violation display) (display))
(check-error (assertion-violation display) (display #f (current-input-port)))
(check-error (assertion-violation display) (display #f (current-output-port) 3))
(check-error (assertion-violation display) (display #f (open-output-bytevector)))

(check-equal "\n"
    (let ((p (open-output-string)))
        (newline p)
        (get-output-string p)))

(check-error (assertion-violation newline) (newline (current-input-port)))
(check-error (assertion-violation newline) (newline (current-output-port) 2))
(check-error (assertion-violation newline) (newline (open-output-bytevector)))

(check-equal "a"
    (let ((p (open-output-string)))
        (write-char #\a p)
        (get-output-string p)))

(check-error (assertion-violation write-char) (write-char #f))
(check-error (assertion-violation write-char) (write-char #\a (current-input-port)))
(check-error (assertion-violation write-char) (write-char #\a (current-output-port) 3))
(check-error (assertion-violation write-char) (write-char #\a (open-output-bytevector)))

(check-equal "abcd"
    (let ((p (open-output-string)))
        (write-string "abcd" p)
        (get-output-string p)))
(check-equal "bcd"
    (let ((p (open-output-string)))
        (write-string "abcd" p 1)
        (get-output-string p)))
(check-equal "bc"
    (let ((p (open-output-string)))
        (write-string "abcd" p 1 3)
        (get-output-string p)))

(check-error (assertion-violation write-string) (write-string #\a))
(check-error (assertion-violation write-string) (write-string "a" (current-input-port)))
(check-error (assertion-violation write-string) (write-string "a" (open-output-bytevector)))
(check-error (assertion-violation write-string) (write-string "a" (current-output-port) -1))
(check-error (assertion-violation write-string) (write-string "a" (current-output-port) 1 0))
(check-error (assertion-violation write-string) (write-string "a" (current-output-port) 1 2))

(check-equal #u8(1)
    (let ((p (open-output-bytevector)))
        (write-u8 1 p)
        (get-output-bytevector p)))

(check-error (assertion-violation write-u8) (write-u8 #f))
(check-error (assertion-violation write-u8) (write-u8 1 (current-input-port)))
(check-error (assertion-violation write-u8) (write-u8 1 (current-output-port)))
(check-error (assertion-violation write-u8) (write-u8 1 (open-output-bytevector) 3))

(check-equal #u8(1 2 3 4)
    (let ((p (open-output-bytevector)))
        (write-bytevector #u8(1 2 3 4) p)
        (get-output-bytevector p)))
(check-equal #u8(2 3 4)
    (let ((p (open-output-bytevector)))
        (write-bytevector #u8(1 2 3 4) p 1)
        (get-output-bytevector p)))
(check-equal #u8(2 3)
    (let ((p (open-output-bytevector)))
        (write-bytevector #u8(1 2 3 4) p 1 3)
        (get-output-bytevector p)))

(check-error (assertion-violation write-bytevector) (write-bytevector #(1 2 3 4)))
(check-error (assertion-violation write-bytevector) (write-bytevector #u8() (current-input-port)))
(check-error (assertion-violation write-bytevector) (write-bytevector #u8() (current-output-port)))
(check-error (assertion-violation write-bytevector)
    (write-bytevector #u8(1) (open-output-bytevector) -1))
(check-error (assertion-violation write-bytevector)
    (write-bytevector #u8(1) (open-output-bytevector) 1 0))
(check-error (assertion-violation write-bytevector)
    (write-bytevector #u8(1) (open-output-bytevector) 1 2))

(check-equal #t (begin (flush-output-port) #t))

(check-error (assertion-violation flush-output-port) (flush-output-port (current-input-port)))
(check-error (assertion-violation flush-output-port) (flush-output-port (current-output-port) 2))

;;
;; ---- system interface ----
;;

(check-equal #t
    (begin
        (load "loadtest.scm" (interaction-environment))
        (eval 'load-test (interaction-environment))))

(check-error (assertion-violation) (load))
(check-error (assertion-violation) (load |filename|))
(check-error (assertion-violation) (load "filename" (interaction-environment) 3))

(check-equal #f (file-exists? "not-a-real-filename"))
(check-equal #t (file-exists? "r7rs.scm"))

(check-error (assertion-violation file-exists?) (file-exists?))
(check-error (assertion-violation file-exists?) (file-exists? #\a))
(check-error (assertion-violation file-exists?) (file-exists? "filename" 2))

(check-equal #t
    (guard (obj
        ((file-error? obj) #t)
        (else #f))
        (delete-file
            (cond-expand
                (windows "not-a-directory\\not-a-file.txt")
                (else "not-a-directory/not-a-file.txt")))))

(check-error (assertion-violation delete-file) (delete-file))
(check-error (assertion-violation delete-file) (delete-file #\a))
(check-error (assertion-violation delete-file) (delete-file "filename" 2))

(check-error (assertion-violation command-line) (command-line 1))

(check-error (assertion-violation emergency-exit) (emergency-exit 1 2))

(check-equal #f (get-environment-variable "not the name of an environment variable"))
(check-equal #t (string? (get-environment-variable (cond-expand (windows "Path") (else "PATH")))))

(check-error (assertion-violation get-environment-variable) (get-environment-variable))
(check-error (assertion-violation get-environment-variable) (get-environment-variable #\a))
(check-error (assertion-violation get-environment-variable) (get-environment-variable "Path" 2))

(check-equal #f (assoc "not the name of an environment variable" (get-environment-variables)))
(check-equal #t (string?
    (car (assoc (cond-expand (windows "Path") (else "PATH"))(get-environment-variables)))))

(check-error (assertion-violation get-environment-variables) (get-environment-variables 1))

(check-equal #t (> (current-second) 0))

(check-error (assertion-violation current-second) (current-second 1))

(check-equal #t (> (current-jiffy) 0))

(check-error (assertion-violation current-jiffy) (current-jiffy 1))

(check-equal #t (> (jiffies-per-second) 0))

(check-error (assertion-violation jiffies-per-second) (jiffies-per-second 1))

(check-equal #f (memq 'not-a-feature (features)))
(check-equal #t (pair? (memq 'r7rs (features))))

(check-error (assertion-violation features) (features 1))

