;;;
;;; Foment
;;;

(import (foment bedrock))

;;
;; ---- syntax ----
;;

;; letrec-values

;; letrec*-values


;; srfi-39
;; parameterize

(define radix (make-parameter 10))

(define write-shared (make-parameter #f
    (lambda (x)
        (if (boolean? x)
            x
            (error "only booleans are accepted by write-shared")))))

(must-equal 10 (radix))
(radix 2)
(must-equal 2 (radix))
(must-raise (assertion-violation error) (write-shared 0))

;(define prompt
;    (make-parameter 123
;        (lambda (x)
;            (if (string? x)
;                x
;                (with-output-to-string (lambda () (write x)))))))

;(prompt)       ==>  "123"
;(prompt ">")
;(prompt)       ==>  ">"

;(radix)                                              ==>  2
;(parameterize ((radix 16)) (radix))                  ==>  16
;(radix)                                              ==>  2

;(define (f n) (number->string n (radix)))

;(f 10)                                               ==>  "1010"
;(parameterize ((radix 8)) (f 10))                    ==>  "12"
;(parameterize ((radix 8) (prompt (f 10))) (prompt))  ==>  "1010"

;;
;; guardians
;;

(define g (make-guardian))
(must-equal #f (g))
(collect)
(must-equal #f (g))
(collect #t)
(must-equal #f (g))

(g (cons 'a 'b))
(must-equal #f (g))
(collect)
(must-equal (a . b) (g))

(g '#(d e f))
(must-equal #f (g))
(collect)
(must-equal #(d e f) (g))

(must-equal #f (g))
(define x '#(a b c))
(define y '#(g h i))
(collect)
(collect)
(collect #t)
(must-equal #f (g))

(define h (make-guardian))
(must-equal #f (h))
(g x)
(define x #f)
(h y)
(define y #f)
(must-equal #f (g))
(must-equal #f (h))
(collect)
(must-equal #f (g))
(must-equal #f (h))
(collect)
(must-equal #f (g))
(must-equal #f (h))
(collect #t)
(must-equal #(a b c) (g))
(must-equal #(g h i) (h))
(must-equal #f (g))
(must-equal #f (h))

(g "123")
(g "456")
(g "789")
(h #(1 2 3))
(h #(4 5 6))
(h #(7 8 9))
(collect)
(must-equal "789" (g))
(must-equal "456" (g))
(must-equal "123" (g))
(must-equal #f (g))
(collect)
(collect #t)
(must-equal #f (g))
(must-equal #(7 8 9) (h))
(must-equal #(4 5 6) (h))
(must-equal #(1 2 3) (h))
(must-equal #f (h))

; From: Guardians in a generation-based garbage collector.
; by R. Kent Dybvig, Carl Bruggeman, and David Eby.

(define G (make-guardian))
(define x (cons 'a 'b))
(G x)
(must-equal #f (G))
(set! x #f)
(collect)
(must-equal (a . b) (G))
(must-equal #f (G))

(define G (make-guardian))
(define x (cons 'a 'b))
(G x)
(G x)
(set! x #f)
(collect)
(must-equal (a . b) (G))
(must-equal (a . b) (G))
(must-equal #f (G))

(define G (make-guardian))
(define H (make-guardian))
(define x (cons 'a 'b))
(G x)
(H x)
(set! x #f)
(collect)
(must-equal (a . b) (G))
(must-equal (a . b) (H))

(define G (make-guardian))
(define H (make-guardian))
(define x (cons 'a 'b))
(G H)
(H x)
(set! x #f)
(set! H #f)
(collect)
(must-equal (a . b) ((G)))

;;
;; trackers
;;

(define t (make-tracker))
(must-equal #f (t))
(define v1 #(1 2 3))
(define v2 (cons 'a 'b))
(define r2 "(cons 'a 'b)")
(define v3 "123")
(define r3 '((a b) (c d)))
(t v1)
(t v2 r2)
(t v3 r3)
(must-equal #f (t))
(collect)
(must-equal ((a b) (c d)) (t))
(must-equal "(cons 'a 'b)" (t))
(must-equal #(1 2 3) (t))
(must-equal #f (t))
(collect)
(must-equal #f (t))

;;
;; Collector and Back References
;;

(define v (make-vector (* 1024 128)))
(collect)
(collect)
(define (fill-vector! vector idx)
    (if (< idx (vector-length vector))
        (begin
            (vector-set! vector idx (cons idx idx))
            (fill-vector! vector (+ idx 1)))))
(fill-vector! v 0)

(define (make-list idx max lst)
    (if (< idx max)
        (make-list (+ idx 1) max (cons idx lst))))
(make-list 0 (* 1024 128) '())

