;;;
;;; R7RS
;;;

(import (scheme base))
(import (scheme case-lambda))
(import (scheme char))
(import (scheme complex))
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

(cond-expand ((library (chibi test)) (import (chibi test))))

(cond-expand
    (chibi
        (define-syntax check-equal
            (syntax-rules () ((check-equal expect expr)
                    (test-propagate-info #f 'expect expr ())))))
    (else
        (define-syntax check-equal
            (syntax-rules () ((check-equal expect expr) (test 'expect expr))))))

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

(define (q2 u)
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
(check-equal 4 (q2 2))

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

(define (make-counter2 n)
    (lambda () (set! n (+ n 1)) n))
(define c1 (make-counter2 0))
(check-equal 1 (c1))
(check-equal 2 (c1))
(check-equal 3 (c1))
(define c2 (make-counter2 100))
(check-equal 101 (c2))
(check-equal 102 (c2))
(check-equal 103 (c2))
(check-equal 4 (c1))

(define (q3 x)
    (set! x 10) x)
(check-equal 10 (q3 123))

(define (q4 x)
    (define (r y)
        (set! x y))
    (r 10)
    x)
(check-equal 10 (q4 123))

;; include

(check-equal (10 20) (let () (include "include.scm") (list INCLUDE-A include-b)))

(check-equal 10 (let ((a 0) (B 0)) (set! a 1) (set! B 1) (include "include2.scm") a))
(check-equal 20 (let ((a 0) (B 0)) (set! a 1) (set! B 1) (include "include2.scm") B))

;; include-ci

(check-equal (10 20) (let () (include-ci "include5.scm") (list include-g include-h)))

(check-equal 10 (let ((a 0) (b 0)) (set! a 1) (set! b 1) (include-ci "include2.scm") a))
(check-equal 20 (let ((a 0) (b 0)) (set! a 1) (set! b 1) (include-ci "include2.scm") b))

;; cond

(check-equal greater (cond ((> 3 2) 'greater) ((< 3 2) 'less)))
(check-equal equal (cond ((> 3 3) 'greater) ((< 3 3) 'less) (else 'equal)))
(check-equal 2 (cond ('(1 2 3) => cadr) (else #f)))
(check-equal 2 (cond ((assv 'b '((a 1) (b 2))) => cadr) (else #f)))

;; case

(check-equal composite (case (* 2 3) ((2 3 5 7) 'prime) ((1 4 6 8 9) 'composite)))
(check-equal #f (eq? 'c (case (car '(c d)) ((a) 'a) ((b) 'b))))
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
(check-equal #f (let ((ret (case (car '(c d)) ((a) 'a) ((b) 'b)))) (eq? ret 'c)))
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

(check-equal 1
    (let ((x 1)) (cond-expand (r7rs) (else (set! x 2))) x))

(define ce 1)
(cond-expand (r7rs) (else (set! ce 2)))
(check-equal 1 ce)

(check-equal 2
    (let ((x 1)) (cond-expand (not-a-feature) (else (set! x 2))) x))

(define ce2 1)
(cond-expand (not-a-feature) (else (set! ce2 2)))
(check-equal 2 ce2)

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
            (if (> count fx)
                count
                (force p)))))
(define fx 5)
(check-equal 6 (force p))
(set! fx 10)
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

(define p2 (delay (+ 1 2 3 4)))
(check-equal #t (eq? p2 (make-promise p2)))

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

(define range2
    (case-lambda
        ((e) (range2 0 e))
        ((b e) (do ((r '() (cons e r))
                    (e (- e 1) (- e 1)))
                    ((< e b) r)))))

(check-equal (0 1 2) (range2 3))
(check-equal (3 4) (range2 3 5))
(check-error (assertion-violation case-lambda) (range2 1 2 3))

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
    (let ((cl (case-lambda ((a) 'one) ((b c) 'two) ((d e f . g) 'rest)))) (eq? cl cl) (cl)))
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
        (guard (excpt ((= excpt 10) (- 10)) ((= excpt 12) (- 12))) (raise-continuable 11)))))

;; http://lists.scheme-reports.org/pipermail/scheme-reports/2012-March/001987.html
(check-equal (a b c d a b) (let ((events '()))
   (guard (c
           (#t #f))
     (guard (c
             ((dynamic-wind
                  (lambda () (set! events (cons 'c events)))
                  (lambda () #f)
                  (lambda () (set! events (cons 'd events))))
              #f))
       (dynamic-wind
           (lambda () (set! events (cons 'a events)))
           (lambda () (raise 'error))
           (lambda () (set! events (cons 'b events))))))
   (reverse events)))

(check-equal (x a b c d a b y) (let ((events '()))
    (dynamic-wind
        (lambda () (set! events (cons 'x events)))
        (lambda ()
            (guard (c
                    (#t #f))
              (guard (c
                      ((dynamic-wind
                           (lambda () (set! events (cons 'c events)))
                           (lambda () #f)
                           (lambda () (set! events (cons 'd events))))
                       #f))
                (dynamic-wind
                    (lambda () (set! events (cons 'a events)))
                    (lambda () (raise 'error))
                    (lambda () (set! events (cons 'b events)))))))
        (lambda () (set! events (cons 'y events))))
    (reverse events)))

(check-equal a
    (guard (c
        ((eq? c 'a) 'a)
        (else 'else))
        (raise 'a)
        'after))

(check-equal else
    (guard (c
        ((eq? c 'a) 'a)
        (else 'else))
        (raise 'b)
        'after))

(check-equal a
    (guard (c
        ((eq? c 'a) 'a))
        (raise 'a)
        'after))

(check-equal after
    (with-exception-handler
        (lambda (c) c)
        (lambda ()
            (guard (c
                ((eq? c 'a) 'a))
                (raise-continuable 'b)
                'after))))

(check-equal handler
    (with-exception-handler
        (lambda (c) 'handler)
        (lambda ()
            (guard (c
                ((eq? c 'a) 'a))
                (raise-continuable 'b)))))

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

;; define-values

(define-values (dv1 dv2 dv3) (values 1 2 3))
(check-equal (1 2 3) (list dv1 dv2 dv3))

(define-values () (values))

(check-equal (a b c) (let () (define-values (x y z) (values 'a 'b 'c)) (list x y z)))

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

;;
;; ---- equivalence predicates ----
;;

(check-equal #t (eqv? 'a 'a))
(check-equal #f (eqv? 'a 'b))
(check-equal #t (eqv? 2 2))
(check-equal #f (eqv? 2 2.0))
(check-equal #t (eqv? '() '()))
(check-equal #t (eqv? 100000000 100000000))
(check-equal #f (eqv? 0.0 +nan.0))
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
(check-equal #t (equal? '#1=(a b . #1#) '#2=(a b a b . #2#)))

(check-error (assertion-violation equal?) (equal? 1))
(check-error (assertion-violation equal?) (equal? 1 2 3))

;; The next three tests are from:
;; http://srfi.schemers.org/srfi-85/post-mail-archive/msg00001.html

(check-equal #t
 (let ()
   (define x
     (let ((x1 (vector 'h))
           (x2 (let ((x (list #f))) (set-car! x x) x)))
       (vector x1 (vector 'h) x1 (vector 'h) x1 x2)))
   (define y
     (let ((y1 (vector 'h))
           (y2 (vector 'h))
           (y3 (let ((x (list #f))) (set-car! x x) x)))
       (vector (vector 'h) y1 y1 y2 y2 y3)))
   (equal? x y)))

(check-equal #t
 (let ()
   (define x
     (let ((x (cons (cons #f 'a) 'a)))
       (set-car! (car x) x)
       x))
   (define y
     (let ((y (cons (cons #f 'a) 'a)))
       (set-car! (car y) (car y))
       y))
   (equal? x y)))

(check-equal #t
  (let ((k 100))
    (define x
      (let ((x1 (cons 
                (let f ((n k))
                  (if (= n 0)
                      (let ((x0 (cons #f #f)))
                        (set-car! x0 x0)
                        (set-cdr! x0 x0)
                        x0)
                      (let ((xi (cons #f (f (- n 1)))))
                        (set-car! xi xi)
                        xi)))
                #f)))
      (set-cdr! x1 x1)
      x1))
  (define y
    (let* ((y2 (cons #f #f)) (y1 (cons y2 y2)))
      (set-car! y2 y1)
      (set-cdr! y2 y1)
      y1))
  (equal? x y)))

;;
;; ---- numbers ----
;;

(check-equal #t (number? 0))
(check-equal #t (number? 0.0))
(check-equal #t (number? -0.0))
(check-equal #t (number? +inf.0))
(check-equal #t (number? -inf.0))
(check-equal #t (number? +nan.0))
(check-equal #t (number? -nan.0))
(check-equal #t (number? 123))
(check-equal #t (number? -123))
(check-equal #t (number? 123.456))
(check-equal #t (number? -123.456))
(check-equal #t (number? 123/456))
(check-equal #t (number? -123/456))
(check-equal #t (number? 12345678901234567890))
(check-equal #t (number? -12345678901234567890))
(check-equal #t (number? 123+456i))
(check-equal #t (number? -123/456-123i))
(check-equal #t (number? 123.456+89i))

(check-equal #f (number? "123"))
(check-equal #f (number? #\1))
(check-equal #f (number? #f))

(check-error (assertion-violation number?) (number?))
(check-error (assertion-violation number?) (number? 1 2))

(check-equal #t (complex? 0))
(check-equal #t (complex? 0.0))
(check-equal #t (complex? -0.0))
(check-equal #t (complex? +inf.0))
(check-equal #t (complex? -inf.0))
(check-equal #t (complex? +nan.0))
(check-equal #t (complex? -nan.0))
(check-equal #t (complex? 123))
(check-equal #t (complex? -123))
(check-equal #t (complex? 123.456))
(check-equal #t (complex? -123.456))
(check-equal #t (complex? 123/456))
(check-equal #t (complex? -123/456))
(check-equal #t (complex? 12345678901234567890))
(check-equal #t (complex? -12345678901234567890))
(check-equal #t (complex? 123+456i))
(check-equal #t (complex? -123/456-123i))
(check-equal #t (complex? 123.456+89i))

(check-equal #f (complex? "123"))
(check-equal #f (complex? #\1))
(check-equal #f (complex? #f))

(check-error (assertion-violation complex?) (complex?))
(check-error (assertion-violation complex?) (complex? 1 2))

(check-equal #t (real? 0))
(check-equal #t (real? 0.0))
(check-equal #t (real? -0.0))
(check-equal #t (real? +inf.0))
(check-equal #t (real? -inf.0))
(check-equal #t (real? +nan.0))
(check-equal #t (real? -nan.0))
(check-equal #t (real? 123))
(check-equal #t (real? -123))
(check-equal #t (real? 123.456))
(check-equal #t (real? -123.456))
(check-equal #t (real? 123/456))
(check-equal #t (real? -123/456))
(check-equal #t (real? 12345678901234567890))
(check-equal #t (real? -12345678901234567890))
(check-equal #f (real? 123+456i))
(check-equal #f (real? -123/456-123i))
(check-equal #f (real? 123.456+89i))

(check-equal #f (real? "123"))
(check-equal #f (real? #\1))
(check-equal #f (real? #f))

(check-error (assertion-violation real?) (real?))
(check-error (assertion-violation real?) (real? 1 2))

(check-equal #t (rational? 0))
(check-equal #t (rational? 0.0))
(check-equal #t (rational? -0.0))
(check-equal #f (rational? +inf.0))
(check-equal #f (rational? -inf.0))
(check-equal #f (rational? +nan.0))
(check-equal #f (rational? -nan.0))
(check-equal #t (rational? 123))
(check-equal #t (rational? -123))
(check-equal #t (rational? 123.456))
(check-equal #t (rational? -123.456))
(check-equal #t (rational? 123/456))
(check-equal #t (rational? -123/456))
(check-equal #t (rational? 12345678901234567890))
(check-equal #t (rational? -12345678901234567890))
(check-equal #f (rational? 123+456i))
(check-equal #f (rational? -123/456-123i))
(check-equal #f (rational? 123.456+89i))

(check-equal #f (rational? "123"))
(check-equal #f (rational? #\1))
(check-equal #f (rational? #f))

(check-error (assertion-violation rational?) (rational?))
(check-error (assertion-violation rational?) (rational? 1 2))

(check-equal #t (integer? 0))
(check-equal #t (integer? 0.0))
(check-equal #f (integer? 0.1))
(check-equal #t (integer? -0.0))
(check-equal #f (integer? +inf.0))
(check-equal #f (integer? -inf.0))
(check-equal #f (integer? +nan.0))
(check-equal #f (integer? -nan.0))
(check-equal #t (integer? 123))
(check-equal #t (integer? -123))
(check-equal #f (integer? 123.456))
(check-equal #f (integer? -123.456))
(check-equal #f (integer? 123/456))
(check-equal #f (integer? -123/456))
(check-equal #t (integer? 12345678901234567890))
(check-equal #t (integer? -12345678901234567890))
(check-equal #f (integer? 123+456i))
(check-equal #f (integer? -123/456-123i))
(check-equal #f (integer? 123.456+89i))
(check-equal #t (integer? 123.000))
(check-equal #f (integer? 123.00000678))

(check-equal #f (integer? "123"))
(check-equal #f (integer? #\1))
(check-equal #f (integer? #f))

(check-error (assertion-violation integer?) (integer?))
(check-error (assertion-violation integer?) (integer? 1 2))

(check-equal #t (complex? 3+4i))
(check-equal #t (complex? 3))
(check-equal #t (real? 3))
(check-equal #t (real? -2.5+0i))
(check-equal #f (real? -2.5+0.0i))
(check-equal #t (real? #e1e10))
(check-equal #t (real? +inf.0))
(check-equal #t (real? +nan.0))
(check-equal #f (rational? -inf.0))
(check-equal #t (rational? 3.5))
(check-equal #t (rational? 6/10))
(check-equal #t (rational? 6/3))
(check-equal #t (integer? 3+0i))
(check-equal #t (integer? 3.0))
(check-equal #t (integer? 8/4))

(check-equal #t (exact? 0))
(check-equal #f (exact? 0.0))
(check-equal #t (exact? 123))
(check-equal #t (exact? 12345678901234567890))
(check-equal #f (exact? 123.456))
(check-equal #t (exact? 123/456))
(check-equal #t (exact? 123+456i))
(check-equal #f (exact? 123+456.789i))

(check-error (assertion-violation exact?) (exact? "1"))
(check-error (assertion-violation exact?) (exact? #\1))
(check-error (assertion-violation exact?) (exact? #f))
(check-error (assertion-violation exact?) (exact?))
(check-error (assertion-violation exact?) (exact? 1 2))

(check-equal #f (exact? 3.0))
(check-equal #t (exact? #e3.0))

(check-equal #f (inexact? 0))
(check-equal #t (inexact? 0.0))
(check-equal #f (inexact? 123))
(check-equal #f (inexact? 12345678901234567890))
(check-equal #t (inexact? 123.456))
(check-equal #f (inexact? 123/456))
(check-equal #f (inexact? 123+456i))
(check-equal #t (inexact? 123+456.789i))

(check-error (assertion-violation inexact?) (inexact? "1"))
(check-error (assertion-violation inexact?) (inexact? #\1))
(check-error (assertion-violation inexact?) (inexact? #f))
(check-error (assertion-violation inexact?) (inexact?))
(check-error (assertion-violation inexact?) (inexact? 1 2))

(check-equal #t (inexact? 3.))

(check-equal #t (exact-integer? 0))
(check-equal #f (exact-integer? 0.0))
(check-equal #t (exact-integer? 123))
(check-equal #t (exact-integer? 12345678901234567890))
(check-equal #f (exact-integer? 123.456))
(check-equal #f (exact-integer? 123/456))
(check-equal #f (exact-integer? 123+456i))
(check-equal #f (exact-integer? 123+456.789i))

(check-error (assertion-violation exact-integer?) (exact-integer? "1"))
(check-error (assertion-violation exact-integer?) (exact-integer? #\1))
(check-error (assertion-violation exact-integer?) (exact-integer? #f))
(check-error (assertion-violation exact-integer?) (exact-integer?))
(check-error (assertion-violation exact-integer?) (exact-integer? 1 2))

(check-equal #t (exact-integer? 32))
(check-equal #f (exact-integer? 32.0))
(check-equal #f (exact-integer? 32/5))

(check-equal #t (finite? 0))
(check-equal #f (finite? +inf.0))
(check-equal #f (finite? -inf.0))
(check-equal #f (finite? +nan.0))
(check-equal #t (finite? 123))
(check-equal #t (finite? 12345678901234567890))
(check-equal #t (finite? 123.456))
(check-equal #t (finite? 123/456))
(check-equal #t (finite? 123+456i))
(check-equal #t (finite? 123+456.789i))
(check-equal #f (finite? 123+inf.0i))
(check-equal #f (finite? -inf.0+12.0i))
(check-equal #f (finite? 123+nan.0i))

(check-error (assertion-violation finite?) (finite? "1"))
(check-error (assertion-violation finite?) (finite? #\1))
(check-error (assertion-violation finite?) (finite? #f))
(check-error (assertion-violation finite?) (finite?))
(check-error (assertion-violation finite?) (finite? 1 2))

(check-equal #t (finite? 3))
(check-equal #f (finite? +inf.0))
(check-equal #f (finite? 3.0+inf.0i))

(check-equal #f (infinite? 0))
(check-equal #t (infinite? +inf.0))
(check-equal #t (infinite? -inf.0))
(check-equal #f (infinite? +nan.0))
(check-equal #f (infinite? 123))
(check-equal #f (infinite? 12345678901234567890))
(check-equal #f (infinite? 123.456))
(check-equal #f (infinite? 123/456))
(check-equal #f (infinite? 123+456i))
(check-equal #f (infinite? 123+456.789i))
(check-equal #t (infinite? 123+inf.0i))
(check-equal #t (infinite? -inf.0+12.0i))
(check-equal #f (infinite? 123+nan.0i))

(check-error (assertion-violation infinite?) (infinite? "1"))
(check-error (assertion-violation infinite?) (infinite? #\1))
(check-error (assertion-violation infinite?) (infinite? #f))
(check-error (assertion-violation infinite?) (infinite?))
(check-error (assertion-violation infinite?) (infinite? 1 2))

(check-equal #f (infinite? 3))
(check-equal #t (infinite? +inf.0))
(check-equal #f (infinite? +nan.0))
(check-equal #t (infinite? 3.0+inf.0i))

(check-equal #f (nan? 0))
(check-equal #f (nan? +inf.0))
(check-equal #f (nan? -inf.0))
(check-equal #t (nan? +nan.0))
(check-equal #f (nan? 123))
(check-equal #f (nan? 12345678901234567890))
(check-equal #f (nan? 123.456))
(check-equal #f (nan? 123/456))
(check-equal #f (nan? 123+456i))
(check-equal #f (nan? 123+456.789i))
(check-equal #f (nan? 123+inf.0i))
(check-equal #f (nan? -inf.0+12.0i))
(check-equal #t (nan? 123+nan.0i))

(check-error (assertion-violation nan?) (nan? "1"))
(check-error (assertion-violation nan?) (nan? #\1))
(check-error (assertion-violation nan?) (nan? #f))
(check-error (assertion-violation nan?) (nan?))
(check-error (assertion-violation nan?) (nan? 1 2))

(check-equal #t (nan? +nan.0))
(check-equal #f (nan? 32))
(check-equal #t (nan? +nan.0+5.0i))
(check-equal #f (nan? 1+2i))

(check-equal #t (= 0 0))
(check-equal #t (= 123 123))
(check-equal #t (= 12345678901234567890 12345678901234567890))
(check-equal #t (= 123.456 123.456))
(check-equal #t (= 123/456 41/152))
(check-equal #t (= 123+456i 123+456i))
(check-equal #t (= 12.3+4.56i 12.3+4.56i))
(check-equal #f (= 123 124))
(check-equal #f (= 12345678901234567890 12345678901234567891))
(check-equal #f (= 123.456 123.457))
(check-equal #f (= 123/457 41/152))
(check-equal #f (= 123+456i 123+457i))
(check-equal #f (= 12.3+4.56i 12.4+45.6i))
(check-equal #t (= 123 123 123))
(check-equal #t (= 12345678901234567890 12345678901234567890 12345678901234567890))
(check-equal #t (= 123.456 123.456 123.456))
(check-equal #t (= 123/456 41/152 123/456))
(check-equal #t (= 123+456i 123+456i 123+456i))
(check-equal #t (= 12.3+4.56i 12.3+4.56i 12.3+4.56i))
(check-equal #f (= 123 123 122))
(check-equal #f (= 12345678901234567890 12345678901234567890 12345678901234567889))
(check-equal #f (= 123.456 123.456 123.455))
(check-equal #f (= 123/456 41/152 123/455))
(check-equal #f (= 123+456i 123+456i 123+455i))
(check-equal #f (= 12.3+4.56i 12.3+4.56i 12.2+4.56i))

(check-equal #t (= -0.0 0.0))
(check-equal #t (= 0 0.0))
(check-equal #t (= 0.0 0))
(check-equal #t (= 123 123.0))
(check-equal #t (= 123.0 123))

(check-equal #t (= 1524074060357399025/12345 123456789012345))
(check-equal #t (= 123456789012345 1524074060357399025/12345))

(define big (expt 2 1000))
(check-equal #f (and (= (- big 1) (inexact big)) (= (+ big 1) (inexact big))))
(check-equal #f (= (- big 1) (+ big 1)))

(check-error (assertion-violation =) (= 1 "1"))
(check-error (assertion-violation =) (= 1 #\1))
(check-error (assertion-violation =) (= 1 #f))
(check-error (assertion-violation =) (= "1" 1))
(check-error (assertion-violation =) (= #\1 1))
(check-error (assertion-violation =) (= #f 1))
(check-error (assertion-violation =) (= 1 1 "1"))
(check-error (assertion-violation =) (= 1 1 #\1))
(check-error (assertion-violation =) (= 1 1 #f))
(check-error (assertion-violation =) (=))
(check-error (assertion-violation =) (= 1))

(check-equal #t (< 0 1))
(check-equal #t (< 123 124))
(check-equal #t (< 12345678901234567890 12345678901234567891))
(check-equal #t (< 123.456 123.457))
(check-equal #t (< 123/456 41/151))

(check-equal #f (< 0 0))
(check-equal #f (< 123 123))
(check-equal #f (< 12345678901234567890 12345678901234567890))
(check-equal #f (< 123.456 123.456))
(check-equal #f (< 123/456 41/152))

(check-equal #t (< 12345/100 123.456))
(check-equal #t (< 123.45 12345/99))
(check-equal #t (< 1524074060357399025/12346 123456789012345))
(check-equal #t (< 123456789012345 1524074060357399025/12344))

(check-equal #t (< 0 1 2))
(check-equal #t (< 123 124 125))
(check-equal #t (< 12345678901234567890 12345678901234567891 12345678901234567892))
(check-equal #t (< 123.456 123.457 123.458))
(check-equal #t (< 123/456 41/151 41/150))

(check-equal #f (< 0 1 1))
(check-equal #f (< 123 124 123))
(check-equal #f (< 12345678901234567890 12345678901234567891 12345678901234567890))
(check-equal #f (< 123.456 123.457 123.456))
(check-equal #f (< 123/456 41/151 123/456))

(check-error (assertion-violation <) (< 1 "1"))
(check-error (assertion-violation <) (< 1 #\1))
(check-error (assertion-violation <) (< 1 #f))
(check-error (assertion-violation <) (< 1 1+i))
(check-error (assertion-violation <) (< "1" 1))
(check-error (assertion-violation <) (< #\1 1))
(check-error (assertion-violation <) (< #f 1))
(check-error (assertion-violation <) (< 1+i 1))
(check-error (assertion-violation <) (< 1 2 "1"))
(check-error (assertion-violation <) (< 1 2 #\1))
(check-error (assertion-violation <) (< 1 2 #f))
(check-error (assertion-violation <) (< 1 2 1+i))
(check-error (assertion-violation <) (<))
(check-error (assertion-violation <) (< 1))

(check-equal #t (> 1 0))
(check-equal #t (> 124 123))
(check-equal #t (> 12345678901234567891 12345678901234567890))
(check-equal #t (> 123.457 123.456))
(check-equal #t (> 41/151 123/456))

(check-equal #f (> 0 0))
(check-equal #f (> 123 123))
(check-equal #f (> 12345678901234567890 12345678901234567890))
(check-equal #f (> 123.456 123.456))
(check-equal #f (> 123/456 41/152))

(check-equal #t (> 123.456 12345/100))
(check-equal #t (> 12345/99 123.45))
(check-equal #t (> 1524074060357399025/12344 123456789012345))
(check-equal #t (> 123456789012345 1524074060357399025/12346))

(check-equal #t (> 2 1 0))
(check-equal #t (> 125 124 123))
(check-equal #t (> 12345678901234567892 12345678901234567891 12345678901234567890))
(check-equal #t (> 123.458 123.457 123.456))
(check-equal #t (> 41/150 41/151 123/456))

(check-equal #f (> 1 0 0))
(check-equal #f (> 123 124 123))
(check-equal #f (> 12345678901234567890 12345678901234567891 12345678901234567890))
(check-equal #f (> 123.456 123.457 123.456))
(check-equal #f (> 123/456 41/151 123/456))

(check-error (assertion-violation >) (> 1 "1"))
(check-error (assertion-violation >) (> 1 #\1))
(check-error (assertion-violation >) (> 1 #f))
(check-error (assertion-violation >) (> 1 1+i))
(check-error (assertion-violation >) (> "1" 1))
(check-error (assertion-violation >) (> #\1 1))
(check-error (assertion-violation >) (> #f 1))
(check-error (assertion-violation >) (> 1+i 1))
(check-error (assertion-violation >) (> 2 1 "1"))
(check-error (assertion-violation >) (> 2 1 #\1))
(check-error (assertion-violation >) (> 2 1 #f))
(check-error (assertion-violation >) (> 2 1 1+i))
(check-error (assertion-violation >) (>))
(check-error (assertion-violation >) (> 1))

(check-equal #t (<= 0 1))
(check-equal #t (<= 123 124))
(check-equal #t (<= 12345678901234567890 12345678901234567891))
(check-equal #t (<= 123.456 123.457))
(check-equal #t (<= 123/456 41/151))

(check-equal #t (<= 12345/100 123.456))
(check-equal #t (<= 123.45 12345/99))
(check-equal #t (<= 1524074060357399025/12346 123456789012345))
(check-equal #t (<= 123456789012345 1524074060357399025/12344))

(check-equal #t (<= 0 1 2))
(check-equal #t (<= 123 124 125))
(check-equal #t (<= 12345678901234567890 12345678901234567891 12345678901234567892))
(check-equal #t (<= 123.456 123.457 123.458))
(check-equal #t (<= 123/456 41/151 41/150))

(check-equal #f (<= 0 1 0))
(check-equal #f (<= 123 124 123))
(check-equal #f (<= 12345678901234567890 12345678901234567891 12345678901234567890))
(check-equal #f (<= 123.456 123.457 123.456))
(check-equal #f (<= 123/456 41/151 123/456))

(check-equal #t (<= -0.0 0.0))
(check-equal #t (<= 0 0.0))
(check-equal #t (<= 0.0 0))
(check-equal #t (<= 123 123.0))
(check-equal #t (<= 123.0 123))

(check-equal #t (<= 1524074060357399025/12345 123456789012345))
(check-equal #t (<= 123456789012345 1524074060357399025/12345))

(check-error (assertion-violation <=) (<= 1 "1"))
(check-error (assertion-violation <=) (<= 1 #\1))
(check-error (assertion-violation <=) (<= 1 #f))
(check-error (assertion-violation <=) (<= 1 1+i))
(check-error (assertion-violation <=) (<= "1" 1))
(check-error (assertion-violation <=) (<= #\1 1))
(check-error (assertion-violation <=) (<= #f 1))
(check-error (assertion-violation <=) (<= 1+i 1))
(check-error (assertion-violation <=) (<= 1 2 "1"))
(check-error (assertion-violation <=) (<= 1 2 #\1))
(check-error (assertion-violation <=) (<= 1 2 #f))
(check-error (assertion-violation <=) (<= 1 2 1+i))
(check-error (assertion-violation <=) (<=))
(check-error (assertion-violation <=) (<= 1))

(check-equal #t (>= 1 0))
(check-equal #t (>= 124 123))
(check-equal #t (>= 12345678901234567891 12345678901234567890))
(check-equal #t (>= 123.457 123.456))
(check-equal #t (>= 41/151 123/456))

(check-equal #t (>= 0 0))
(check-equal #t (>= 123 123))
(check-equal #t (>= 12345678901234567890 12345678901234567890))
(check-equal #t (>= 123.456 123.456))
(check-equal #t (>= 123/456 41/152))

(check-equal #t (>= 123.456 12345/100))
(check-equal #t (>= 12345/99 123.45))
(check-equal #t (>= 1524074060357399025/12344 123456789012345))
(check-equal #t (>= 123456789012345 1524074060357399025/12346))

(check-equal #t (>= 2 1 0))
(check-equal #t (>= 125 124 123))
(check-equal #t (>= 12345678901234567892 12345678901234567891 12345678901234567890))
(check-equal #t (>= 123.458 123.457 123.456))
(check-equal #t (>= 41/150 41/151 123/456))

(check-equal #f (>= 1 0 1))
(check-equal #f (>= 123 124 123))
(check-equal #f (>= 12345678901234567890 12345678901234567891 12345678901234567890))
(check-equal #f (>= 123.456 123.457 123.456))
(check-equal #f (>= 123/456 41/151 123/456))

(check-error (assertion-violation >=) (>= 1 "1"))
(check-error (assertion-violation >=) (>= 1 #\1))
(check-error (assertion-violation >=) (>= 1 #f))
(check-error (assertion-violation >=) (>= 1 1+i))
(check-error (assertion-violation >=) (>= "1" 1))
(check-error (assertion-violation >=) (>= #\1 1))
(check-error (assertion-violation >=) (>= #f 1))
(check-error (assertion-violation >=) (>= 1+i 1))
(check-error (assertion-violation >=) (>= 2 1 "1"))
(check-error (assertion-violation >=) (>= 2 1 #\1))
(check-error (assertion-violation >=) (>= 2 1 #f))
(check-error (assertion-violation >=) (>= 2 1 1+i))
(check-error (assertion-violation >=) (>=))
(check-error (assertion-violation >=) (>= 1))

(check-equal #t (zero? 0))
(check-equal #t (zero? 0.0))
(check-equal #f (zero? 1))
(check-equal #f (zero? -1))
(check-equal #f (zero? -i))
(check-equal #f (zero? 1/2))

(check-error (assertion-violation zero?) (zero?))
(check-error (assertion-violation zero?) (zero? 1 2))
(check-error (assertion-violation zero?) (zero? #f))
(check-error (assertion-violation zero?) (zero? #\0))
(check-error (assertion-violation zero?) (zero? "0"))

(check-equal #t (positive? 1))
(check-equal #t (positive? 123456678901234567890))
(check-equal #t (positive? 1/2))
(check-equal #t (positive? 12345678901234567890/12345))
(check-equal #t (positive? 1.0001))
(check-equal #t (positive? 0.0000000000001))

(check-equal #f (positive? -1))
(check-equal #f (positive? -123456678901234567890))
(check-equal #f (positive? -1/2))
(check-equal #f (positive? -12345678901234567890/12345))
(check-equal #f (positive? -1.0001))
(check-equal #f (positive? -0.0000000000001))

(check-equal #f (positive? 0))
(check-equal #f (positive? 0.0))

(check-error (assertion-violation positive?) (positive?))
(check-error (assertion-violation positive?) (positive? 1 2))
(check-error (assertion-violation positive?) (positive? +i))
(check-error (assertion-violation positive?) (positive? #\0))
(check-error (assertion-violation positive?) (positive? "0"))

(check-equal #f (negative? 1))
(check-equal #f (negative? 123456678901234567890))
(check-equal #f (negative? 1/2))
(check-equal #f (negative? 12345678901234567890/12345))
(check-equal #f (negative? 1.0001))
(check-equal #f (negative? 0.0000000000001))

(check-equal #t (negative? -1))
(check-equal #t (negative? -123456678901234567890))
(check-equal #t (negative? -1/2))
(check-equal #t (negative? -12345678901234567890/12345))
(check-equal #t (negative? -1.0001))
(check-equal #t (negative? -0.0000000000001))

(check-equal #f (negative? 0))
(check-equal #f (negative? 0.0))

(check-error (assertion-violation negative?) (negative?))
(check-error (assertion-violation negative?) (negative? 1 2))
(check-error (assertion-violation negative?) (negative? +i))
(check-error (assertion-violation negative?) (negative? #\0))
(check-error (assertion-violation negative?) (negative? "0"))

(check-equal #f (odd? 0))
(check-equal #t (odd? 1))
(check-equal #t (odd? -1))
(check-equal #f (odd? 2))
(check-equal #f (odd? -2))
(check-equal #f (odd? 12345678901234567890))
(check-equal #f (odd? -12345678901234567890))
(check-equal #t (odd? 1234567890123456789))
(check-equal #t (odd? -1234567890123456789))

(check-error (assertion-violation odd?) (odd?))
(check-error (assertion-violation odd?) (odd? 1 2))
(check-error (assertion-violation odd?) (odd? 12.3))
(check-error (assertion-violation odd?) (odd? 123/456))
(check-error (assertion-violation odd?) (odd? 12+34i))
(check-error (assertion-violation odd?) (odd? #\0))

(check-equal #t (even? 0))
(check-equal #f (even? 1))
(check-equal #f (even? -1))
(check-equal #t (even? 2))
(check-equal #t (even? -2))
(check-equal #t (even? 12345678901234567890))
(check-equal #t (even? -12345678901234567890))
(check-equal #f (even? 1234567890123456789))
(check-equal #f (even? -1234567890123456789))

(check-error (assertion-violation even?) (even?))
(check-error (assertion-violation even?) (even? 1 2))
(check-error (assertion-violation even?) (even? 12.3))
(check-error (assertion-violation even?) (even? 123/456))
(check-error (assertion-violation even?) (even? 12+34i))
(check-error (assertion-violation even?) (even? #\0))

(check-equal 1 (max 1))
(check-equal 4 (max 1 2 3 4))
(check-equal 4 (max 4 3 2 1))
(check-equal 4.0 (max 1.0 2 3 4))
(check-equal 4.0 (max 4 3 2.0 1))
(check-equal 12345678901234567890 (max 1/2 12345678901234567890))
(check-equal 1 (max 1 -5 -12345678901234567890))
(check-equal 3/4 (max 1/2 3/4 5/8 9/16 17/32))

(check-equal 4 (max 3 4))
(check-equal 4.0 (max 3.9 4))

(check-error (assertion-violation max) (max))
(check-error (assertion-violation max) (max 1 +4i 2))
(check-error (assertion-violation max) (max +3i))
(check-error (assertion-violation max) (max 1 #\0))
(check-error (assertion-violation max) (max #\0 1))

(check-equal 1 (min 1))
(check-equal 1 (min 1 2 3 4))
(check-equal 1 (min 4 3 2 1))
(check-equal 1.0 (min 1 2.0 3 4))
(check-equal 1.0 (min 4 3 2.0 1))
(check-equal 1/2 (min 1/2 12345678901234567890))
(check-equal -12345678901234567890 (min 1 -5 -12345678901234567890))
(check-equal 1/2 (min 1/2 3/4 5/8 9/16 17/32))

(check-equal 3 (min 3 4))
(check-equal 3.0 (min 3 4.9))

(check-error (assertion-violation min) (min))
(check-error (assertion-violation min) (min 1 +4i 2))
(check-error (assertion-violation min) (min +3i))
(check-error (assertion-violation min) (min 1 #\0))
(check-error (assertion-violation min) (min #\0 1))

(check-equal 28630/14803 (+ 109/113 127/131))
(check-equal 72/14803 (+ -109/113 127/131))
(check-equal -72/14803 (+ 109/113 -127/131))
(check-equal -28630/14803 (+ -109/113 -127/131))
(check-equal 1716049367271604936847/139 (+ 137/139 12345678901234567890))
(check-equal 1716049367271604936573/139 (+ -137/139 12345678901234567890))
(check-equal -1716049367271604936573/139 (+ 137/139 -12345678901234567890))
(check-equal -1716049367271604936847/139 (+ -137/139 -12345678901234567890))
(check-equal 23856/151+163i (+ 149/151 157+163i))
(check-equal 23558/151+163i (+ -149/151 157+163i))
(check-equal -23558/151-163i (+ 149/151 -157-163i))
(check-equal -23856/151-163i (+ -149/151 -157-163i))
(check-equal 180.14631791907516-191.193i (+ 167/173 179.181-191.193i))
(check-equal 178.21568208092486-191.193i (+ -167/173 179.181-191.193i))
(check-equal -178.21568208092486+191.193i (+ 167/173 -179.181+191.193i))
(check-equal -180.14631791907516+191.193i (+ -167/173 -179.181+191.193i))
(check-equal 212.21294974874374 (+ 197/199 211.223))
(check-equal 210.2330502512563 (+ -197/199 211.223))
(check-equal -210.2330502512563 (+ 197/199 -211.223))
(check-equal -212.21294974874374 (+ -197/199 -211.223))
(check-equal 53584/229 (+ 227/229 233))
(check-equal 53130/229 (+ -227/229 233))
(check-equal -53130/229 (+ 227/229 -233))
(check-equal -53584/229 (+ -227/229 -233))
(check-equal 67226791603290978665428/281 (+ 239241251257263269271 277/281))
(check-equal -67226791603290978664874/281 (+ -239241251257263269271 277/281))
(check-equal 67226791603290978664874/281 (+ 239241251257263269271 -277/281))
(check-equal -67226791603290978665428/281 (+ -239241251257263269271 -277/281))
(check-equal 620640656664672684704 (+ 283293307311313317331 337347349353359367373))
(check-equal 54054042042046050042 (+ -283293307311313317331 337347349353359367373))
(check-equal -54054042042046050042 (+ 283293307311313317331 -337347349353359367373))
(check-equal -620640656664672684704 (+ -283293307311313317331 -337347349353359367373))
(check-equal 379383389397401409840-431i (+ 379383389397401409419 421-431i))
(check-equal -379383389397401408998-431i (+ -379383389397401409419 421-431i))
(check-equal 379383389397401408998+431i (+ 379383389397401409419 -421+431i))
(check-equal -379383389397401409840+431i (+ -379383389397401409419 -421+431i))
(check-equal 4.334394434494575e20+487.491i (+ 433439443449457461463 467.479+487.491i))
(check-equal -4.334394434494575e20+487.491i (+ -433439443449457461463 467.479+487.491i))
(check-equal 4.334394434494575e20-487.491i (+ 433439443449457461463 -467.479-487.491i))
(check-equal -4.334394434494575e20-487.491i (+ -433439443449457461463 -467.479-487.491i))
(check-equal 4.9950350952152354e20 (+ 499503509521523541547 557.563))
(check-equal -4.9950350952152354e20 (+ -499503509521523541547 557.563))
(check-equal 4.9950350952152354e20 (+ 499503509521523541547 -557.563))
(check-equal -4.9950350952152354e20 (+ -499503509521523541547 -557.563))
(check-equal 5.695715775875936e20 (+ 569571577587593599601 607.613))
(check-equal -5.695715775875936e20 (+ -569571577587593599601 607.613))
(check-equal 5.695715775875936e20 (+ 569571577587593599601 -607.613))
(check-equal -5.695715775875936e20 (+ -569571577587593599601 -607.613))
(check-equal 396128/641+619i (+ 617+619i 631/641))
(check-equal -394866/641-619i (+ -617-619i 631/641))
(check-equal 394866/641+619i (+ 617+619i -631/641))
(check-equal -396128/641-619i (+ -617-619i -631/641))
(check-equal 12345678901234568533-647i (+ 643-647i 12345678901234567890))
(check-equal 12345678901234567247+647i (+ -643+647i 12345678901234567890))
(check-equal -12345678901234567247-647i (+ 643-647i -12345678901234567890))
(check-equal -12345678901234568533+647i (+ -643+647i -12345678901234567890))
(check-equal 1314-14i (+ 653+659i 661-673i))
(check-equal 8-1332i (+ -653-659i 661-673i))
(check-equal -8+1332i (+ 653+659i -661+673i))
(check-equal -1314+14i (+ -653-659i -661+673i))
(check-equal 1368.701+26.71900000000005i (+ 677-683i 691.701+709.719i))
(check-equal 14.701000000000022+1392.719i (+ -677+683i 691.701+709.719i))
(check-equal -14.701000000000022-1392.719i (+ 677-683i -691.701-709.719i))
(check-equal -1368.701-26.71900000000005i (+ -677+683i -691.701-709.719i))
(check-equal 1466.743+733.0i (+ 727+733i 739.743))
(check-equal 12.743000000000052-733.0i (+ -727-733i 739.743))
(check-equal -12.743000000000052+733.0i (+ 727+733i -739.743))
(check-equal -1466.743-733.0i (+ -727-733i -739.743))
(check-equal 1512-757i (+ 751-757i 761))
(check-equal 10+757i (+ -751+757i 761))
(check-equal -10-757i (+ 751-757i -761))
(check-equal -1512+757i (+ -751+757i -761))
(check-equal 770.7705339087546+787.797i (+ 769.773+787.797i 809/811))
(check-equal -768.7754660912454-787.797i (+ -769.773-787.797i 809/811))
(check-equal 768.7754660912454+787.797i (+ 769.773+787.797i -809/811))
(check-equal -770.7705339087546-787.797i (+ -769.773-787.797i -809/811))
(check-equal 8.398538578598638e20-827.829i (+ 821.823-827.829i 839853857859863877881))
(check-equal 8.398538578598638e20+827.829i (+ -821.823+827.829i 839853857859863877881))
(check-equal -8.398538578598638e20-827.829i (+ 821.823-827.829i -839853857859863877881))
(check-equal -8.398538578598638e20+827.829i (+ -821.823+827.829i -839853857859863877881))
(check-equal 1802.887-21.089000000000055i (+ 883.887+907.911i 919-929i))
(check-equal 35.113000000000056-1836.911i (+ -883.887-907.911i 919-929i))
(check-equal -35.113000000000056+1836.911i (+ 883.887+907.911i -919+929i))
(check-equal -1802.887+21.089000000000055i (+ -883.887-907.911i -919+929i))
(check-equal 1905.912+30.029999999999973i (+ 937.941-947.953i 967.971+977.983i))
(check-equal 30.029999999999973+1925.936i (+ -937.941+947.953i 967.971+977.983i))
(check-equal -30.029999999999973-1925.936i (+ 937.941-947.953i -967.971-977.983i))
(check-equal -1905.912-30.029999999999973i (+ -937.941+947.953i -967.971-977.983i))
(check-equal 1119.128+109.113i (+ 991.997+109.113i 127.131))
(check-equal -864.866-109.113i (+ -991.997-109.113i 127.131))
(check-equal 864.866+109.113i (+ 991.997+109.113i -127.131))
(check-equal -1119.128-109.113i (+ -991.997-109.113i -127.131))
(check-equal 294.139-149.151i (+ 137.139-149.151i 157))
(check-equal 19.86099999999999+149.151i (+ -137.139+149.151i 157))
(check-equal -19.86099999999999-149.151i (+ 137.139-149.151i -157))
(check-equal -294.139+149.151i (+ -137.139+149.151i -157))
(check-equal 164.13348044692736 (+ 163.167 173/179))
(check-equal -162.20051955307264 (+ -163.167 173/179))
(check-equal 162.20051955307264 (+ 163.167 -173/179))
(check-equal -164.13348044692736 (+ -163.167 -173/179))
(check-equal 1.9319719921122322e20 (+ 181.191 193197199211223227229))
(check-equal 1.9319719921122322e20 (+ -181.191 193197199211223227229))
(check-equal -1.9319719921122322e20 (+ 181.191 -193197199211223227229))
(check-equal -1.9319719921122322e20 (+ -181.191 -193197199211223227229))
(check-equal 241.233239+251.0i (+ 0.233239 241+251i))
(check-equal 240.766761+251.0i (+ -0.233239 241+251i))
(check-equal -240.766761-251.0i (+ 0.233239 -241-251i))
(check-equal -241.233239-251.0i (+ -0.233239 -241-251i))
(check-equal 257263.269+271.0i (+ 257263.0 0.269+271.0i))
(check-equal -257262.731+271.0i (+ -257263.0 0.269+271.0i))
(check-equal 257262.731-271.0i (+ 257263.0 -0.269-271.0i))
(check-equal -257263.269-271.0i (+ -257263.0 -0.269-271.0i))
(check-equal 283570.588 (+ 277.281 283293.307))
(check-equal 283016.02599999995 (+ -277.281 283293.307))
(check-equal -283016.02599999995 (+ 277.281 -283293.307))
(check-equal -283570.588 (+ -277.281 -283293.307))
(check-equal 628.313 (+ 311.313 317))
(check-equal 5.687000000000012 (+ -311.313 317))
(check-equal -5.687000000000012 (+ 311.313 -317))
(check-equal -628.313 (+ -311.313 -317))
(check-equal 115194/347 (+ 331 337/347))
(check-equal -114520/347 (+ -331 337/347))
(check-equal 114520/347 (+ 331 -337/347))
(check-equal -115194/347 (+ -331 -337/347))
(check-equal 353359367373379383738 (+ 349 353359367373379383389))
(check-equal 353359367373379383040 (+ -349 353359367373379383389))
(check-equal -353359367373379383040 (+ 349 -353359367373379383389))
(check-equal -353359367373379383738 (+ -349 -353359367373379383389))
(check-equal 798+409i (+ 397 401+409i))
(check-equal 4+409i (+ -397 401+409i))
(check-equal -4-409i (+ 397 -401-409i))
(check-equal -798-409i (+ -397 -401-409i))
(check-equal 840.431-433.439i (+ 419 421.431-433.439i))
(check-equal 2.4309999999999-433.439i (+ -419 421.431-433.439i))
(check-equal -2.4309999999999+433.439i (+ 419 -421.431+433.439i))
(check-equal -840.431+433.439i (+ -419 -421.431+433.439i))
(check-equal 892.457 (+ 443 449.457))
(check-equal 6.456999999999994 (+ -443 449.457))
(check-equal -6.456999999999994 (+ 443 -449.457))
(check-equal -892.457 (+ -443 -449.457))
(check-equal 924 (+ 461 463))
(check-equal 2 (+ -461 463))
(check-equal -2 (+ 461 -463))
(check-equal -924 (+ -461 -463))

(check-equal 350032021/118300067 (+ 467/479 487/491 499/503))
(check-equal 101/12 (+ 3/2 5/4 17/3))

(check-equal 0 (+))

(check-error (assertion-violation +) (+ #\0))
(check-error (assertion-violation +) (+ #f))
(check-error (assertion-violation +) (+ "0"))
(check-error (assertion-violation +) (+ 0 #\0))
(check-error (assertion-violation +) (+ 0 #f))
(check-error (assertion-violation +) (+ 0 "0"))

(check-equal 13843/14803 (* 109/113 127/131))
(check-equal -13843/14803 (* -109/113 127/131))
(check-equal -13843/14803 (* 109/113 -127/131))
(check-equal 13843/14803 (* -109/113 -127/131))
(check-equal 1691358009469135800930/139 (* 137/139 12345678901234567890))
(check-equal -1691358009469135800930/139 (* -137/139 12345678901234567890))
(check-equal -1691358009469135800930/139 (* 137/139 -12345678901234567890))
(check-equal 1691358009469135800930/139 (* -137/139 -12345678901234567890))
(check-equal 23393/151+24287/151i (* 149/151 157+163i))
(check-equal -23393/151-24287/151i (* -149/151 157+163i))
(check-equal -23393/151-24287/151i (* 149/151 -157-163i))
(check-equal 23393/151+24287/151i (* -149/151 -157-163i))
(check-equal 172.96663005780348-184.56202890173412i (* 167/173 179.181-191.193i))
(check-equal -172.96663005780348+184.56202890173412i (* -167/173 179.181-191.193i))
(check-equal -172.96663005780348+184.56202890173412i (* 167/173 -179.181+191.193i))
(check-equal 172.96663005780348-184.56202890173412i (* -167/173 -179.181+191.193i))
(check-equal 209.10015577889448 (* 197/199 211.223))
(check-equal -209.10015577889448 (* -197/199 211.223))
(check-equal -209.10015577889448 (* 197/199 -211.223))
(check-equal 209.10015577889448 (* -197/199 -211.223))
(check-equal 52891/229 (* 227/229 233))
(check-equal -52891/229 (* -227/229 233))
(check-equal -52891/229 (* 227/229 -233))
(check-equal 52891/229 (* -227/229 -233))
(check-equal 66269826598261925588067/281 (* 239241251257263269271 277/281))
(check-equal -66269826598261925588067/281 (* -239241251257263269271 277/281))
(check-equal -66269826598261925588067/281 (* 239241251257263269271 -277/281))
(check-equal 66269826598261925588067/281 (* -239241251257263269271 -277/281))
(check-equal 95568246311018209162539154896872156841463 (* 283293307311313317331 337347349353359367373))
(check-equal -95568246311018209162539154896872156841463 (* -283293307311313317331 337347349353359367373))
(check-equal -95568246311018209162539154896872156841463 (* 283293307311313317331 -337347349353359367373))
(check-equal 95568246311018209162539154896872156841463 (* -283293307311313317331 -337347349353359367373))
(check-equal 159720406936305993365399-163514240830280007459589i (* 379383389397401409419 421-431i))
(check-equal -159720406936305993365399+163514240830280007459589i (* -379383389397401409419 421-431i))
(check-equal -159720406936305993365399+163514240830280007459589i (* 379383389397401409419 -421+431i))
(check-equal 159720406936305993365399-163514240830280007459589i (* -379383389397401409419 -421+431i))
(check-equal 2.0262383758430893e23+2.1129782772661948e23i (* 433439443449457461463 467.479+487.491i))
(check-equal -2.0262383758430893e23-2.1129782772661948e23i (* -433439443449457461463 467.479+487.491i))
(check-equal -2.0262383758430893e23-2.1129782772661948e23i (* 433439443449457461463 -467.479-487.491i))
(check-equal 2.0262383758430893e23+2.1129782772661948e23i (* -433439443449457461463 -467.479-487.491i))
(check-equal 2.7850467527934924e23 (* 499503509521523541547 557.563))
(check-equal -2.7850467527934924e23 (* -499503509521523541547 557.563))
(check-equal -2.7850467527934924e23 (* 499503509521523541547 -557.563))
(check-equal 2.7850467527934924e23 (* -499503509521523541547 -557.563))
(check-equal 3.4607909497273055e23 (* 569571577587593599601 607.613))
(check-equal -3.4607909497273055e23 (* -569571577587593599601 607.613))
(check-equal -3.4607909497273055e23 (* 569571577587593599601 -607.613))
(check-equal 3.4607909497273055e23 (* -569571577587593599601 -607.613))
(check-equal 389327/641+390589/641i (* 617+619i 631/641))
(check-equal -389327/641-390589/641i (* -617-619i 631/641))
(check-equal -389327/641-390589/641i (* 617+619i -631/641))
(check-equal 389327/641+390589/641i (* -617-619i -631/641))
(check-equal 7938271533493827153270-7987654249098765424830i (* 643-647i 12345678901234567890))
(check-equal -7938271533493827153270+7987654249098765424830i (* -643+647i 12345678901234567890))
(check-equal -7938271533493827153270+7987654249098765424830i (* 643-647i -12345678901234567890))
(check-equal 7938271533493827153270-7987654249098765424830i (* -643+647i -12345678901234567890))
(check-equal 875140-3870i (* 653+659i 661-673i))
(check-equal -875140+3870i (* -653-659i 661-673i))
(check-equal -875140+3870i (* 653+659i -661+673i))
(check-equal 875140-3870i (* -653-659i -661+673i))
(check-equal 953019.6540000001+8047.98000000004i (* 677-683i 691.701+709.719i))
(check-equal -953019.6540000001-8047.98000000004i (* -677+683i 691.701+709.719i))
(check-equal -953019.6540000001-8047.98000000004i (* 677-683i -691.701-709.719i))
(check-equal 953019.6540000001+8047.98000000004i (* -677+683i -691.701-709.719i))
(check-equal 537793.1610000001+542231.6190000001i (* 727+733i 739.743))
(check-equal -537793.1610000001-542231.6190000001i (* -727-733i 739.743))
(check-equal -537793.1610000001-542231.6190000001i (* 727+733i -739.743))
(check-equal 537793.1610000001+542231.6190000001i (* -727-733i -739.743))
(check-equal 571511-576077i (* 751-757i 761))
(check-equal -571511+576077i (* -751+757i 761))
(check-equal -571511+576077i (* 751-757i -761))
(check-equal 571511-576077i (* -751+757i -761))
(check-equal 767.8746695437732+785.8542207151664i (* 769.773+787.797i 809/811))
(check-equal -767.8746695437732-785.8542207151664i (* -769.773-787.797i 809/811))
(check-equal -767.8746695437732-785.8542207151664i (* 769.773+787.797i -809/811))
(check-equal 767.8746695437732+785.8542207151664i (* -769.773-787.797i -809/811))
(check-equal 6.902112170279668e23-6.952553792982732e23i (* 821.823-827.829i 839853857859863877881))
(check-equal -6.902112170279668e23+6.952553792982732e23i (* -821.823+827.829i 839853857859863877881))
(check-equal -6.902112170279668e23+6.952553792982732e23i (* 821.823-827.829i -839853857859863877881))
(check-equal 6.902112170279668e23-6.952553792982732e23i (* -821.823+827.829i -839853857859863877881))
(check-equal 1655741.4719999998+13239.185999999987i (* 883.887+907.911i 919-929i))
(check-equal -1655741.4719999998-13239.185999999987i (* -883.887-907.911i 919-929i))
(check-equal -1655741.4719999998-13239.185999999987i (* 883.887+907.911i -919+929i))
(check-equal 1655741.4719999998+13239.185999999987i (* -883.887-907.911i -919+929i))
(check-equal 1834981.6065099998-300.66035999997985i (* 937.941-947.953i 967.971+977.983i))
(check-equal -1834981.6065099998+300.66035999997985i (* -937.941+947.953i 967.971+977.983i))
(check-equal -1834981.6065099998+300.66035999997985i (* 937.941-947.953i -967.971-977.983i))
(check-equal 1834981.6065099998-300.66035999997985i (* -937.941+947.953i -967.971-977.983i))
(check-equal 126113.570607+13871.644803i (* 991.997+109.113i 127.131))
(check-equal -126113.570607-13871.644803i (* -991.997-109.113i 127.131))
(check-equal -126113.570607-13871.644803i (* 991.997+109.113i -127.131))
(check-equal 126113.570607+13871.644803i (* -991.997-109.113i -127.131))
(check-equal 21530.823-23416.707000000002i (* 137.139-149.151i 157))
(check-equal -21530.823+23416.707000000002i (* -137.139+149.151i 157))
(check-equal -21530.823+23416.707000000002i (* 137.139-149.151i -157))
(check-equal 21530.823-23416.707000000002i (* -137.139+149.151i -157))
(check-equal 157.6977150837989 (* 163.167 173/179))
(check-equal -157.6977150837989 (* -163.167 173/179))
(check-equal -157.6977150837989 (* 163.167 -173/179))
(check-equal 157.6977150837989 (* -163.167 -173/179))
(check-equal 3.5005593722280747e22 (* 181.191 193197199211223227229))
(check-equal -3.5005593722280747e22 (* -181.191 193197199211223227229))
(check-equal -3.5005593722280747e22 (* 181.191 -193197199211223227229))
(check-equal 3.5005593722280747e22 (* -181.191 -193197199211223227229))
(check-equal 56.210599+58.542989i (* 0.233239 241+251i))
(check-equal -56.210599-58.542989i (* -0.233239 241+251i))
(check-equal -56.210599-58.542989i (* 0.233239 -241-251i))
(check-equal 56.210599+58.542989i (* -0.233239 -241-251i))
(check-equal 69203.747+69718273.0i (* 257263.0 0.269+271.0i))
(check-equal -69203.747-69718273.0i (* -257263.0 0.269+271.0i))
(check-equal -69203.747-69718273.0i (* 257263.0 -0.269-271.0i))
(check-equal 69203.747+69718273.0i (* -257263.0 -0.269-271.0i))
(check-equal 78551851.45826699 (* 277.281 283293.307))
(check-equal -78551851.45826699 (* -277.281 283293.307))
(check-equal -78551851.45826699 (* 277.281 -283293.307))
(check-equal 78551851.45826699 (* -277.281 -283293.307))
(check-equal 98686.22099999999 (* 311.313 317))
(check-equal -98686.22099999999 (* -311.313 317))
(check-equal -98686.22099999999 (* 311.313 -317))
(check-equal 98686.22099999999 (* -311.313 -317))
(check-equal 111547/347 (* 331 337/347))
(check-equal -111547/347 (* -331 337/347))
(check-equal -111547/347 (* 331 -337/347))
(check-equal 111547/347 (* -331 -337/347))
(check-equal 123322419213309404802761 (* 349 353359367373379383389))
(check-equal -123322419213309404802761 (* -349 353359367373379383389))
(check-equal -123322419213309404802761 (* 349 -353359367373379383389))
(check-equal 123322419213309404802761 (* -349 -353359367373379383389))
(check-equal 159197+162373i (* 397 401+409i))
(check-equal -159197-162373i (* -397 401+409i))
(check-equal -159197-162373i (* 397 -401-409i))
(check-equal 159197+162373i (* -397 -401-409i))
(check-equal 176579.589-181610.94100000002i (* 419 421.431-433.439i))
(check-equal -176579.589+181610.94100000002i (* -419 421.431-433.439i))
(check-equal -176579.589+181610.94100000002i (* 419 -421.431+433.439i))
(check-equal 176579.589-181610.94100000002i (* -419 -421.431+433.439i))
(check-equal 199109.451 (* 443 449.457))
(check-equal -199109.451 (* -443 449.457))
(check-equal -199109.451 (* 443 -449.457))
(check-equal 199109.451 (* -443 -449.457))
(check-equal 213443 (* 461 463))
(check-equal -213443 (* -461 463))
(check-equal -213443 (* 461 -463))
(check-equal 213443 (* -461 -463))
(check-equal 113487071/118300067 (* 467/479 487/491 499/503))
(check-equal 85/8 (* 3/2 5/4 17/3))

(check-equal 1 (*))

(check-error (assertion-violation *) (* #\0))
(check-error (assertion-violation *) (* #f))
(check-error (assertion-violation *) (* "0"))
(check-error (assertion-violation *) (* 0 #\0))
(check-error (assertion-violation *) (* 0 #f))
(check-error (assertion-violation *) (* 0 "0"))

(check-equal -72/14803 (- 109/113 127/131))
(check-equal -28630/14803 (- -109/113 127/131))
(check-equal 28630/14803 (- 109/113 -127/131))
(check-equal 72/14803 (- -109/113 -127/131))
(check-equal -1716049367271604936573/139 (- 137/139 12345678901234567890))
(check-equal -1716049367271604936847/139 (- -137/139 12345678901234567890))
(check-equal 1716049367271604936847/139 (- 137/139 -12345678901234567890))
(check-equal 1716049367271604936573/139 (- -137/139 -12345678901234567890))
(check-equal -23558/151-163i (- 149/151 157+163i))
(check-equal -23856/151-163i (- -149/151 157+163i))
(check-equal 23856/151+163i (- 149/151 -157-163i))
(check-equal 23558/151+163i (- -149/151 -157-163i))
(check-equal -178.21568208092486+191.193i (- 167/173 179.181-191.193i))
(check-equal -180.14631791907516+191.193i (- -167/173 179.181-191.193i))
(check-equal 180.14631791907516-191.193i (- 167/173 -179.181+191.193i))
(check-equal 178.21568208092486-191.193i (- -167/173 -179.181+191.193i))
(check-equal -210.2330502512563 (- 197/199 211.223))
(check-equal -212.21294974874374 (- -197/199 211.223))
(check-equal 212.21294974874374 (- 197/199 -211.223))
(check-equal 210.2330502512563 (- -197/199 -211.223))
(check-equal -53130/229 (- 227/229 233))
(check-equal -53584/229 (- -227/229 233))
(check-equal 53584/229 (- 227/229 -233))
(check-equal 53130/229 (- -227/229 -233))
(check-equal 67226791603290978664874/281 (- 239241251257263269271 277/281))
(check-equal -67226791603290978665428/281 (- -239241251257263269271 277/281))
(check-equal 67226791603290978665428/281 (- 239241251257263269271 -277/281))
(check-equal -67226791603290978664874/281 (- -239241251257263269271 -277/281))
(check-equal -54054042042046050042 (- 283293307311313317331 337347349353359367373))
(check-equal -620640656664672684704 (- -283293307311313317331 337347349353359367373))
(check-equal 620640656664672684704 (- 283293307311313317331 -337347349353359367373))
(check-equal 54054042042046050042 (- -283293307311313317331 -337347349353359367373))
(check-equal 379383389397401408998+431i (- 379383389397401409419 421-431i))
(check-equal -379383389397401409840+431i (- -379383389397401409419 421-431i))
(check-equal 379383389397401409840-431i (- 379383389397401409419 -421+431i))
(check-equal -379383389397401408998-431i (- -379383389397401409419 -421+431i))
(check-equal 4.334394434494575e20-487.491i (- 433439443449457461463 467.479+487.491i))
(check-equal -4.334394434494575e20-487.491i (- -433439443449457461463 467.479+487.491i))
(check-equal 4.334394434494575e20+487.491i (- 433439443449457461463 -467.479-487.491i))
(check-equal -4.334394434494575e20+487.491i (- -433439443449457461463 -467.479-487.491i))
(check-equal 4.9950350952152354e20 (- 499503509521523541547 557.563))
(check-equal -4.9950350952152354e20 (- -499503509521523541547 557.563))
(check-equal 4.9950350952152354e20 (- 499503509521523541547 -557.563))
(check-equal -4.9950350952152354e20 (- -499503509521523541547 -557.563))
(check-equal 5.695715775875936e20 (- 569571577587593599601 607.613))
(check-equal -5.695715775875936e20 (- -569571577587593599601 607.613))
(check-equal 5.695715775875936e20 (- 569571577587593599601 -607.613))
(check-equal -5.695715775875936e20 (- -569571577587593599601 -607.613))
(check-equal 394866/641+619i (- 617+619i 631/641))
(check-equal -396128/641-619i (- -617-619i 631/641))
(check-equal 396128/641+619i (- 617+619i -631/641))
(check-equal -394866/641-619i (- -617-619i -631/641))
(check-equal -12345678901234567247-647i (- 643-647i 12345678901234567890))
(check-equal -12345678901234568533+647i (- -643+647i 12345678901234567890))
(check-equal 12345678901234568533-647i (- 643-647i -12345678901234567890))
(check-equal 12345678901234567247+647i (- -643+647i -12345678901234567890))
(check-equal -8+1332i (- 653+659i 661-673i))
(check-equal -1314+14i (- -653-659i 661-673i))
(check-equal 1314-14i (- 653+659i -661+673i))
(check-equal 8-1332i (- -653-659i -661+673i))
(check-equal -14.701000000000022-1392.719i (- 677-683i 691.701+709.719i))
(check-equal -1368.701-26.71900000000005i (- -677+683i 691.701+709.719i))
(check-equal 1368.701+26.71900000000005i (- 677-683i -691.701-709.719i))
(check-equal 14.701000000000022+1392.719i (- -677+683i -691.701-709.719i))
(check-equal -12.743000000000052+733.0i (- 727+733i 739.743))
(check-equal -1466.743-733.0i (- -727-733i 739.743))
(check-equal 1466.743+733.0i (- 727+733i -739.743))
(check-equal 12.743000000000052-733.0i (- -727-733i -739.743))
(check-equal -10-757i (- 751-757i 761))
(check-equal -1512+757i (- -751+757i 761))
(check-equal 1512-757i (- 751-757i -761))
(check-equal 10+757i (- -751+757i -761))
(check-equal 768.7754660912454+787.797i (- 769.773+787.797i 809/811))
(check-equal -770.7705339087546-787.797i (- -769.773-787.797i 809/811))
(check-equal 770.7705339087546+787.797i (- 769.773+787.797i -809/811))
(check-equal -768.7754660912454-787.797i (- -769.773-787.797i -809/811))
(check-equal -8.398538578598638e20-827.829i (- 821.823-827.829i 839853857859863877881))
(check-equal -8.398538578598638e20+827.829i (- -821.823+827.829i 839853857859863877881))
(check-equal 8.398538578598638e20-827.829i (- 821.823-827.829i -839853857859863877881))
(check-equal 8.398538578598638e20+827.829i (- -821.823+827.829i -839853857859863877881))
(check-equal -35.113000000000056+1836.911i (- 883.887+907.911i 919-929i))
(check-equal -1802.887+21.089000000000055i (- -883.887-907.911i 919-929i))
(check-equal 1802.887-21.089000000000055i (- 883.887+907.911i -919+929i))
(check-equal 35.113000000000056-1836.911i (- -883.887-907.911i -919+929i))
(check-equal -30.029999999999973-1925.936i (- 937.941-947.953i 967.971+977.983i))
(check-equal -1905.912-30.029999999999973i (- -937.941+947.953i 967.971+977.983i))
(check-equal 1905.912+30.029999999999973i (- 937.941-947.953i -967.971-977.983i))
(check-equal 30.029999999999973+1925.936i (- -937.941+947.953i -967.971-977.983i))
(check-equal 864.866+109.113i (- 991.997+109.113i 127.131))
(check-equal -1119.128-109.113i (- -991.997-109.113i 127.131))
(check-equal 1119.128+109.113i (- 991.997+109.113i -127.131))
(check-equal -864.866-109.113i (- -991.997-109.113i -127.131))
(check-equal -19.86099999999999-149.151i (- 137.139-149.151i 157))
(check-equal -294.139+149.151i (- -137.139+149.151i 157))
(check-equal 294.139-149.151i (- 137.139-149.151i -157))
(check-equal 19.86099999999999+149.151i (- -137.139+149.151i -157))
(check-equal 162.20051955307264 (- 163.167 173/179))
(check-equal -164.13348044692736 (- -163.167 173/179))
(check-equal 164.13348044692736 (- 163.167 -173/179))
(check-equal -162.20051955307264 (- -163.167 -173/179))
(check-equal -1.9319719921122322e20 (- 181.191 193197199211223227229))
(check-equal -1.9319719921122322e20 (- -181.191 193197199211223227229))
(check-equal 1.9319719921122322e20 (- 181.191 -193197199211223227229))
(check-equal 1.9319719921122322e20 (- -181.191 -193197199211223227229))
(check-equal -240.766761-251.0i (- 0.233239 241+251i))
(check-equal -241.233239-251.0i (- -0.233239 241+251i))
(check-equal 241.233239+251.0i (- 0.233239 -241-251i))
(check-equal 240.766761+251.0i (- -0.233239 -241-251i))
(check-equal 257262.731-271.0i (- 257263.0 0.269+271.0i))
(check-equal -257263.269-271.0i (- -257263.0 0.269+271.0i))
(check-equal 257263.269+271.0i (- 257263.0 -0.269-271.0i))
(check-equal -257262.731+271.0i (- -257263.0 -0.269-271.0i))
(check-equal -283016.02599999995 (- 277.281 283293.307))
(check-equal -283570.588 (- -277.281 283293.307))
(check-equal 283570.588 (- 277.281 -283293.307))
(check-equal 283016.02599999995 (- -277.281 -283293.307))
(check-equal -5.687000000000012 (- 311.313 317))
(check-equal -628.313 (- -311.313 317))
(check-equal 628.313 (- 311.313 -317))
(check-equal 5.687000000000012 (- -311.313 -317))
(check-equal 114520/347 (- 331 337/347))
(check-equal -115194/347 (- -331 337/347))
(check-equal 115194/347 (- 331 -337/347))
(check-equal -114520/347 (- -331 -337/347))
(check-equal -353359367373379383040 (- 349 353359367373379383389))
(check-equal -353359367373379383738 (- -349 353359367373379383389))
(check-equal 353359367373379383738 (- 349 -353359367373379383389))
(check-equal 353359367373379383040 (- -349 -353359367373379383389))
(check-equal -4-409i (- 397 401+409i))
(check-equal -798-409i (- -397 401+409i))
(check-equal 798+409i (- 397 -401-409i))
(check-equal 4+409i (- -397 -401-409i))
(check-equal -2.4309999999999+433.439i (- 419 421.431-433.439i))
(check-equal -840.431+433.439i (- -419 421.431-433.439i))
(check-equal 840.431-433.439i (- 419 -421.431+433.439i))
(check-equal 2.4309999999999-433.439i (- -419 -421.431+433.439i))
(check-equal -6.456999999999994 (- 443 449.457))
(check-equal -892.457 (- -443 449.457))
(check-equal 892.457 (- 443 -449.457))
(check-equal 6.456999999999994 (- -443 -449.457))
(check-equal -2 (- 461 463))
(check-equal -924 (- -461 463))
(check-equal 924 (- 461 -463))
(check-equal 2 (- -461 -463))
(check-equal -119359239/118300067 (- 467/479 487/491 499/503))
(check-equal -65/12 (- 3/2 5/4 17/3))
(check-equal -509/521 (- 509/521))
(check-equal 523/541 (- -523/541))
(check-equal -547557563569571577587 (- 547557563569571577587))
(check-equal 593599601607613617619 (- -593599601607613617619))
(check-equal -631-641i (- 631+641i))
(check-equal -643+647i (- 643-647i))
(check-equal 653-659i (- -653+659i))
(check-equal 661+673i (- -661-673i))
(check-equal -677.683 (- 677.683))
(check-equal 691.701 (- -691.701))
(check-equal -709 (- 709))
(check-equal 719 (- -719))

(check-error (assertion-violation -) (-))
(check-error (assertion-violation -) (- #\0))
(check-error (assertion-violation -) (- #f))
(check-error (assertion-violation -) (- "0"))
(check-error (assertion-violation -) (- 0 #\0))
(check-error (assertion-violation -) (- 0 #f))
(check-error (assertion-violation -) (- 0 "0"))

(check-equal 14279/14351 (/ 109/113 127/131))
(check-equal -14279/14351 (/ -109/113 127/131))
(check-equal -14279/14351 (/ 109/113 -127/131))
(check-equal 14279/14351 (/ -109/113 -127/131))
(check-equal 137/1716049367271604936710 (/ 137/139 12345678901234567890))
(check-equal -137/1716049367271604936710 (/ -137/139 12345678901234567890))
(check-equal -137/1716049367271604936710 (/ 137/139 -12345678901234567890))
(check-equal 137/1716049367271604936710 (/ -137/139 -12345678901234567890))
(check-equal 23393/7733918-24287/7733918i (/ 149/151 157+163i))
(check-equal -23393/7733918+24287/7733918i (/ -149/151 157+163i))
(check-equal -23393/7733918+24287/7733918i (/ 149/151 -157-163i))
(check-equal 23393/7733918-24287/7733918i (/ -149/151 -157-163i))
(check-equal 0.002519154291508342+0.002688034258411073i (/ 167/173 179.181-191.193i))
(check-equal -0.002519154291508342-0.002688034258411073i (/ -167/173 179.181-191.193i))
(check-equal -0.002519154291508342-0.002688034258411073i (/ 167/173 -179.181+191.193i))
(check-equal 0.002519154291508342+0.002688034258411073i (/ -167/173 -179.181+191.193i))
(check-equal 0.004686751673556945 (/ 197/199 211.223))
(check-equal -0.004686751673556945 (/ -197/199 211.223))
(check-equal -0.004686751673556945 (/ 197/199 -211.223))
(check-equal 0.004686751673556945 (/ -197/199 -211.223))
(check-equal 227/53357 (/ 227/229 233))
(check-equal -227/53357 (/ -227/229 233))
(check-equal -227/53357 (/ 227/229 -233))
(check-equal 227/53357 (/ -227/229 -233))
(check-equal 67226791603290978665151/277 (/ 239241251257263269271 277/281))
(check-equal -67226791603290978665151/277 (/ -239241251257263269271 277/281))
(check-equal -67226791603290978665151/277 (/ 239241251257263269271 -277/281))
(check-equal 67226791603290978665151/277 (/ -239241251257263269271 -277/281))
(check-equal 283293307311313317331/337347349353359367373 (/ 283293307311313317331 337347349353359367373))
(check-equal -283293307311313317331/337347349353359367373 (/ -283293307311313317331 337347349353359367373))
(check-equal -283293307311313317331/337347349353359367373 (/ 283293307311313317331 -337347349353359367373))
(check-equal 283293307311313317331/337347349353359367373 (/ -283293307311313317331 -337347349353359367373))
(check-equal 159720406936305993365399/363002+163514240830280007459589/363002i (/ 379383389397401409419 421-431i))
(check-equal -159720406936305993365399/363002-163514240830280007459589/363002i (/ -379383389397401409419 421-431i))
(check-equal -159720406936305993365399/363002-163514240830280007459589/363002i (/ 379383389397401409419 -421+431i))
(check-equal 159720406936305993365399/363002+163514240830280007459589/363002i (/ -379383389397401409419 -421+431i))
(check-equal 4.441712058665868e17-4.631854378894202e17i (/ 433439443449457461463 467.479+487.491i))
(check-equal -4.441712058665868e17+4.631854378894202e17i (/ -433439443449457461463 467.479+487.491i))
(check-equal -4.441712058665868e17+4.631854378894202e17i (/ 433439443449457461463 -467.479-487.491i))
(check-equal 4.441712058665868e17-4.631854378894202e17i (/ -433439443449457461463 -467.479-487.491i))
(check-equal 8.958691834313316e17 (/ 499503509521523541547 557.563))
(check-equal -8.958691834313316e17 (/ -499503509521523541547 557.563))
(check-equal -8.958691834313316e17 (/ 499503509521523541547 -557.563))
(check-equal 8.958691834313316e17 (/ -499503509521523541547 -557.563))
(check-equal 9.373920202293129e17 (/ 569571577587593599601 607.613))
(check-equal -9.373920202293129e17 (/ -569571577587593599601 607.613))
(check-equal -9.373920202293129e17 (/ 569571577587593599601 -607.613))
(check-equal 9.373920202293129e17 (/ -569571577587593599601 -607.613))
(check-equal 395497/631+396779/631i (/ 617+619i 631/641))
(check-equal -395497/631-396779/631i (/ -617-619i 631/641))
(check-equal -395497/631-396779/631i (/ 617+619i -631/641))
(check-equal 395497/631+396779/631i (/ -617-619i -631/641))
(check-equal 643/12345678901234567890-647/12345678901234567890i (/ 643-647i 12345678901234567890))
(check-equal -643/12345678901234567890+647/12345678901234567890i (/ -643+647i 12345678901234567890))
(check-equal -643/12345678901234567890+647/12345678901234567890i (/ 643-647i -12345678901234567890))
(check-equal 643/12345678901234567890-647/12345678901234567890i (/ -643+647i -12345678901234567890))
(check-equal -5937/444925+437534/444925i (/ 653+659i 661-673i))
(check-equal 5937/444925-437534/444925i (/ -653-659i 661-673i))
(check-equal 5937/444925-437534/444925i (/ 653+659i -661+673i))
(check-equal -5937/444925+437534/444925i (/ -653-659i -661+673i))
(check-equal -0.016755564501881204-0.9702288380633964i (/ 677-683i 691.701+709.719i))
(check-equal 0.016755564501881204+0.9702288380633964i (/ -677+683i 691.701+709.719i))
(check-equal 0.016755564501881204+0.9702288380633964i (/ 677-683i -691.701-709.719i))
(check-equal -0.016755564501881204-0.9702288380633964i (/ -677+683i -691.701-709.719i))
(check-equal 0.9827737470986545+0.9908846721090973i (/ 727+733i 739.743))
(check-equal -0.9827737470986545-0.9908846721090973i (/ -727-733i 739.743))
(check-equal -0.9827737470986545-0.9908846721090973i (/ 727+733i -739.743))
(check-equal 0.9827737470986545+0.9908846721090973i (/ -727-733i -739.743))
(check-equal 751/761-757/761i (/ 751-757i 761))
(check-equal -751/761+757/761i (/ -751+757i 761))
(check-equal -751/761+757/761i (/ 751-757i -761))
(check-equal 751/761-757/761i (/ -751+757i -761))
(check-equal 771.67602348579+789.7445822002472i (/ 769.773+787.797i 809/811))
(check-equal -771.67602348579-789.7445822002472i (/ -769.773-787.797i 809/811))
(check-equal -771.67602348579-789.7445822002472i (/ 769.773+787.797i -809/811))
(check-equal 771.67602348579+789.7445822002472i (/ -769.773-787.797i -809/811))
(check-equal 9.785309578671098e-19-9.856822020315465e-19i (/ 821.823-827.829i 839853857859863877881))
(check-equal -9.785309578671098e-19+9.856822020315465e-19i (/ -821.823+827.829i 839853857859863877881))
(check-equal -9.785309578671098e-19+9.856822020315465e-19i (/ 821.823-827.829i -839853857859863877881))
(check-equal 9.785309578671098e-19-9.856822020315465e-19i (/ -821.823+827.829i -839853857859863877881))
(check-equal -0.018246152206427474+0.9694889277478006i (/ 883.887+907.911i 919-929i))
(check-equal 0.018246152206427474-0.9694889277478006i (/ -883.887-907.911i 919-929i))
(check-equal 0.018246152206427474-0.9694889277478006i (/ 883.887+907.911i -919+929i))
(check-equal -0.018246152206427474+0.9694889277478006i (/ -883.887-907.911i -919+929i))
(check-equal -0.010131003802343523-0.9690838367145014i (/ 937.941-947.953i 967.971+977.983i))
(check-equal 0.010131003802343523+0.9690838367145014i (/ -937.941+947.953i 967.971+977.983i))
(check-equal 0.010131003802343523+0.9690838367145014i (/ 937.941-947.953i -967.971-977.983i))
(check-equal -0.010131003802343523-0.9690838367145014i (/ -937.941+947.953i -967.971-977.983i))
(check-equal 7.802951286468288+0.8582721759444982i (/ 991.997+109.113i 127.131))
(check-equal -7.802951286468288-0.8582721759444982i (/ -991.997-109.113i 127.131))
(check-equal -7.802951286468288-0.8582721759444982i (/ 991.997+109.113i -127.131))
(check-equal 7.802951286468288+0.8582721759444982i (/ -991.997-109.113i -127.131))
(check-equal 0.8734968152866243-0.9500063694267517i (/ 137.139-149.151i 157))
(check-equal -0.8734968152866243+0.9500063694267517i (/ -137.139+149.151i 157))
(check-equal -0.8734968152866243+0.9500063694267517i (/ 137.139-149.151i -157))
(check-equal 0.8734968152866243-0.9500063694267517i (/ -137.139+149.151i -157))
(check-equal 168.82597109826588 (/ 163.167 173/179))
(check-equal -168.82597109826588 (/ -163.167 173/179))
(check-equal -168.82597109826588 (/ 163.167 -173/179))
(check-equal 168.82597109826588 (/ -163.167 -173/179))
(check-equal 9.378552108403146e-19 (/ 181.191 193197199211223227229))
(check-equal -9.378552108403146e-19 (/ -181.191 193197199211223227229))
(check-equal -9.378552108403146e-19 (/ 181.191 -193197199211223227229))
(check-equal 9.378552108403146e-19 (/ -181.191 -193197199211223227229))
(check-equal 4.6423579888009783e-4-4.8349869509918905e-4i (/ 0.233239 241+251i))
(check-equal -4.6423579888009783e-4+4.8349869509918905e-4i (/ -0.233239 241+251i))
(check-equal -4.6423579888009783e-4+4.8349869509918905e-4i (/ 0.233239 -241-251i))
(check-equal 4.6423579888009783e-4-4.8349869509918905e-4i (/ -0.233239 -241-251i))
(check-equal 0.942303057066332-949.3090277508401i (/ 257263.0 0.269+271.0i))
(check-equal -0.942303057066332+949.3090277508401i (/ -257263.0 0.269+271.0i))
(check-equal -0.942303057066332+949.3090277508401i (/ 257263.0 -0.269-271.0i))
(check-equal 0.942303057066332-949.3090277508401i (/ -257263.0 -0.269-271.0i))
(check-equal 9.787770947938422e-4 (/ 277.281 283293.307))
(check-equal -9.787770947938422e-4 (/ -277.281 283293.307))
(check-equal -9.787770947938422e-4 (/ 277.281 -283293.307))
(check-equal 9.787770947938422e-4 (/ -277.281 -283293.307))
(check-equal 0.9820599369085173 (/ 311.313 317))
(check-equal -0.9820599369085173 (/ -311.313 317))
(check-equal -0.9820599369085173 (/ 311.313 -317))
(check-equal 0.9820599369085173 (/ -311.313 -317))
(check-equal 114857/337 (/ 331 337/347))
(check-equal -114857/337 (/ -331 337/347))
(check-equal -114857/337 (/ 331 -337/347))
(check-equal 114857/337 (/ -331 -337/347))
(check-equal 349/353359367373379383389 (/ 349 353359367373379383389))
(check-equal -349/353359367373379383389 (/ -349 353359367373379383389))
(check-equal -349/353359367373379383389 (/ 349 -353359367373379383389))
(check-equal 349/353359367373379383389 (/ -349 -353359367373379383389))
(check-equal 159197/328082-162373/328082i (/ 397 401+409i))
(check-equal -159197/328082+162373/328082i (/ -397 401+409i))
(check-equal -159197/328082+162373/328082i (/ 397 -401-409i))
(check-equal 159197/328082-162373/328082i (/ -397 -401-409i))
(check-equal 0.48315298097442744+0.49691964976609426i (/ 419 421.431-433.439i))
(check-equal -0.48315298097442744-0.49691964976609426i (/ -419 421.431-433.439i))
(check-equal -0.48315298097442744-0.49691964976609426i (/ 419 -421.431+433.439i))
(check-equal 0.48315298097442744+0.49691964976609426i (/ -419 -421.431+433.439i))
(check-equal 0.9856337758673244 (/ 443 449.457))
(check-equal -0.9856337758673244 (/ -443 449.457))
(check-equal -0.9856337758673244 (/ 443 -449.457))
(check-equal 0.9856337758673244 (/ -443 -449.457))
(check-equal 461/463 (/ 461 463))
(check-equal -461/463 (/ -461 463))
(check-equal -461/463 (/ 461 -463))
(check-equal 461/463 (/ -461 -463))
(check-equal 115336391/116403227 (/ 467/479 487/491 499/503))
(check-equal 18/85 (/ 3/2 5/4 17/3))
(check-equal 521/509 (/ 509/521))
(check-equal -541/523 (/ -523/541))
(check-equal 1/547557563569571577587 (/ 547557563569571577587))
(check-equal -1/593599601607613617619 (/ -593599601607613617619))
(check-equal 631/809042-641/809042i (/ 631+641i))
(check-equal 643/832058+647/832058i (/ 643-647i))
(check-equal -653/860690-659/860690i (/ -653+659i))
(check-equal -661/889850+673/889850i (/ -661-673i))
(check-equal 0.0014756161804265417 (/ 677.683))
(check-equal -0.001445711369507923 (/ -691.701))
(check-equal 1/709 (/ 709))
(check-equal -1/719 (/ -719))

(check-error (assertion-violation /) (/))
(check-error (assertion-violation /) (/ #\0))
(check-error (assertion-violation /) (/ #f))
(check-error (assertion-violation /) (/ "0"))
(check-error (assertion-violation /) (/ 0 #\0))
(check-error (assertion-violation /) (/ 0 #f))
(check-error (assertion-violation /) (/ 0 "0"))

(check-error (assertion-violation /) (/ 0))
(check-error (assertion-violation /) (/ 1 0))

(check-equal 123/456 (abs 123/456))
(check-equal 123/456 (abs -123/456))
(check-equal 123/456 (abs (/ -123 456)))
(check-equal 123/456 (abs (/ 123 -456)))
(check-equal 123/456 (abs (/ -123 -456)))
(check-equal 12345678901234567890 (abs 12345678901234567890))
(check-equal 12345678901234567890 (abs -12345678901234567890))
(check-equal 123.456 (abs 123.456))
(check-equal 123.456 (abs -123.456))
(check-equal 123 (abs 123))
(check-equal 123 (abs -123))
(check-equal 0 (abs 0))

(check-error (assertion-violation abs) (abs))
(check-error (assertion-violation abs) (abs 1 2))
(check-error (assertion-violation abs) (abs 1+2i))
(check-error (assertion-violation abs) (abs #\0))
(check-error (assertion-violation abs) (abs #f))
(check-error (assertion-violation abs) (abs "0"))

(check-equal (2 1) (let-values ((ret (floor/ 5 2))) ret))
(check-equal (-3 1) (let-values ((ret (floor/ -5 2))) ret))
(check-equal (-3 -1) (let-values ((ret (floor/ 5 -2))) ret))
(check-equal (2 -1) (let-values ((ret (floor/ -5 -2))) ret))
(check-equal (-3.0 -1.0) (let-values ((ret (floor/ 5.0 -2))) ret))
(check-equal (2.0 -1.0) (let-values ((ret (floor/ -5 -2.0))) ret))

(check-equal (100000639104090 32850)
    (let-values ((ret (floor/ 12345678901234567890 123456))) ret))
(check-equal (-100000639104091 90606)
    (let-values ((ret (floor/ -12345678901234567890 123456))) ret))
(check-equal (-100000639104091 -90606)
    (let-values ((ret (floor/ 12345678901234567890 -123456))) ret))
(check-equal (100000639104090 -32850)
    (let-values ((ret (floor/ -12345678901234567890 -123456))) ret))

(check-error (assertion-violation floor/) (floor/ 1))
(check-error (assertion-violation floor/) (floor/ 1 2 3))

(check-error (assertion-violation floor-quotient) (floor-quotient 1))
(check-error (assertion-violation floor-quotient) (floor-quotient 1 2 3))
(check-error (assertion-violation floor-quotient) (floor-quotient 1/2 3))
(check-error (assertion-violation floor-quotient) (floor-quotient 1 2/3))
(check-error (assertion-violation floor-quotient) (floor-quotient +i 1))
(check-error (assertion-violation floor-quotient) (floor-quotient 1 +i))
(check-error (assertion-violation floor-quotient) (floor-quotient 1.2 3))
(check-error (assertion-violation floor-quotient) (floor-quotient 1 2.3))

(check-error (assertion-violation floor-remainder) (floor-remainder 1))
(check-error (assertion-violation floor-remainder) (floor-remainder 1 2 3))

(check-equal (2 1) (let-values ((ret (truncate/ 5 2))) ret))
(check-equal (-2 -1) (let-values ((ret (truncate/ -5 2))) ret))
(check-equal (-2 1) (let-values ((ret (truncate/ 5 -2))) ret))
(check-equal (2 -1) (let-values ((ret (truncate/ -5 -2))) ret))
(check-equal (-2.0 1.0) (let-values ((ret (truncate/ 5 -2.0))) ret))
(check-equal (2.0 -1.0) (let-values ((ret (truncate/ -5.0 -2))) ret))

(check-equal (100000639104090 32850)
    (let-values ((ret (truncate/ 12345678901234567890 123456))) ret))
(check-equal (-100000639104090 -32850)
    (let-values ((ret (truncate/ -12345678901234567890 123456))) ret))
(check-equal (-100000639104090 32850)
    (let-values ((ret (truncate/ 12345678901234567890 -123456))) ret))
(check-equal (100000639104090 -32850)
    (let-values ((ret (truncate/ -12345678901234567890 -123456))) ret))

(check-error (assertion-violation truncate/) (truncate/ 1))
(check-error (assertion-violation truncate/) (truncate/ 1 2 3))

(check-error (assertion-violation truncate-quotient) (truncate-quotient 1))
(check-error (assertion-violation truncate-quotient) (truncate-quotient 1 2 3))
(check-error (assertion-violation truncate-quotient) (truncate-quotient 1/2 3))
(check-error (assertion-violation truncate-quotient) (truncate-quotient 1 2/3))
(check-error (assertion-violation truncate-quotient) (truncate-quotient +i 1))
(check-error (assertion-violation truncate-quotient) (truncate-quotient 1 +i))
(check-error (assertion-violation truncate-quotient) (truncate-quotient 1.2 3))
(check-error (assertion-violation truncate-quotient) (truncate-quotient 1 2.3))

(check-error (assertion-violation truncate-remainder) (truncate-remainder 1))
(check-error (assertion-violation truncate-remainder) (truncate-remainder 1 2 3))
(check-error (assertion-violation truncate-remainder) (truncate-remainder 1/2 3))
(check-error (assertion-violation truncate-remainder) (truncate-remainder 1 2/3))
(check-error (assertion-violation truncate-remainder) (truncate-remainder +i 1))
(check-error (assertion-violation truncate-remainder) (truncate-remainder 1 +i))
(check-error (assertion-violation truncate-remainder) (truncate-remainder 1.2 3))
(check-error (assertion-violation truncate-remainder) (truncate-remainder 1 2.3))

(check-equal #t (equal? (quotient -5 2) (truncate-quotient -5 2)))
(check-equal #f (equal? (quotient -5 2) (floor-quotient -5 2)))
(check-equal #t (equal? (remainder -5 2) (truncate-remainder -5 2)))
(check-equal #f (equal? (remainder -5 2) (floor-remainder -5 2)))
(check-equal #t (equal? (modulo -5 2) (floor-remainder -5 2)))
(check-equal #f (equal? (modulo -5 2) (truncate-remainder -5 2)))

(check-equal 4 (gcd 32 -36))
(check-equal 0 (gcd))
(check-equal 288 (lcm 32 -36))
(check-equal 288.0 (lcm 32.0 -36))
(check-equal 1 (lcm))

(check-error (assertion-violation gcd) (gcd 1.2))
(check-error (assertion-violation gcd) (gcd 1 2.3))
(check-error (assertion-violation gcd) (gcd +i))
(check-error (assertion-violation gcd) (gcd 1/2))

(check-error (assertion-violation lcm) (lcm 1.2))
(check-error (assertion-violation lcm) (lcm 1 2.3))
(check-error (assertion-violation lcm) (lcm +i))
(check-error (assertion-violation lcm) (lcm 1/2))

(check-equal 3 (numerator (/ 6 4)))
(check-equal 3.0 (numerator 1.5))
(check-equal 2.0 (denominator 1.5))
(check-equal 2 (denominator (/ 6 4)))
(check-equal 2.0 (denominator (inexact (/ 6 4))))
(check-equal 2057613150205761315 (numerator 12345678901234567890/123456))
(check-equal 20576 (denominator 12345678901234567890/123456))

(check-error (assertion-violation numerator) (numerator))
(check-error (assertion-violation numerator) (numerator 1 2))
(check-error (assertion-violation numerator) (numerator +inf.0))
(check-error (assertion-violation numerator) (numerator +i))

(check-error (assertion-violation denominator) (denominator))
(check-error (assertion-violation denominator) (denominator 1 2))
(check-error (assertion-violation denominator) (denominator +inf.0))
(check-error (assertion-violation denominator) (denominator +i))

(check-equal -5.0 (floor -4.3))
(check-equal -4.0 (ceiling -4.3))
(check-equal 0 (ceiling (/ -1 3)))
(check-equal -1 (ceiling (/ 4 -3)))
(check-equal -4.0 (truncate -4.3))
(check-equal -4.0 (round -4.3))
(check-equal 3.0 (floor 3.5))
(check-equal 4.0 (ceiling 3.5))
(check-equal 1 (ceiling (/ 1 3)))
(check-equal 2 (ceiling (/ 4 3)))
(check-equal 3.0 (truncate 3.5))
(check-equal 4.0 (round 3.5))
(check-equal 4 (round 7/2))
(check-equal 7 (round 7))

(check-error (assertion-violation floor) (floor))
(check-error (assertion-violation floor) (floor 1 2))
(check-error (assertion-violation floor) (floor +i))
(check-error (assertion-violation floor) (floor #\0))
(check-error (assertion-violation floor) (floor #f))
(check-error (assertion-violation floor) (floor "0"))

(check-error (assertion-violation ceiling) (ceiling))
(check-error (assertion-violation ceiling) (ceiling 1 2))
(check-error (assertion-violation ceiling) (ceiling +i))
(check-error (assertion-violation ceiling) (ceiling #\0))
(check-error (assertion-violation ceiling) (ceiling #f))
(check-error (assertion-violation ceiling) (ceiling "0"))

(check-error (assertion-violation truncate) (truncate))
(check-error (assertion-violation truncate) (truncate 1 2))
(check-error (assertion-violation truncate) (truncate +i))
(check-error (assertion-violation truncate) (truncate #\0))
(check-error (assertion-violation truncate) (truncate #f))
(check-error (assertion-violation truncate) (truncate "0"))

(check-error (assertion-violation round) (round))
(check-error (assertion-violation round) (round 1 2))
(check-error (assertion-violation round) (round +i))
(check-error (assertion-violation round) (round #\0))
(check-error (assertion-violation round) (round #f))
(check-error (assertion-violation round) (round "0"))

(check-equal 1/3 (rationalize (exact .3) 1/10))
(check-equal #i1/3 (rationalize .3 1/10))

(check-error (assertion-violation rationalize) (rationalize 1))
(check-error (assertion-violation rationalize) (rationalize 1 2 3))

(check-equal 1.1051709180756477 (exp 1/10))
(check-equal 0.9048374180359595 (exp -1/10))
(check-equal +inf.0 (exp 12345678901234567890))
(check-equal 0.0 (exp -12345678901234567890))
(check-equal -1.1312043837568135+2.4717266720048188i (exp 1+2i))
(check-equal -0.1530918656742263+0.33451182923926226i (exp -1+2i))
(check-equal -3.209883040054176+0.8484263372940289i (exp 1.2-3.4i))
(check-equal -0.2911940196921122+0.07696750083614708i (exp -1.2-3.4i))
(check-equal 1.1051709180756477 (exp 0.1))
(check-equal 0.9048374180359595 (exp -0.1))
(check-equal 22026.465794806718 (exp 10))
(check-equal 4.5399929762484854e-5 (exp -10))
(check-equal 2.6195173187490626e53 (exp 123))
(check-equal 2.6195173187490626e53 (exp 123))

(check-error (assertion-violation exp) (exp))
(check-error (assertion-violation exp) (exp 1 2))
(check-error (assertion-violation exp) (exp #\0))
(check-error (assertion-violation exp) (exp #f))
(check-error (assertion-violation exp) (exp "0"))

(check-equal 0.4054651081081644 (log 3/2))
(check-equal 0.4054651081081644+3.141592653589793i (log -3/2))
(check-equal 43.959837789202524 (log 12345678901234567890))
(check-equal 43.959837789202524+3.141592653589793i (log -12345678901234567890))
(check-equal 0.8047189562170501+1.1071487177940904i (log 1+2i))
(check-equal 0.8047189562170501+2.0344439357957027i (log -1+2i))
(check-equal 1.2824746787307684-1.2315037123408519i (log 1.2-3.4i))
(check-equal 1.2824746787307684-1.9100889412489412i (log -1.2-3.4i))
(check-equal 0.4054651081081644 (log 1.5))
(check-equal 0.4054651081081644+3.141592653589793i (log -1.5))
(check-equal 14.02623085927966 (log 1234567))
(check-equal 14.02623085927966+3.141592653589793i (log -1234567))
(check-equal 2.302585092994046 (log 10))
(check-equal 2.302585092994046+3.141592653589793i (log -10))

(check-equal 1.296299930939515 (log 12 34/5))
(check-equal 1.296299930939515+1.638872969427635i (log -12 34/5))
(check-equal 0.35169112283875825-0.5763770748080949i (log 12 -34/5))
(check-equal 1.0803873030631417-0.13174457807537043i (log -12 -34/5))
(check-equal -0.19656163223282258 (log 1/2 34))
(check-equal -0.19656163223282258+0.8908881073445405i (log -1/2 34))
(check-equal -0.1095855753117824+0.09762848578177642i (log 1/2 -34))
(check-equal 0.33290188235248397+0.5943097794441011i (log -1/2 -34))
(check-equal 2.409420839653209 (log 1/2 3/4))
(check-equal 2.409420839653209-10.920362978532015i (log -1/2 3/4))
(check-equal 0.020036042677901707+0.2188008586960453i (log 1/2 -3/4))
(check-equal 1.0117203336294214+0.12799029746277502i (log -1/2 -3/4))
(check-equal 9.135110906572999 (log 12345678901234567890 123))
(check-equal 9.135110906572999+0.6528412923504182i (log -12345678901234567890 123))
(check-equal 6.405202410789154-4.181580619625606i (log 12345678901234567890 -123))
(check-equal 6.704039340488562-3.7238324146621795i (log -12345678901234567890 -123))
(check-equal 0.10946774595593235 (log 123 12345678901234567890))
(check-equal 0.10946774595593235+0.07146506474055815i (log -123 12345678901234567890))
(check-equal 0.1089115070648178-0.00778336790337896i (log 123 -12345678901234567890))
(check-equal 0.11399281102569848+0.06331856112064839i (log -123 -12345678901234567890))
(check-equal 1.1591622043557208-0.6678639546657751i (log 12 3+4i))
(check-equal 2.003522491996478+0.7976299164354059i (log -12 3+4i))
(check-equal 0.5337098097672296+0.7342888184661313i (log 12 -3-4i))
(check-equal -0.39462943303818315+1.4090420602656097i (log -12 -3-4i))
(check-equal 0.22820098812915404+0.3139635638686154i (log 1+2i 34))
(check-equal 0.22820098812915404-0.5769245434759251i (log -1-2i 34))
(check-equal 0.283164742120509+0.06169546269417022i (log 1+2i -34))
(check-equal -0.15932271554375732-0.4349858309681545i (log -1-2i -34))
(check-equal 0.6729526521196117+0.30018116126729044i (log 1+2i 3+4i))
(check-equal -0.17140763552114516-1.1653127098338907i (log -1-2i 3+4i))
(check-equal -0.15432391508523913+0.4755881929072293i (log 1+2i -3-4i))
(check-equal 0.7740153277201737-0.1991650488922492i (log -1-2i -3-4i))
(check-equal -8.637683358612836 (log 12 0.75))
(check-equal -8.637683358612836-10.920362978532015i (log -12 0.75))
(check-equal -0.07182846166312633-0.7843928735509108i (log 12 -0.75))
(check-equal 0.9198558292883932-0.8752034347841812i (log -12 -0.75))
(check-equal 0.14898285427440489 (log 1.2 3.4))
(check-equal 0.14898285427440489+2.5671316586455806i (log -1.2 3.4))
(check-equal 0.019628407972746984-0.050388707515650104i (log 1.2 -3.4))
(check-equal 0.887878965606753+0.2878294569909943i (log -1.2 -3.4))
(check-equal 0.7859844846031109 (log 123 456))
(check-equal 0.7859844846031109+0.5131231266956723i (log -123 456))
(check-equal 0.6221700165879744-0.3192498242479197i (log 123 -456))
(check-equal 0.83058948443263+0.08692845344305467i (log -123 -456))

(check-error (assertion-violation log) (log))
(check-error (assertion-violation log) (log 1 2 3))
(check-error (assertion-violation log) (log #\0))
(check-error (assertion-violation log) (log #f))
(check-error (assertion-violation log) (log "0"))
(check-error (assertion-violation log) (log 1 #\2))
(check-error (assertion-violation log) (log 1 #t))
(check-error (assertion-violation log) (log 1 "2"))

(check-equal 0.479425538604203 (sin 1/2))
(check-equal 3.165778513216168+1.959601041421606i (sin 1+2i))
(check-equal 0.479425538604203 (sin 0.5))
(check-equal 0.8414709848078965 (sin 1))

(check-error (assertion-violation sin) (sin))
(check-error (assertion-violation sin) (sin 1 2))
(check-error (assertion-violation sin) (sin #\0))
(check-error (assertion-violation sin) (sin #f))
(check-error (assertion-violation sin) (sin "0"))

(check-equal 0.8775825618903728 (cos 1/2))
(check-equal 2.0327230070196656-3.0518977991517997i (cos 1+2i))
(check-equal 0.8775825618903728 (cos 0.5))
(check-equal 0.5403023058681398 (cos 1))

(check-error (assertion-violation cos) (cos))
(check-error (assertion-violation cos) (cos 1 2))
(check-error (assertion-violation cos) (cos #\0))
(check-error (assertion-violation cos) (cos #f))
(check-error (assertion-violation cos) (cos "0"))

(check-equal 0.5463024898437905 (tan 1/2))
(check-equal 0.0338128260798967+1.0147936161466335i (tan 1+2i))
(check-equal 0.5463024898437905 (tan 0.5))
(check-equal 1.5574077246549023 (tan 1))

(check-error (assertion-violation tan) (tan))
(check-error (assertion-violation tan) (tan 1 2))
(check-error (assertion-violation tan) (tan #\0))
(check-error (assertion-violation tan) (tan #f))
(check-error (assertion-violation tan) (tan "0"))

(check-equal 0.5235987755982989 (asin 1/2))
(check-equal 0.42707858639247603+1.528570919480998i (asin 1+2i))
(check-equal 0.5235987755982989 (asin 0.5))
(check-equal 1.5707963267948966 (asin 1))

(check-error (assertion-violation asin) (asin))
(check-error (assertion-violation asin) (asin 1 2))
(check-error (assertion-violation asin) (asin #\0))
(check-error (assertion-violation asin) (asin #f))
(check-error (assertion-violation asin) (asin "0"))

(check-equal 1.0471975511965979 (acos 1/2))
(check-equal 1.1437177404024206-1.528570919480998i (acos 1+2i))
(check-equal 1.0471975511965979 (acos 0.5))
(check-equal 0.0 (acos 1))

(check-error (assertion-violation acos) (acos))
(check-error (assertion-violation acos) (acos 1 2))
(check-error (assertion-violation acos) (acos #\0))
(check-error (assertion-violation acos) (acos #f))
(check-error (assertion-violation acos) (acos "0"))

(check-equal 0.4636476090008061 (atan 1/2))
(check-equal 1.3389725222944935+0.40235947810852507i (atan 1+2i))
(check-equal 0.4636476090008061 (atan 0.5))
(check-equal 0.7853981633974483 (atan 1))
(check-equal 0.5880026035475675 (atan 1/2 3/4))
(check-equal 0.5880026035475675 (atan 0.5 0.75))
(check-equal 0.4636476090008061 (atan 1 2))

(check-error (assertion-violation atan) (atan))
(check-error (assertion-violation atan) (atan 1 2 3))
(check-error (assertion-violation atan) (atan #\0))
(check-error (assertion-violation atan) (atan #f))
(check-error (assertion-violation atan) (atan "0"))
(check-error (assertion-violation atan) (atan 1 +2i))
(check-error (assertion-violation atan) (atan 1 #\2))
(check-error (assertion-violation atan) (atan 1 #t))
(check-error (assertion-violation atan) (atan 1 "2"))

(check-equal 1/4 (square 1/2))
(check-equal 1/4 (square -1/2))
(check-equal 1/5502209507697009701905199875019052100 (square 1/2345678901234567890))
(check-equal 38103946883097091875476299968754763025/380689 (square 6172839450617283945/617))
(check-equal 152415787532388367501905199875019052100 (square 12345678901234567890))
(check-equal -1012+816i (square 12+34i))
(check-equal -1012-816i (square -12+34i))
(check-equal -1012-816i (square 12-34i))
(check-equal -1012+816i (square -12-34i))
(check-equal -3071.6928000000003-1401.3304i (square -12.34+56.78i))
(check-equal 15241.383936 (square 123.456))
(check-equal 15241.383936 (square -123.456))
(check-equal 15129 (square 123))
(check-equal 15129 (square -123))

(check-equal 1764 (square 42))
(check-equal 4.0 (square 2.0))

(check-error (assertion-violation square) (square))
(check-error (assertion-violation square) (square 1 2))
(check-error (assertion-violation square) (square #\1))
(check-error (assertion-violation square) (square #t))
(check-error (assertion-violation square) (square "123"))

(check-equal 0.7071067811865476 (sqrt 1/2))
(check-equal 0.0+0.7071067811865476i (sqrt -1/2))
(check-equal 6.529286405631101e-10 (sqrt 1/2345678901234567890))
(check-equal 100023007.48793943 (sqrt 6172839450617283945/617))
(check-equal 3513641828.820144 (sqrt 12345678901234567890))
(check-equal 4.9018115403715745+3.468105589124983i (sqrt 12+34i))
(check-equal 3.468105589124983+4.9018115403715745i (sqrt -12+34i))
(check-equal 4.9018115403715745-3.468105589124983i (sqrt 12-34i))
(check-equal 3.468105589124983-4.9018115403715745i (sqrt -12-34i))
(check-equal 4.783589439635793+5.934873876250033i (sqrt -12.34+56.78i))
(check-equal 11.111075555498667 (sqrt 123.456))
(check-equal 0.0+11.111075555498667i (sqrt -123.456))
(check-equal 11.090536506409418 (sqrt 123))
(check-equal 0.0+11.090536506409418i (sqrt -123))

(check-equal 3 (sqrt 9))
(check-equal +i (sqrt -1))

(check-error (assertion-violation sqrt) (sqrt))
(check-error (assertion-violation sqrt) (sqrt 1 2))
(check-error (assertion-violation sqrt) (sqrt #\1))
(check-error (assertion-violation sqrt) (sqrt #t))
(check-error (assertion-violation sqrt) (sqrt "123"))

(check-equal (2 0) (let-values ((ret (exact-integer-sqrt 4))) ret))
(check-equal (2 1) (let-values ((ret (exact-integer-sqrt 5))) ret))
(check-equal (12345678 123456) (let-values ((ret (exact-integer-sqrt 152415765403140))) ret))
(check-equal (12345678901234567890 1234567890)
    (let-values ((ret (exact-integer-sqrt 152415787532388367501905199876253619990))) ret))

(check-error (assertion-violation exact-integer-sqrt) (exact-integer-sqrt))
(check-error (assertion-violation exact-integer-sqrt) (exact-integer-sqrt 1 2))
(check-error (assertion-violation exact-integer-sqrt) (exact-integer-sqrt -1))
(check-error (assertion-violation exact-integer-sqrt) (exact-integer-sqrt 1.0))
(check-error (assertion-violation exact-integer-sqrt) (exact-integer-sqrt 1/2))
(check-error (assertion-violation exact-integer-sqrt) (exact-integer-sqrt +i))
(check-error (assertion-violation exact-integer-sqrt) (exact-integer-sqrt #\1))
(check-error (assertion-violation exact-integer-sqrt) (exact-integer-sqrt #t))
(check-error (assertion-violation exact-integer-sqrt) (exact-integer-sqrt "123"))

(check-equal 0.5946035575013605 (expt 1/2 3/4))
(check-equal -0.4204482076268572+0.4204482076268573i (expt -1/2 3/4))
(check-equal 1.681792830507429 (expt 1/2 -3/4))
(check-equal -1.1892071150027208-1.189207115002721i (expt -1/2 -3/4))
(check-equal 2.0827456347946116e14 (expt 12345678901234567890 3/4))
(check-equal 0.12900959407446694+0.03392409290517001i (expt 1+2i 3+4i))
(check-equal 0.0010615540860505679-1.0926740923978484e-4i (expt 1+2i -3+4i))
(check-equal 932.1391946432208+95.94653366034186i (expt 1+2i 3-4i))
(check-equal 7.250043728275966-1.9064563280666638i (expt 1+2i -3-4i))
(check-equal -0.0032506884953619+3.3459841076514e-4i (expt -1+2i 3+4i))
(check-equal -2.528338457798414e-5-6.648465903129566e-6i (expt -1+2i -3+4i))
(check-equal -36993.67050808001+9727.77818752736i (expt -1+2i 3-4i))
(check-equal -304.40202814058-31.332573082548i (expt -1+2i -3-4i))
(check-equal 932.1391946432208-95.94653366034186i (expt 1-2i 3+4i))
(check-equal 7.250043728275966+1.9064563280666638i (expt 1-2i -3+4i))
(check-equal 0.12900959407446694-0.03392409290517001i (expt 1-2i 3-4i))
(check-equal 0.0010615540860505679+1.0926740923978484e-4i (expt 1-2i -3-4i))
(check-equal -36993.67050808001-9727.77818752736i (expt -1-2i 3+4i))
(check-equal -304.40202814058+31.332573082548i (expt -1-2i -3+4i))
(check-equal -0.0032506884953619-3.3459841076514e-4i (expt -1-2i 3-4i))
(check-equal -2.528338457798414e-5+6.648465903129566e-6i (expt -1-2i -3-4i))
(check-equal 4.084477325155075 (expt 12.34 0.56))
(check-equal -0.7653547305831429+4.012130002390245i (expt -12.34 0.56))
(check-equal 0.2448293674789915 (expt 12.34 -0.56))
(check-equal -0.045876448727404935-0.24049276627858238i (expt -12.34 -0.56))
(check-equal 1728 (expt 12 3))
(check-equal -1728 (expt -12 3))
(check-equal 1/1728 (expt 12 -3))
(check-equal -1/1728 (expt -12 -3))
(check-equal 1 (expt 0 0))
(check-equal 0 (expt 0 3))
(check-equal 0 (expt 0 3+4i))
(check-equal 0 (expt 0 3-4i))
(check-equal 1.0 (expt 0.0 0.0))
(check-equal 0.0 (expt 0.0 3))
(check-equal 0.0 (expt 0.0 3+4i))
(check-equal 0.0 (expt 0.0 3-4i))

(check-error (assertion-violation expt) (expt 1))
(check-error (assertion-violation expt) (expt 1 2 3))
(check-error (assertion-violation expt) (expt 0 -1))
(check-error (assertion-violation expt) (expt 0 +i))
(check-error (assertion-violation expt) (expt 1 #\2))
(check-error (assertion-violation expt) (expt 1 #t))
(check-error (assertion-violation expt) (expt #\1 2))
(check-error (assertion-violation expt) (expt #t 2))

(check-equal 1/2+3/4i (make-rectangular 1/2 3/4))
(check-equal 12345678901234567890+123456i (make-rectangular 12345678901234567890 123456))
(check-equal 12.34+56.78i (make-rectangular 12.34 56.78))
(check-equal 1+2i (make-rectangular 1 2))

(check-error (assertion-violation make-rectangular) (make-rectangular 1))
(check-error (assertion-violation make-rectangular) (make-rectangular 1 2 3))
(check-error (assertion-violation make-rectangular) (make-rectangular +i 2))
(check-error (assertion-violation make-rectangular) (make-rectangular 1 +2i))
(check-error (assertion-violation make-rectangular) (make-rectangular #\1 2))
(check-error (assertion-violation make-rectangular) (make-rectangular 1 #\2))

(check-equal 0.36584443443691045+0.34081938001166706i (make-polar 1/2 3/4))
(check-equal -8.299936735875683e18-9.139301817581562e18i (make-polar 12345678901234567890 123456))
(check-equal 12.011284751631464+2.8292470049865663i (make-polar 12.34 56.78))
(check-equal -10.182843297415262+6.348992233440287i (make-polar 12 34))

(check-error (assertion-violation make-polar) (make-polar 1))
(check-error (assertion-violation make-polar) (make-polar 1 2 3))
(check-error (assertion-violation make-polar) (make-polar +i 2))
(check-error (assertion-violation make-polar) (make-polar 1 +2i))
(check-error (assertion-violation make-polar) (make-polar #\1 2))
(check-error (assertion-violation make-polar) (make-polar 1 #\2))

(check-equal 1/2 (real-part 1/2))
(check-equal 12345678901234567890 (real-part 12345678901234567890))
(check-equal 123 (real-part 123+456i))
(check-equal 123.456 (real-part 123.456))
(check-equal 123 (real-part 123))

(check-error (assertion-violation real-part) (real-part))
(check-error (assertion-violation real-part) (real-part 1 2))
(check-error (assertion-violation real-part) (real-part #\1))
(check-error (assertion-violation real-part) (real-part #t))

(check-equal 0 (imag-part 1/2))
(check-equal 0 (imag-part 12345678901234567890))
(check-equal 456 (imag-part 123+456i))
(check-equal 0.0 (imag-part 123.456))
(check-equal 0 (imag-part 123))

(check-error (assertion-violation imag-part) (imag-part))
(check-error (assertion-violation imag-part) (imag-part 1 2))
(check-error (assertion-violation imag-part) (imag-part #\1))
(check-error (assertion-violation imag-part) (imag-part #t))

(check-equal 1/2 (magnitude 1/2))
(check-equal 12345678901234567890 (magnitude 12345678901234567890))
(check-equal 36.05551275463989 (magnitude 12+34i))
(check-equal 12 (magnitude 12@34))
(check-equal 12.34 (magnitude 12.34))
(check-equal 1234 (magnitude 1234))

(check-error (assertion-violation magnitude) (magnitude))
(check-error (assertion-violation magnitude) (magnitude 1 2))

(check-equal 0 (angle 1/2))
(check-equal 0 (angle 12345678901234567890))
(check-equal 1.2315037123408519 (angle 12+34i))
(check-equal 2.5840734641020675 (angle 12@34))
(check-equal 0.0 (angle 12.34))
(check-equal 0 (angle 1234))

(check-error (assertion-violation angle) (angle))
(check-error (assertion-violation angle) (angle 1 2))

(check-equal 1/2 (exact 1/2))
(check-equal 12345678901234567890 (exact 12345678901234567890))
(check-equal 123+456i (exact 123+456i))
(check-equal 3/2+5/2i (exact 1.5+2.5i))
(check-equal 1/2 (exact 0.5))
(check-equal 123 (exact 123))

(check-error (assertion-violation exact) (exact))
(check-error (assertion-violation exact) (exact 1 2))
(check-error (assertion-violation exact) (exact #\1))
(check-error (assertion-violation exact) (exact "123"))

(check-equal 0.5 (inexact 1/2))
(check-equal 1.2345678901235e+019 (inexact 12345678901234567890))
(check-equal 123+456i (inexact 123+456i))
(check-equal 1.5+2.5i (inexact 3/2+5/2i))
(check-equal 0.5 (inexact 0.5))
(check-equal 123.0 (inexact 123))

(check-error (assertion-violation inexact) (inexact))
(check-error (assertion-violation inexact) (inexact 1 2))
(check-error (assertion-violation inexact) (inexact #\1))
(check-error (assertion-violation inexact) (inexact "123"))

(check-equal "823/4526" (number->string 823/4526))
(check-equal "1100110111/1000110101110" (number->string 823/4526 2))
(check-equal "1467/10656" (number->string 823/4526 8))
(check-equal "823/4526" (number->string 823/4526 10))
(check-equal "337/11ae" (number->string 823/4526 16))
(check-equal "12345678901234567890" (number->string 12345678901234567890))
(check-equal "1010101101010100101010011000110011101011000111110000101011010010"
    (number->string 12345678901234567890 2))
(check-equal "1255245230635307605322" (number->string 12345678901234567890 8))
(check-equal "12345678901234567890" (number->string 12345678901234567890 10))
(check-equal "ab54a98ceb1f0ad2" (number->string 12345678901234567890 16))
(check-equal "12345+67890i" (number->string 12345+67890i))
(check-equal "11000000111001+10000100100110010i" (number->string 12345+67890i 2))
(check-equal "30071+204462i" (number->string 12345+67890i 8))
(check-equal "12345+67890i" (number->string 12345+67890i 10))
(check-equal "3039+10932i" (number->string 12345+67890i 16))
(check-equal "12345.6789" (number->string 12345.6789))
(check-equal "12345.6789" (number->string 12345.6789 10))
(check-equal "4567" (number->string 4567))
(check-equal "1000111010111" (number->string 4567 2))
(check-equal "10727" (number->string 4567 8))
(check-equal "4567" (number->string 4567 10))
(check-equal "11d7" (number->string 4567 16))

(check-error (assertion-violation number->string) (number->string))
(check-error (assertion-violation number->string) (number->string 1 2 3))
(check-error (assertion-violation number->string) (number->string 1 1))
(check-error (assertion-violation number->string) (number->string 1 9))
(check-error (assertion-violation number->string) (number->string 1 17))
(check-error (assertion-violation number->string) (number->string 1.2 2))
(check-error (assertion-violation number->string) (number->string #\1))
(check-error (assertion-violation number->string) (number->string "123"))

(check-equal 86 (string->number "1010110" 2))
(check-equal 266312 (string->number "1010110" 8))
(check-equal 1010110 (string->number "1010110" 10))
(check-equal 1010110 (string->number "1010110"))
(check-equal 16843024 (string->number "1010110" 16))

(check-equal 86 (string->number "+1010110" 2))
(check-equal 266312 (string->number "+1010110" 8))
(check-equal 1010110 (string->number "+1010110" 10))
(check-equal 1010110 (string->number "+1010110"))
(check-equal 16843024 (string->number "+1010110" 16))

(check-equal -86 (string->number "-1010110" 2))
(check-equal -266312 (string->number "-1010110" 8))
(check-equal -1010110 (string->number "-1010110" 10))
(check-equal -1010110 (string->number "-1010110"))
(check-equal -16843024 (string->number "-1010110" 16))

(check-equal 27 (string->number "000011011" 2))
(check-equal 4617 (string->number "000011011" 8))
(check-equal 11011 (string->number "000011011" 10))
(check-equal 69649 (string->number "000011011" 16))

(check-equal 342391 (string->number "1234567" 8))
(check-equal 1234567 (string->number "1234567" 10))
(check-equal 1234567 (string->number "1234567"))
(check-equal 19088743 (string->number "1234567" 16))

(check-equal 1234567890 (string->number "1234567890" 10))
(check-equal 1234567890 (string->number "1234567890"))
(check-equal 78187493520 (string->number "1234567890" 16))

(check-equal 439041101 (string->number "1A2B3C4D" 16))

(check-equal 16843024 (string->number "#x1010110" 2))
(check-equal 1010110 (string->number "#d1010110" 8))
(check-equal 266312 (string->number "#o1010110" 10))
(check-equal 266312 (string->number "#o1010110"))
(check-equal 86 (string->number "#b1010110" 16))

(check-equal 4444580219171430789360
    (string->number "111100001111000011110000111100001111000011110000111100001111000011110000" 2))
(check-equal 15040940990774093821774547278228463013989131795412990679509667840
    (string->number "111100001111000011110000111100001111000011110000111100001111000011110000" 8))
(check-equal 111100001111000011110000111100001111000011110000111100001111000011110000
    (string->number "111100001111000011110000111100001111000011110000111100001111000011110000" 10))
(check-equal 33154376531681113854560661422351801859329838410521262808467732924068440520839004225536
    (string->number "111100001111000011110000111100001111000011110000111100001111000011110000" 16))

(check-equal 13889035636016081036092333743870916983
    (string->number "123456712345671234567123456712345671234567" 8))
(check-equal 123456712345671234567123456712345671234567
    (string->number "123456712345671234567123456712345671234567" 10))
(check-equal 26605824711816610855172917917002038088574653646183
    (string->number "123456712345671234567123456712345671234567" 16))

(check-equal 123456789012345678901234567890 (string->number "123456789012345678901234567890" 10))
(check-equal 94522879687365475552814062743484560
    (string->number "123456789012345678901234567890" 16))

(check-equal 3/17 (string->number "10101/1110111" 2))
(check-equal 57/4097 (string->number "10101/1110111" 8))
(check-equal 91/10001 (string->number "10101/1110111" 10))
(check-equal 91/10001 (string->number "10101/1110111"))
(check-equal 241/65537 (string->number "10101/1110111" 16))

(check-equal 27/119 (string->number "0011011/0001110111" 2))
(check-equal 4617/299081 (string->number "0011011/0001110111" 8))
(check-equal 11011/1110111 (string->number "0011011/0001110111" 10))
(check-equal 69649/17891601 (string->number "0011011/0001110111" 16))

(check-equal 668/375 (string->number "1234/567" 8))
(check-equal 1234/567 (string->number "1234/567" 10))
(check-equal 1234/567 (string->number "1234/567"))
(check-equal 4660/1383 (string->number "1234/567" 16))

(check-equal 668/375 (string->number "+1234/567" 8))
(check-equal 1234/567 (string->number "+1234/567" 10))
(check-equal 1234/567 (string->number "+1234/567"))
(check-equal 4660/1383 (string->number "+1234/567" 16))

(check-equal -668/375 (string->number "-1234/567" 8))
(check-equal -1234/567 (string->number "-1234/567" 10))
(check-equal -1234/567 (string->number "-1234/567"))
(check-equal -4660/1383 (string->number "-1234/567" 16))

(check-equal 218107/329 (string->number "654321/987" 10))
(check-equal 218107/329 (string->number "654321/987"))
(check-equal 737369/271 (string->number "654321/987" 16))

(check-equal 43981/239 (string->number "ABCD/EF" 16))

(check-equal 241/65537 (string->number "#x10101/1110111" 2))
(check-equal 91/10001 (string->number "#d10101/1110111" 8))
(check-equal 57/4097 (string->number "#o10101/1110111" 10))
(check-equal 57/4097 (string->number "#o10101/1110111"))
(check-equal 3/17 (string->number "#b10101/1110111" 16))

(check-equal 123.0 (string->number "123."))
(check-equal 123.0 (string->number "123.0"))
(check-equal 123.456 (string->number "123.456"))
(check-equal 0.456 (string->number ".456"))
(check-equal 0.456 (string->number "0.456"))
(check-equal 1.23e47 (string->number "123.e45"))
(check-equal 1.23e47 (string->number "123.e+45"))
(check-equal 1.23e-43 (string->number "123.e-45"))
(check-equal 1.23456e80 (string->number "123.456e78"))
(check-equal 1.23456e80 (string->number "123.456e+78"))
(check-equal 1.23456e-76 (string->number "123.456e-78"))
(check-equal 1.23e44 (string->number "0.123e45"))
(check-equal 1.23e44 (string->number "0.123e+45"))
(check-equal 1.23e-46 (string->number ".123e-45"))
(check-equal 123.0 (string->number "+123."))
(check-equal 123.0 (string->number "+123.0"))
(check-equal 123.456 (string->number "+123.456"))
(check-equal 0.456 (string->number "+.456"))
(check-equal 0.456 (string->number "+0.456"))
(check-equal 1.23e47 (string->number "+123.e45"))
(check-equal 1.23e47 (string->number "+123.e+45"))
(check-equal 1.23e-43 (string->number "+123.e-45"))
(check-equal 1.23456e80 (string->number "+123.456e78"))
(check-equal 1.23456e80 (string->number "+123.456e+78"))
(check-equal 1.23456e-76 (string->number "+123.456e-78"))
(check-equal 1.23e44 (string->number "+0.123e45"))
(check-equal 1.23e44 (string->number "+0.123e+45"))
(check-equal 1.23e-46 (string->number "+.123e-45"))
(check-equal -123.0 (string->number "-123."))
(check-equal -123.0 (string->number "-123.0"))
(check-equal -123.456 (string->number "-123.456"))
(check-equal -0.456 (string->number "-.456"))
(check-equal -0.456 (string->number "-0.456"))
(check-equal -1.23e47 (string->number "-123.e45"))
(check-equal -1.23e47 (string->number "-123.e+45"))
(check-equal -1.23e-43 (string->number "-123.e-45"))
(check-equal -1.23456e80 (string->number "-123.456e78"))
(check-equal -1.23456e80 (string->number "-123.456e+78"))
(check-equal -1.23456e-76 (string->number "-123.456e-78"))
(check-equal -1.23e44 (string->number "-0.123e45"))
(check-equal -1.23e44 (string->number "-0.123e+45"))
(check-equal -1.23e-46 (string->number "-.123e-45"))

(check-equal 10+51i (string->number "1010+110011i" 2))
(check-equal 520+36873i (string->number "1010+110011i" 8))
(check-equal 1010+110011i (string->number "1010+110011i" 10))
(check-equal 4112+1114129i (string->number "1010+110011i" 16))

(check-equal 4112+1114129i (string->number "#x1010+110011i" 2))
(check-equal 1010+110011i (string->number "#d1010+110011i" 8))
(check-equal 520+36873i (string->number "#o1010+110011i" 10))
(check-equal 10+51i (string->number "#b1010+110011i" 16))

(check-equal 12345+67890i (string->number "12345+67890i"))
(check-equal -12345+67890i (string->number "-12345+67890i"))
(check-equal 12345-67890i (string->number "12345-67890i"))
(check-equal -12345-67890i (string->number "-12345-67890i"))

(check-equal 123.45+6.789i (string->number "123.45+6.7890i"))
(check-equal -123.45+6.789i (string->number "-123.45+6.7890i"))
(check-equal 123.45-6.789i (string->number "123.45-6.7890i"))
(check-equal -123.45-6.789i (string->number "-123.45-6.7890i"))

(check-equal 0+67890i (string->number "+67890i"))
(check-equal 0-67890i (string->number "-67890i"))
(check-equal 0.0+6.789i (string->number "+6.7890i"))
(check-equal 0.0-6.789i (string->number "-6.7890i"))

(check-equal 25/2 (string->number "#e12.5"))
(check-equal 12.5 (string->number "#i12.5"))
(check-equal -25/2 (string->number "#e-12.5"))
(check-equal 12.5 (string->number "#i+12.5"))

(check-equal -25/2 (string->number "#e#d-12.5"))
(check-equal 12.5 (string->number "#d#i+12.5"))

(check-equal 2 (string->number "#e12/5" 8))
(check-equal 2.0 (string->number "#i12/5" 8))
(check-equal 12/5 (string->number "#e12/5" 10))
(check-equal 2.4 (string->number "#i12/5" 10))
(check-equal 18/5 (string->number "#e12/5" 16))
(check-equal 3.6 (string->number "#i12/5" 16))

(check-equal -2 (string->number "#e-12/5" 8))
(check-equal 2.0 (string->number "#i+12/5" 8))
(check-equal 12/5 (string->number "#e+12/5" 10))
(check-equal -2.4 (string->number "#i-12/5" 10))
(check-equal -18/5 (string->number "#e-12/5" 16))
(check-equal 3.6 (string->number "#i+12/5" 16))

(check-equal 2 (string->number "#e#o12/5"))
(check-equal 2.0 (string->number "#i#o12/5"))
(check-equal 12/5 (string->number "#e#d12/5"))
(check-equal 2.4 (string->number "#i#d12/5"))
(check-equal 18/5 (string->number "#e#x12/5"))
(check-equal 3.6 (string->number "#i#x12/5"))

(check-equal -2 (string->number "#o#e-12/5"))
(check-equal 2.0 (string->number "#o#i+12/5"))
(check-equal 12/5 (string->number "#d#e+12/5"))
(check-equal -2.4 (string->number "#d#i-12/5"))
(check-equal -18/5 (string->number "#x#e-12/5"))
(check-equal 3.6 (string->number "#x#i+12/5"))

(check-equal #f (string->number "2" 2))
(check-equal #f (string->number "8" 8))
(check-equal #f (string->number "A" 10))
(check-equal #f (string->number "a" 10))
(check-equal #f (string->number "G" 16))
(check-equal #f (string->number "g" 16))

(check-equal #f (string->number "#b2"))
(check-equal #f (string->number "#o8"))
(check-equal #f (string->number "#dA"))
(check-equal #f (string->number "#da"))
(check-equal #f (string->number "#xG"))
(check-equal #f (string->number "#xg"))

(check-equal #f (string->number "#a101"))
(check-equal #f (string->number "#b#a101"))
(check-equal #f (string->number "-#b#e101"))
(check-equal #f (string->number "+#b#e101"))

(check-equal #f (string->number "12.34" 2))
(check-equal #f (string->number "12.34" 8))
(check-equal #f (string->number "12.34" 16))

(check-equal #f (string->number "#b12.34"))
(check-equal #f (string->number "#o12.34"))
(check-equal #f (string->number "#x12.34"))

(check-equal #f (string->number "110011z" 2))
(check-equal #f (string->number "110011z" 8))
(check-equal #f (string->number "110011z" 10))
(check-equal #f (string->number "110011z" 16))

(check-error (assertion-violation string->number) (string->number))
(check-error (assertion-violation string->number) (string->number "123" 10 3))
(check-error (assertion-violation string->number) (string->number "111" 3))
(check-error (assertion-violation string->number) (string->number "111" 12))
(check-error (assertion-violation string->number) (string->number "111" 18))
(check-error (assertion-violation string->number) (string->number #\1))

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

(check-equal () (append))
(check-equal (x y) (append '(x) '(y)))
(check-equal (a b c d) (append '(a) '(b c d)))
(check-equal (a (b) (c)) (append '(a (b)) '((c))))
(check-equal (a b c . d) (append '(a b) '(c . d)))
(check-equal a (append '() 'a))

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
(check-error (assertion-violation member) (member 'a 'b))
(check-error (assertion-violation member)
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
(check-equal (2 4) (assoc 2.0 '((1 1) (2 4) (3 9)) =))
(check-equal ("B" . b) (assoc "b" (list '("A" . a) '("B" . b) '("C" . c)) string-ci=?))
(check-equal #f (assoc "d" (list '("A" . a) '("B" . b) '("C" . c)) string-ci=?))

(check-error (assertion-violation case-lambda) (assoc 'a))
(check-error (assertion-violation case-lambda) (assoc 'a '(a b c) 3 4))
(check-error (assertion-violation assoc) (assoc 'a 'b))
(check-error (assertion-violation assoc)
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

(check-equal 658 (string-length "DURING the whole of a dull, dark, and soundless day in the autumn of the year, when the clouds hung oppressively low in the heavens, I had been passing alone, on horseback, through a singularly dreary tract of country; and at length found myself, as the shades of the evening drew on, within view of the melancholy House of Usher. I know not how it was--but, with the first glimpse of the building, a sense of insufferable gloom pervaded my spirit. I say insufferable; for the feeling was unrelieved by any of that half-pleasurable, because poetic, sentiment, with which the mind usually receives even the sternest natural images of the desolate or terrible."))

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

(define aa "12345")
(define bb (string-copy "abcde"))
(string-copy! bb 1 aa 0 2)
(check-equal "a12de" bb)
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

(define v2 (vector 1 2 3))
(check-error (assertion-violation vector-set!) (vector-set!))
(check-error (assertion-violation vector-set!) (vector-set! v2))
(check-error (assertion-violation vector-set!) (vector-set! v2 1))
(check-error (assertion-violation vector-set!) (vector-set! v2 1 1 1))
(check-error (assertion-violation vector-set!) (vector-set! 1 1 1))
(check-error (assertion-violation vector-set!) (vector-set! v2 -1 1 1))
(check-error (assertion-violation vector-set!) (vector-set! v2 3 1 1))

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

(define a2 #(1 8 2 8))
(define b2 (vector-copy a2))
(check-equal #(1 8 2 8) b2)
(vector-set! b2 0 3)
(check-equal #(3 8 2 8) b2)
(define c (vector-copy b2 1 3))
(check-equal #(8 2) c)

(define v3 (vector 1 2 3 4))
(check-error (assertion-violation vector-copy) (vector-copy))
(check-error (assertion-violation vector-copy) (vector-copy 1))
(check-error (assertion-violation vector-copy) (vector-copy v3 1 2 1))
(check-error (assertion-violation vector-copy) (vector-copy v3 -1 2))
(check-error (assertion-violation vector-copy) (vector-copy v3 3 2))
(check-error (assertion-violation vector-copy) (vector-copy v3 1 5))

(define a3 (vector 1 2 3 4 5))
(define b3 (vector 10 20 30 40 50))
(vector-copy! b3 1 a3 0 2)
(check-equal #(10 1 2 40 50) b3)

(define x2 (vector 'a 'b 'c 'd 'e 'f 'g))
(vector-copy! x2 1 x2 0 3)
(check-equal #(a a b c e f g) x2)

(define x3 (vector 'a 'b 'c 'd 'e 'f 'g))
(vector-copy! x3 1 x3 3 6)
(check-equal #(a d e f e f g) x3)

(check-error (assertion-violation vector-copy!) (vector-copy! a3 0))
(check-error (assertion-violation vector-copy!) (vector-copy! a3 0 b3 1 1 1))
(check-error (assertion-violation vector-copy!) (vector-copy! 1 0 b))
(check-error (assertion-violation vector-copy!) (vector-copy! a3 0 1))
(check-error (assertion-violation vector-copy!) (vector-copy! a3 -1 b3))
(check-error (assertion-violation vector-copy!) (vector-copy! a3 3 b3))
(check-error (assertion-violation vector-copy!) (vector-copy! a3 0 b3 -1))
(check-error (assertion-violation vector-copy!) (vector-copy! a3 0 b3 1 0))
(check-error (assertion-violation vector-copy!) (vector-copy! a3 0 b3 1 6))

(check-equal #(a b c d e f) (vector-append #(a b c) #(d e f)))
(check-error (assertion-violation vector-append) (vector-append 1))
(check-error (assertion-violation vector-append) (vector-append #(1 2) 1))

(define a4 (vector 1 2 3 4 5))
(vector-fill! a4 'smash 2 4)
(check-equal #(1 2 smash smash 5) a4)

(define v4 (vector 1 2 3 4))
(check-error (assertion-violation vector-fill!) (vector-fill! 1))
(check-error (assertion-violation vector-fill!) (vector-fill! 1 #f))
(check-error (assertion-violation vector-fill!) (vector-fill! v4 #f 1 2 1))
(check-error (assertion-violation vector-fill!) (vector-fill! v4 #f -1 2))
(check-error (assertion-violation vector-fill!) (vector-fill! v4 #f 3 2))
(check-error (assertion-violation vector-fill!) (vector-fill! v4 #f 1 5))

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

(define a5 #u8(1 2 3 4 5))
(check-equal #u8(3 4) (bytevector-copy a5 2 4))

(define bv2 (bytevector 1 2 3 4))
(check-error (assertion-violation bytevector-copy) (bytevector-copy))
(check-error (assertion-violation bytevector-copy) (bytevector-copy 1))
(check-error (assertion-violation bytevector-copy) (bytevector-copy bv2 1 2 1))
(check-error (assertion-violation bytevector-copy) (bytevector-copy bv2 -1 2))
(check-error (assertion-violation bytevector-copy) (bytevector-copy bv2 3 2))
(check-error (assertion-violation bytevector-copy) (bytevector-copy bv2 1 5))

(define a6 (bytevector 1 2 3 4 5))
(define b4 (bytevector 10 20 30 40 50))
(bytevector-copy! b4 1 a6 0 2)
(check-equal #u8(10 1 2 40 50) b4)

(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a6 0))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a6 0 b4 1 1 1))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! 1 0 b4))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a6 0 1))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a6 -1 b4))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a6 3 b4))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a6 0 b4 -1))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a6 0 b4 1 0))
(check-error (assertion-violation bytevector-copy!) (bytevector-copy! a6 0 b4 1 6))

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

(check-equal 1000000 (apply + (make-list 1000000 1)))

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
(define (v5)
    (define (v1) (v3) (v2) (v2))
    (define (v2) (v3) (v3))
    (define (v3) (cc-values 1 2 3 4))
    (v1))
(check-equal (4 3 2 1) (let-values (((w x y z) (v5))) (list z y x w)))

(check-equal 5 (call-with-values (lambda () (cc-values 4 5)) (lambda (a b) b)))

;; values

(define (val0) (values))
(define (val1) (values 1))
(define (val2) (values 1 2))

(check-equal 1 (+ (val1) 0))
(check-error (assertion-violation values) (+ (val0) 0))
(check-error (assertion-violation values) (+ (val2) 0))
(check-equal 1 (begin (val0) 1))
(check-equal 1 (begin (val1) 1))
(check-equal 1 (begin (val2) 1))

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

(define e2 #f)
(check-equal exception (call-with-current-continuation
    (lambda (k)
        (with-exception-handler
            (lambda (x) (set! e2 x) (k 'exception))
            (lambda () (+ 1 (raise 'an-error)))))))
(check-equal an-error e2)

(check-equal (another-error)
    (guard (o ((eq? o 10) 10) (else (list o)))
        (with-exception-handler
            (lambda (x) (set! e2 x))
            (lambda ()
                (+ 1 (raise 'another-error))))))
(check-equal another-error e2)

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
(check-equal 3/2 (eval '(inexact->exact 1.5) (environment '(scheme r5rs))))
(check-equal 1.5 (eval '(exact->inexact 3/2) (environment '(scheme r5rs))))

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

(check-error (assertion-violation input-port?) (input-port?))
(check-error (assertion-violation input-port?) (input-port? 'port 'port))

(check-equal #f (output-port? "port"))
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

(define output-file (open-output-file "output3.txt"))
(define input-file (open-input-file "r7rs.scm"))

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
(check-error (assertion-violation read) (read output-file))

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
(check-error (assertion-violation read-char) (read-char output-file))

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
(check-error (assertion-violation peek-char) (peek-char output-file))

(check-equal "abcd" (read-line (open-input-string "abcd\nABCD\n")))
(check-equal "ABCD"
    (let ((p (open-input-string "abcd\n\nABCD\n")))
        (read-line p)
        (read-line p)
        (read-line p)))

(check-error (assertion-violation read-line) (read-line 'port))
(check-error (assertion-violation read-line) (read-line (open-input-bytevector #u8(1 2 3))))
(check-error (assertion-violation read-line) (read-line (current-input-port) 2))
(check-error (assertion-violation read-line) (read-line output-file))

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
(check-error (assertion-violation char-ready?) (char-ready? output-file))

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

(check-error (assertion-violation read-string) (read-string -1 output-file))
(check-error (assertion-violation read-string) (read-string 1 'port))
(check-error (assertion-violation read-string) (read-string 1 (open-input-bytevector #u8(1 2 3))))
(check-error (assertion-violation read-string) (read-string 1 (current-input-port) 2))
(check-error (assertion-violation read-string) (read-string 1 output-file))

(check-equal 1 (read-u8 (open-input-bytevector #u8(1 2 3 4 5))))

(check-error (assertion-violation read-u8) (read-u8 'port))
(check-error (assertion-violation read-u8) (read-u8 (open-input-string "1234")))
(check-error (assertion-violation read-u8) (read-u8 (open-input-bytevector #u8(1 2 3)) 2))
(check-error (assertion-violation read-u8) (read-u8 output-file))

(check-equal 1 (peek-u8 (open-input-bytevector #u8(1 2 3 4 5))))

(check-error (assertion-violation peek-u8) (peek-u8 'port))
(check-error (assertion-violation peek-u8) (peek-u8 (open-input-string "1234")))
(check-error (assertion-violation peek-u8) (peek-u8 (open-input-bytevector #u8(1 2 3)) 2))
(check-error (assertion-violation peek-u8) (peek-u8 output-file))

(check-equal #t (u8-ready? (open-input-bytevector #u8(1 2 3 4 5))))

(check-error (assertion-violation u8-ready?) (u8-ready? 'port))
(check-error (assertion-violation u8-ready?) (u8-ready? (open-input-string "1234")))
(check-error (assertion-violation u8-ready?) (u8-ready? (open-input-bytevector #u8(1 2 3)) 2))
(check-error (assertion-violation u8-ready?) (u8-ready? output-file))

(check-equal #u8(1 2 3 4) (read-bytevector 4 (open-input-bytevector #u8(1 2 3 4 5 6 7 8))))
(check-equal #u8(1 2 3 4) (read-bytevector 8 (open-input-bytevector #u8(1 2 3 4))))

(check-error (assertion-violation read-bytevector)
        (read-bytevector -1 (open-input-bytevector #u8(1 2 3))))
(check-error (assertion-violation read-bytevector) (read-bytevector 1 'port))
(check-error (assertion-violation read-bytevector) (read-bytevector 1 (open-input-string "1234")))
(check-error (assertion-violation read-bytevector)
        (read-bytevector 1 (open-input-bytevector #u8(1 2 3)) 2))
(check-error (assertion-violation read-bytevector) (read-bytevector 1 output-file))

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

(define bv3 (make-bytevector 4 0))
(check-error (assertion-violation read-bytevector!) (read-bytevector! #(1 2 3)))
(check-error (assertion-violation read-bytevector!) (read-bytevector! bv3 'port))
(check-error (assertion-violation read-bytevector!) (read-bytevector! bv3 (current-input-port)))
(check-error (assertion-violation read-bytevector!)
    (read-bytevector! bv3 (open-input-bytevector #u8()) -1))
(check-error (assertion-violation read-bytevector!)
    (read-bytevector! bv3 (open-input-bytevector #u8()) 1 0))
(check-error (assertion-violation read-bytevector!)
    (read-bytevector! bv3 (open-input-bytevector #u8()) 1 5))

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
(check-error (assertion-violation write) (write #f input-file))
(check-error (assertion-violation write) (write #f (current-output-port) 3))
(check-error (assertion-violation write) (write #f (open-output-bytevector)))

(check-error (assertion-violation write-shared) (write-shared))
(check-error (assertion-violation write-shared) (write-shared #f input-file))
(check-error (assertion-violation write-shared) (write-shared #f (current-output-port) 3))
(check-error (assertion-violation write-shared) (write-shared #f (open-output-bytevector)))

(check-error (assertion-violation write-simple) (write-simple))
(check-error (assertion-violation write-simple) (write-simple #f input-file))
(check-error (assertion-violation write-simple) (write-simple #f (current-output-port) 3))
(check-error (assertion-violation write-simple) (write-simple #f (open-output-bytevector)))

(check-error (assertion-violation display) (display))
(check-error (assertion-violation display) (display #f input-file))
(check-error (assertion-violation display) (display #f (current-output-port) 3))
(check-error (assertion-violation display) (display #f (open-output-bytevector)))

(check-equal "\n"
    (let ((p (open-output-string)))
        (newline p)
        (get-output-string p)))

(check-error (assertion-violation newline) (newline input-file))
(check-error (assertion-violation newline) (newline (current-output-port) 2))
(check-error (assertion-violation newline) (newline (open-output-bytevector)))

(check-equal "a"
    (let ((p (open-output-string)))
        (write-char #\a p)
        (get-output-string p)))

(check-error (assertion-violation write-char) (write-char #f))
(check-error (assertion-violation write-char) (write-char #\a input-file))
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
(check-error (assertion-violation write-string) (write-string "a" input-file))
(check-error (assertion-violation write-string) (write-string "a" (open-output-bytevector)))
(check-error (assertion-violation write-string) (write-string "a" (current-output-port) -1))
(check-error (assertion-violation write-string) (write-string "a" (current-output-port) 1 0))
(check-error (assertion-violation write-string) (write-string "a" (current-output-port) 1 2))

(check-equal #u8(1)
    (let ((p (open-output-bytevector)))
        (write-u8 1 p)
        (get-output-bytevector p)))

(check-error (assertion-violation write-u8) (write-u8 #f))
(check-error (assertion-violation write-u8) (write-u8 1 input-file))
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
(check-error (assertion-violation write-bytevector) (write-bytevector #u8() input-file))
(check-error (assertion-violation write-bytevector) (write-bytevector #u8() (current-output-port)))
(check-error (assertion-violation write-bytevector)
    (write-bytevector #u8(1) (open-output-bytevector) -1))
(check-error (assertion-violation write-bytevector)
    (write-bytevector #u8(1) (open-output-bytevector) 1 0))
(check-error (assertion-violation write-bytevector)
    (write-bytevector #u8(1) (open-output-bytevector) 1 2))

(check-equal #t (begin (flush-output-port) #t))

(check-error (assertion-violation flush-output-port) (flush-output-port input-file))
(check-error (assertion-violation flush-output-port) (flush-output-port (current-output-port) 2))

;;
;; ---- system interface ----
;;

(check-equal #t
    (begin
        (load "loadtest.scm" (interaction-environment))
        (eval 'load-test (interaction-environment))))

(check-error (assertion-violation) (load))
(check-error (assertion-violation) (load '|filename|))
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

(check-error (assertion-violation exit) (exit 1 2))

(check-error (assertion-violation exit) (emergency-exit 1 2))

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

;;
;; ----------------------------------------------------------------
;;
;; Additional test cases from Seth Alves.
;;

(define powers-of-two ;; 64 bits worth
  (vector
   #x8000000000000000 #x4000000000000000 #x2000000000000000 #x1000000000000000
   #x800000000000000 #x400000000000000 #x200000000000000 #x100000000000000
   #x80000000000000 #x40000000000000 #x20000000000000 #x10000000000000
   #x8000000000000 #x4000000000000 #x2000000000000 #x1000000000000

   #x800000000000 #x400000000000 #x200000000000 #x100000000000
   #x80000000000 #x40000000000 #x20000000000 #x10000000000
   #x8000000000 #x4000000000 #x2000000000 #x1000000000
   #x800000000 #x400000000 #x200000000 #x100000000

   #x80000000 #x40000000 #x20000000 #x10000000
   #x8000000 #x4000000 #x2000000 #x1000000
   #x800000 #x400000 #x200000 #x100000
   #x80000 #x40000 #x20000 #x10000

   #x8000 #x4000 #x2000 #x1000
   #x800 #x400 #x200 #x100
   #x80 #x40 #x20 #x10
   #x8 #x4 #x2 #x1))

(check-equal #t (= (vector-ref powers-of-two 36) #x8000000))
(/ 12418 (vector-ref powers-of-two 36))
(check-equal #t (= (vector-ref powers-of-two 36) #x8000000))

(define (something i j)
  (define result '())

  (define (blerg A)
    (define (lc x)
      (vector-ref A x))
    (make-vector 18 (lc 2)))

  (define (foo A)
    (cond
     (#t '#(1 0 0 0 1 0 0 0 1 1 0 0 0 1 0 0 0 1))
     (else (blerg #f))))

  (set! result (cons 'OK1 result))
  (foo (blerg (make-vector 18 0)))
  (set! result (cons 'OK2 result))
  result
  )

(check-equal (OK2 OK1) (something 0 1))

(define (something2)
  (define result '())

  (define (product A B)
    (define (lc)
      (vector-ref A 2))
    (vector (lc) 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7 7))

  (define (power A e)
    (cond
     (#t
      '#(1 0 0 0 1 0 0 0 1 1 0 0 0 1 0 0 0 1))
     (else
      (product (power A (- e 1)) A))
     ))


  (define A (make-vector 18 0))

  (set! result (cons 'OK1 result))
  (power (product A A) 0)
  (set! result (cons 'OK2 result))
  result
  )

(check-equal (OK2 OK1) (something2))

(define (something3 i j)
  (define (lc) (vector-ref A 2))
  (define (product) (lc))
  (define (power A) (product))
  (define A (make-vector 18 0))
  (power (product)))

(check-equal 0 (something3 0 1))
