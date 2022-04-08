;;;
;;; Foment
;;;

(import (foment base))

;; set!-values

(define sv1 1)
(define sv2 2)

(check-equal (a b c)
    (let ((sv3 3)) (set!-values (sv1 sv2 sv3) (values 'a 'b 'c)) (list sv1 sv2 sv3)))

(set!-values (sv1 sv2) (values 10 20))
(check-equal (10 20) (list sv1 sv2))

(set!-values () (values))

(check-equal #f (let () (set!-values () (values)) #f))

(check-error (assertion-violation) (let () (set!-values () (values 1))))

;; with-continuation-mark
;; current-continuation-marks

(define (tst)
    (with-continuation-mark 'a 1
        (with-continuation-mark 'b 2
            (let ((ret (with-continuation-mark 'c 3 (current-continuation-marks))))
                ret))))

(check-equal (((c . 3)) ((b . 2) (a . 1))) (let ((ret (tst))) ret))

(define (count n m)
    (if (= n m)
        (current-continuation-marks)
        (let ((r (with-continuation-mark 'key n (count (+ n 1) m))))
            r)))

(check-equal (((key . 3)) ((key . 2)) ((key . 1)) ((key . 0))) (count 0 4))

;; call-with-continuation-prompt
;; abort-current-continuation
;; default-prompt-tag
;; default-prompt-handler

(define rl '())
(define (st o)
    (set! rl (cons o rl)))

(define (at1)
    (call-with-continuation-prompt
        (lambda (x y) (st x) (st y)
            (dynamic-wind
                (lambda () (st 'before))
                (lambda ()
                    (st 'thunk)
                    (abort-current-continuation 'prompt-tag 'a 'b 'c)
                    (st 'oops))
                (lambda () (st 'after))))
        'prompt-tag
        (lambda (a b c) (st a) (st b) (st c))
        'x 'y)
    (reverse rl))

(check-equal (x y before thunk after a b c) (at1))

(set! rl '())
(define (at2)
    (call-with-continuation-prompt
        (lambda () (st 'proc)
            (dynamic-wind
                (lambda () (st 'before))
                (lambda () (st 'thunk) (abort-current-continuation (default-prompt-tag)
                    (lambda () (st 'handler))) (st 'oops))
                (lambda () (st 'after))))
        (default-prompt-tag)
        default-prompt-handler)
    (reverse rl))

(check-equal (proc before thunk after handler) (at2))

(set! rl '())
(define (at3)
    (call-with-continuation-prompt
        (lambda () (st 'proc)
            (dynamic-wind
                (lambda () (st 'before1))
                (lambda ()
                    (st 'thunk1)
                    (dynamic-wind
                        (lambda () (st 'before2))
                        (lambda ()
                            (st 'thunk2)
                            (abort-current-continuation (default-prompt-tag)
                                (lambda () (st 'handler)))
                            (st 'oops))
                        (lambda () (st 'after2))))
                (lambda () (st 'after1))))
        (default-prompt-tag)
        default-prompt-handler)
    (reverse rl))

(check-equal (proc before1 thunk1 before2 thunk2 after2 after1 handler) (at3))

;; srfi-39
;; parameterize

(define radix (make-parameter 10))

(define boolean-parameter (make-parameter #f
    (lambda (x)
        (if (boolean? x)
            x
            (error "only booleans are accepted by boolean-parameter")))))

(check-equal 10 (radix))
(radix 2)
(check-equal 2 (radix))
(check-error (assertion-violation error) (boolean-parameter 0))

(check-equal 16 (parameterize ((radix 16)) (radix)))
(check-equal 2 (radix))

(define prompt
    (make-parameter 123
        (lambda (x)
            (if (string? x)
                x
                (number->string x 10)))))

(check-equal "123" (prompt))
(prompt ">")
(check-equal ">" (prompt))

(define (f n) (number->string n (radix)))

(check-equal "1010" (f 10))
(check-equal "12" (parameterize ((radix 8)) (f 10)))
(check-equal "1010" (parameterize ((radix 8) (prompt (f 10))) (prompt)))

(define p1 (make-parameter 10))
(define p2 (make-parameter 20))

(check-equal 10 (p1))
(check-equal 20 (p2))
(p1 100)
(check-equal 100 (p1))

(check-equal 1000 (parameterize ((p1 1000) (p2 200)) (p1)))
(check-equal 100 (p1))
(check-equal 20 (p2))
(check-equal 1000 (parameterize ((p1 10) (p2 200)) (p1 1000) (p1)))
(check-equal 100 (p1))
(check-equal 20 (p2))
(check-equal 1000 (parameterize ((p1 0)) (p1) (parameterize ((p2 200)) (p1 1000) (p1))))

(define *k* #f)
(define p (make-parameter 1))
(define (tst)
    (parameterize ((p 10))
        (if (call/cc (lambda (k) (set! *k* k) #t))
            (p 100)
            #f)
        (p)))

(check-equal 1 (p))
(tst)
;(check-equal 100 (tst))
(check-equal 1 (p))
(check-equal 10 (*k* #f))
(check-equal 1 (p))

(define *k2* #f)
(define p2 (make-parameter 2))
(define (tst2)
    (parameterize ((p2 20))
        (call/cc (lambda (k) (set! *k2* k)))
        (p2)))

(check-equal 2 (p2))
(check-equal 20 (tst2))
(check-equal 2 (p2))
(check-equal 20 (*k2*))
(check-equal 2 (p2))

;;
;; guardians
;;

(test-when (memq 'guardians (features))
    (collect #t)
    (collect #t)
    (collect #t)
    (collect #t)

    (define g (make-guardian))
    (check-equal #f (g))
    (collect)
    (check-equal #f (g))
    (collect #t)
    (check-equal #f (g))

    (g (cons 'a 'b))
    (check-equal #f (g))
    (collect)
    (check-equal (a . b) (g))

    (g '#(d e f))
    (check-equal #f (g))
    (collect)
    (check-equal #(d e f) (g))

    (check-equal #f (g))
    (define x '#(a b c))
    (define y '#(g h i))
    (collect)
    (collect)
    (collect #t)
    (check-equal #f (g))

    (collect #t)
    (define h (make-guardian))
    (check-equal #f (h))
    (g x)
    (define x #f)
    (h y)
    (define y #f)
    (check-equal #f (g))
    (check-equal #f (h))
    (collect #t)
    (check-equal #(a b c) (g))
    (check-equal #(g h i) (h))
    (check-equal #f (g))
    (check-equal #f (h))

    (g (string #\1 #\2 #\3))
    (g (string #\4 #\5 #\6))
    (g (string #\7 #\8 #\9))
    (h #(1 2 3))
    (h #(4 5 6))
    (h #(7 8 9))
    (collect)
    (check-equal #t (let ((v (g))) (or (equal? v "789") (equal? v "456") (equal? v "123"))))
    (check-equal #t (let ((v (g))) (or (equal? v "789") (equal? v "456") (equal? v "123"))))
    (check-equal #t (let ((v (g))) (or (equal? v "789") (equal? v "456") (equal? v "123"))))
    (check-equal #f (g))
    (collect)
    (collect #t)
    (check-equal #f (g))
    (check-equal #t
        (let ((v (h))) (or (equal? v #(1 2 3)) (equal? v #(4 5 6)) (equal? v #(7 8 9)))))
    (check-equal #t
        (let ((v (h))) (or (equal? v #(1 2 3)) (equal? v #(4 5 6)) (equal? v #(7 8 9)))))
    (check-equal #t
        (let ((v (h))) (or (equal? v #(1 2 3)) (equal? v #(4 5 6)) (equal? v #(7 8 9)))))
    (check-equal #f (h)))

; From: Guardians in a generation-based garbage collector.
; by R. Kent Dybvig, Carl Bruggeman, and David Eby.

(test-when (memq 'guardians (features))
    (define G (make-guardian))
    (define x (cons 'a 'b))
    (G x)
    (check-equal #f (G))
    (set! x #f)
    (collect)
    (check-equal (a . b) (G))
    (check-equal #f (G))

    (define G (make-guardian))
    (define x (cons 'a 'b))
    (G x)
    (G x)
    (set! x #f)
    (collect)
    (check-equal (a . b) (G))
    (check-equal (a . b) (G))
    (check-equal #f (G))

    (define G (make-guardian))
    (define H (make-guardian))
    (define x (cons 'a 'b))
    (G x)
    (H x)
    (set! x #f)
    (collect)
    (check-equal (a . b) (G))
    (check-equal (a . b) (H))

    (define G (make-guardian))
    (define H (make-guardian))
    (define x (cons 'a 'b))
    (G H)
    (H x)
    (set! x #f)
    (set! H #f)
    (collect)
    (check-equal (a . b) ((G))))

;;
;; trackers
;;

(test-when (memq 'trackers (features))
    (define t (make-tracker))
    (check-equal #f (t))
    (define v1 #(1 2 3))
    (define v2 (cons 'a 'b))
    (define r2 "(cons 'a 'b)")
    (define v3 "123")
    (define r3 '((a b) (c d)))
    (t v1)
    (t v2 r2)
    (t v3 r3)
    (check-equal #f (t))
    (collect)
    (check-equal #(1 2 3) (t))
    (check-equal "(cons 'a 'b)" (t))
    (check-equal ((a b) (c d)) (t))
    (check-equal #f (t))
    (collect)
    (check-equal #f (t)))

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

(define (make-big-list idx max lst)
    (if (< idx max)
        (make-big-list (+ idx 1) max (cons idx lst))))
(make-big-list 0 (* 1024 128) '())

;;
;; threads
;;

(define e (make-exclusive))
(define c (make-condition))
(define t (current-thread))
(check-equal #t (eq? t (current-thread)))

(run-thread
    (lambda ()
        (sleep 100)
        (enter-exclusive e)
        (set! t (current-thread))
        (leave-exclusive e)
        (condition-wake c)))

(enter-exclusive e)
(condition-wait c e)
(leave-exclusive e)

(check-equal #f (eq? t (current-thread)))
(check-equal #t (thread? t))
(check-equal #t (thread? (current-thread)))
(check-equal #f (thread? e))
(check-equal #f (thread? c))

(check-error (assertion-violation current-thread) (current-thread #t))

(check-error (assertion-violation thread?) (thread?))
(check-error (assertion-violation thread?) (thread? #t #t))

(check-error (assertion-violation run-thread) (run-thread))
(check-error (assertion-violation run-thread) (run-thread #t))
(check-error (assertion-violation run-thread) (run-thread + #t))
(check-error (assertion-violation run-thread) (run-thread (lambda () (+ 1 2 3)) #t))

(define unwound-it #f)

(run-thread
    (lambda ()
        (dynamic-wind
            (lambda () 'nothing)
            (lambda () (exit-thread #t))
            (lambda () (set! unwound-it #t)))))

(sleep 10)
(check-equal #t unwound-it)

(check-error (assertion-violation sleep) (sleep))
(check-error (assertion-violation sleep) (sleep #t))
(check-error (assertion-violation sleep) (sleep 1 #t))
(check-error (assertion-violation sleep) (sleep -1))

(check-equal #t (exclusive? e))
(check-equal #t (exclusive? (make-exclusive)))
(check-equal #f (exclusive? #t))
(check-error (assertion-violation exclusive?) (exclusive?))
(check-error (assertion-violation exclusive?) (exclusive? #t #t))

(check-error (assertion-violation make-exclusive) (make-exclusive #t))

(check-error (assertion-violation enter-exclusive) (enter-exclusive #t))
(check-error (assertion-violation enter-exclusive) (enter-exclusive))
(check-error (assertion-violation enter-exclusive) (enter-exclusive c))
(check-error (assertion-violation enter-exclusive) (enter-exclusive e #t))

(check-error (assertion-violation leave-exclusive) (leave-exclusive #t))
(check-error (assertion-violation leave-exclusive) (leave-exclusive))
(check-error (assertion-violation leave-exclusive) (leave-exclusive c))
(check-error (assertion-violation leave-exclusive) (leave-exclusive e #t))

(check-error (assertion-violation try-exclusive) (try-exclusive #t))
(check-error (assertion-violation try-exclusive) (try-exclusive))
(check-error (assertion-violation try-exclusive) (try-exclusive c))
(check-error (assertion-violation try-exclusive) (try-exclusive e #t))

(define te (make-exclusive))
(check-equal #t (try-exclusive te))
(leave-exclusive te)

(run-thread (lambda () (enter-exclusive te) (sleep 1000) (leave-exclusive te)))
(sleep 100)

(check-equal #f (try-exclusive te))

(check-equal #t (condition? c))
(check-equal #t (condition? (make-condition)))
(check-equal #f (condition? #t))
(check-error (assertion-violation condition?) (condition?))
(check-error (assertion-violation condition?) (condition? #t #t))

(check-error (assertion-violation make-condition) (make-condition #t))

(check-error (assertion-violation condition-wait) (condition-wait #t))
(check-error (assertion-violation condition-wait) (condition-wait c #t))
(check-error (assertion-violation condition-wait) (condition-wait #t e))
(check-error (assertion-violation condition-wait) (condition-wait c e #t))
(check-error (assertion-violation condition-wait) (condition-wait e c))

(check-error (assertion-violation condition-wake) (condition-wake #t))
(check-error (assertion-violation condition-wake) (condition-wake c #t))
(check-error (assertion-violation condition-wake) (condition-wake e))

(check-error (assertion-violation condition-wake-all) (condition-wake-all #t))
(check-error (assertion-violation condition-wake-all) (condition-wake-all c #t))
(check-error (assertion-violation condition-wake-all) (condition-wake-all e))

;; r7rs-letrec

(define-syntax r7rs-letrec
    (syntax-rules ()
        ((r7rs-letrec ((var1 init1) ...) body ...)
            (r7rs-letrec "generate temp names" (var1 ...) () ((var1 init1) ...) body ...))
        ((r7rs-letrec "generate temp names" () (temp1 ...) ((var1 init1) ...) body ...)
            (let ((var1 (no-value)) ...)
                (let ((temp1 init1) ...)
                    (set! var1 temp1) ...
                    body ...)))
        ((r7rs-letrec "generate temp names" (x y ...) (temp ...) ((var1 init1) ...) body ...)
            (r7rs-letrec "generate temp names" (y ...) (newtemp temp ...) ((var1 init1) ...)
                    (let () body ...)))))

(check-equal #t (r7rs-letrec ((even? (lambda (n) (if (zero? n) #t (odd? (- n 1)))))
                        (odd? (lambda (n) (if (zero? n) #f (even? (- n 1))))))
                    (even? 88)))

(check-equal 0 (let ((cont #f))
        (r7rs-letrec ((x (call-with-current-continuation (lambda (c) (set! cont c) 0)))
                     (y (call-with-current-continuation (lambda (c) (set! cont c) 0))))
              (if cont
                  (let ((c cont))
                      (set! cont #f)
                      (set! x 1)
                      (set! y 1)
                      (c 0))
                  (+ x y)))))

(check-equal #t
    (r7rs-letrec ((x (call/cc list)) (y (call/cc list)))
        (cond ((procedure? x) (x (pair? y)))
            ((procedure? y) (y (pair? x))))
            (let ((x (car x)) (y (car y)))
                (and (call/cc x) (call/cc y) (call/cc x)))))

(check-equal #t
    (r7rs-letrec ((x (call-with-current-continuation (lambda (c) (list #t c)))))
        (if (car x)
            ((cadr x) (list #f (lambda () x)))
            (eq? x ((cadr x))))))

(check-syntax (syntax-violation syntax-rules) (r7rs-letrec))
(check-syntax (syntax-violation syntax-rules) (r7rs-letrec (x 2) x))
(check-syntax (syntax-violation syntax-rules) (r7rs-letrec x x))
(check-syntax (syntax-violation syntax-rules) (r7rs-letrec ((x)) x))
(check-syntax (syntax-violation syntax-rules) (r7rs-letrec ((x) 2) x))
(check-syntax (syntax-violation syntax-rules) (r7rs-letrec ((x 2) y) x))
(check-syntax (syntax-violation syntax-rules) (r7rs-letrec ((x 2) . y) x))
(check-syntax (syntax-violation let) (r7rs-letrec ((x 2) (x 3)) x))
(check-syntax (syntax-violation let) (r7rs-letrec ((x 2) (y 1) (x 3)) x))
;(check-syntax (syntax-violation syntax-rules) (r7rs-letrec ((x 2))))
(check-syntax (syntax-violation syntax-rules) (r7rs-letrec ((x 2)) . x))
(check-syntax (syntax-violation syntax-rules) (r7rs-letrec ((x 2)) y . x))
(check-syntax (syntax-violation let) (r7rs-letrec (((x y z) 2)) y x))
(check-syntax (syntax-violation let) (r7rs-letrec ((x 2) ("y" 3)) y))

;;
;; file system api
;;

(check-equal #t (= (cond-expand (windows 127) (else 122))
    (file-size "lib-a-b-c.sld")))
(check-error (assertion-violation file-size) (file-size))
(check-error (assertion-violation file-size) (file-size 'not-actually-a-filename))
(check-error (assertion-violation file-size) (file-size "not actually a filename"))
(check-error (assertion-violation file-size) (file-size "not actually" "a filename"))

(check-equal #t (file-regular? "foment.scm"))
(check-equal #f (file-regular? "lib"))
(check-equal #f (file-regular? "not actually a filename"))
(check-error (assertion-violation file-regular?) (file-regular?))
(check-error (assertion-violation file-regular?) (file-regular? 'not-a-filename))
(check-error (assertion-violation file-regular?) (file-regular? "not a filename" "just not"))

(check-equal #f (file-directory? "foment.scm"))
(check-equal #t (file-directory? "lib"))
(check-equal #f (file-directory? "not actually a filename"))
(check-error (assertion-violation file-directory?) (file-directory?))
(check-error (assertion-violation file-directory?) (file-directory? 'not-a-filename))
(check-error (assertion-violation file-directory?) (file-directory? "not a filename" "just not"))

(check-equal #t (file-readable? "foment.scm"))
(check-equal #f (file-readable? "not a file"))
(check-error (assertion-violation file-readable?) (file-readable?))
(check-error (assertion-violation file-readable?) (file-readable? 'not-a-filename))
(check-error (assertion-violation file-readable?) (file-readable? "not a filename" "just not"))

(check-equal #t (file-writable? "foment.scm"))
(check-equal #f (file-writable? "not a file"))
(check-error (assertion-violation file-writable?) (file-writable?))
(check-error (assertion-violation file-writable?) (file-writable? 'not-a-filename))
(check-error (assertion-violation file-writable?) (file-writable? "not a filename" "just not"))

(check-error (assertion-violation create-symbolic-link) (create-symbolic-link "not a filename"))
(check-error (assertion-violation create-symbolic-link) (create-symbolic-link "not a" 'filename))
(check-error (assertion-violation create-symbolic-link)
    (create-symbolic-link "not a filename" "just not" "a filename"))

(call-with-output-file "rename.me" (lambda (p) (write "all good" p) (newline p)))
(if (file-regular? "output.renamed")
    (delete-file "output.renamed"))
(check-equal #f (file-regular? "output.renamed"))
(rename-file "rename.me" "output.renamed")
(check-equal #t (file-regular? "output.renamed"))

(call-with-output-file "output.overwritten" (lambda (p) (write "all bad" p) (newline p)))
(check-equal #t (file-regular? "output.overwritten"))
(rename-file "output.renamed" "output.overwritten")
(check-equal #t (file-regular? "output.overwritten"))
(check-equal "all good" (call-with-input-file "output.overwritten" (lambda (p) (read p))))

(check-error (assertion-violation rename-file) (rename-file "not a filename"))
(check-error (assertion-violation rename-file) (rename-file "not a" 'filename))
(check-error (assertion-violation rename-file)
    (rename-file "not a filename" "just not" "a filename"))

(if (file-directory? "testdirectory")
    (delete-directory "testdirectory"))

(create-directory "testdirectory")
(check-equal #t (file-directory? "testdirectory"))
(delete-directory "testdirectory")
(check-equal #f (file-directory? "testdirectory"))

(check-error (assertion-violation create-directory) (create-directory))
(check-error (assertion-violation create-directory) (create-directory 'not-a-filename))
(check-error (assertion-violation create-directory) (create-directory "not a filename" "just not"))

(check-error (assertion-violation delete-directory) (delete-directory))
(check-error (assertion-violation delete-directory) (delete-directory 'not-a-filename))
(check-error (assertion-violation delete-directory) (delete-directory "not a filename" "just not"))

(check-equal "foment.scm" (car (member "foment.scm" (list-directory "."))))
(check-equal #f (member "not a filename" (list-directory ".")))
(check-equal "test" (car (member "test" (list-directory ".."))))
(check-error (assertion-violation list-directory) (list-directory))
(check-error (assertion-violation list-directory) (list-directory 'not-a-filename))
(check-error (assertion-violation list-directory) (list-directory "not a filename" "just not"))

(check-error (assertion-violation current-directory) (current-directory 'not-a-filename))
(check-error (assertion-violation current-directory) (current-directory "not-a-filename"))
(check-error (assertion-violation current-directory) (current-directory ".." "not a filename"))

;;
;; port positioning
;;

(define obp (open-binary-output-file "output4.txt"))
(define ibp (open-binary-input-file "input.txt"))

(check-equal #t (port-has-port-position? ibp))
(check-equal #f (port-has-port-position? (current-output-port)))
(check-equal #t (port-has-port-position? obp))
(check-equal #f (port-has-port-position? (current-input-port)))
(check-equal #t (eq? port-has-port-position? port-has-set-port-position!?))

(check-error (assertion-violation positioning-port?) (port-has-port-position?))
(check-error (assertion-violation positioning-port?) (port-has-port-position? ibp obp))
(check-error (assertion-violation positioning-port?) (port-has-port-position? 'port))

(check-equal 0 (port-position ibp))
(check-equal 97 (read-u8 ibp)) ;; #\a
(check-equal 1 (port-position ibp))
(set-port-position! ibp 25 'current)
(check-equal 26 (port-position ibp))
(set-port-position! ibp 0 'end)
(check-equal 52 (port-position ibp))

(set-port-position! ibp 22 'begin)
(check-equal 22 (port-position ibp))
(set-port-position! ibp -10 'current)
(check-equal 12 (port-position ibp))
(check-equal 109 (read-u8 ibp)) ;; #\m
(check-equal 13 (port-position ibp))

(set-port-position! ibp -1 'end)
(check-equal 90 (read-u8 ibp)) ;; #\Z
(check-equal 52 (port-position ibp))

(check-equal 0 (port-position obp))
(write-bytevector #u8(0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0) obp)
(set-port-position! obp -1 'end)
(write-u8 123 obp)
(set-port-position! obp 10 'begin)
(write-u8 34 obp)
(set-port-position! obp 1 'current)
(write-u8 56 obp)
(close-port obp)

(define obp (open-binary-input-file "output4.txt"))
(check-equal #u8(0 1 2 3 4 5 6 7 8 9 34 1 56 3 4 5 6 7 8 9 123) (read-bytevector 1024 obp))

;;
;; ---- SRFI 124: Ephemerons ----
;;

(import (scheme ephemeron))

(test-when (not (eq? (cdr (assq 'collector (config))) 'none))

    (check-equal #f (ephemeron? (cons 1 2)))
    (check-equal #f (ephemeron? (box 1)))
    (check-equal #f (ephemeron? #(1 2)))
    (check-equal #t (ephemeron? (make-ephemeron 'abc (cons 1 2))))

    (define e1 (make-ephemeron (cons 1 2) (cons 3 4)))
    (collect)

    (check-equal #t (ephemeron-broken? e1))
    (check-equal #f (ephemeron-broken? (make-ephemeron 1 2)))
    (check-error (assertion-violation ephemeron-broken?) (ephemeron-broken? (cons 1 2)))

    (check-equal (1 . 2) (ephemeron-key (make-ephemeron (cons 1 2) (cons 3 4))))
    (check-equal #f (ephemeron-key e1))
    (check-error (assertion-violation ephemeron-key) (ephemeron-key (cons 1 2)))

    (check-equal (3 . 4) (ephemeron-datum (make-ephemeron (cons 1 2) (cons 3 4))))
    (check-equal #f (ephemeron-datum e1))
    (check-error (assertion-violation ephemeron-datum) (ephemeron-datum (cons 1 2)))

    (define e2 (make-ephemeron (cons 1 2) (cons 3 4)))

    (set-ephemeron-key! e2 (cons 'a 'b))
    (check-equal (a . b) (ephemeron-key e2))
    (check-error (assertion-violation set-ephemeron-key!) (set-ephemeron-key! (cons 1 2) 'a))
    (check-equal #f (ephemeron-key e1))
    (check-equal #t (ephemeron-broken? e1))
    (set-ephemeron-key! e1 'abc)
    (check-equal #f (ephemeron-key e1))

    (set-ephemeron-datum! e2 (cons 'c 'd))
    (check-equal (c . d) (ephemeron-datum e2))
    (check-error (assertion-violation set-ephemeron-datum!) (set-ephemeron-datum! (cons 1 2) 'a))
    (check-equal #f (ephemeron-datum e1))
    (check-equal #t (ephemeron-broken? e1))
    (set-ephemeron-datum! e1 'def)
    (check-equal #f (ephemeron-datum e1))

    (define (ephemeron-list key cnt)
        (define (eph-list cnt)
            (if (= cnt 0)
                (list (make-ephemeron (cons 'key cnt) (list 'end)))
                (let ((lst (eph-list (- cnt 1))))
                    (cons (make-ephemeron (cons 'key cnt) (ephemeron-key (car lst))) lst))))
        (let ((lst (eph-list cnt)))
            (cons (make-ephemeron key (ephemeron-key (car lst))) lst)))

    (define (ephemeron-list cnt lst)
        (if (= cnt 0)
            lst
            (ephemeron-list (- cnt 1)
                    (cons (make-ephemeron (cons 'key cnt) (cons 'datum cnt)) lst))))
    (define (forward-chain lst key)
        (if (pair? lst)
            (begin
                (set-ephemeron-key! (car lst) key)
                (forward-chain (cdr lst) (ephemeron-datum (car lst))))))
    (define (backward-chain lst key)
        (if (pair? lst)
            (let ((key (backward-chain (cdr lst) key)))
                (set-ephemeron-key! (car lst) key)
                (ephemeron-datum (car lst)))
            key))
    (define (count-broken lst)
        (if (pair? lst)
            (if (ephemeron-broken? (car lst))
                (+ (count-broken (cdr lst)) 1)
                (count-broken (cdr lst)))
            0))

    (define ekey1 (cons 'ekey1 'ekey1))
    (define e3 (make-ephemeron ekey1 (cons 3 3)))
    (define e4 (make-ephemeron ekey1 (cons 4 4)))
    (collect)
    (set! ekey1 #f)
    (collect)

    (define ekey2 (cons 'ekey2 'ekey2))
    (define blst (ephemeron-list 1000 '()))
    (backward-chain blst ekey2)
    (define flst (ephemeron-list 1000 '()))
    (backward-chain flst ekey2)

    (collect)
    (set! ekey2 #f)
    (collect)
    (collect)
    (check-equal 1000 (count-broken blst))
    (check-equal 1000 (count-broken flst)))

;;
;; Number tests
;;

(check-equal 18446744073709551615 (string->number "#xFFFFFFFFFFFFFFFF"))
(check-equal 18446744073709551600 (* (string->number "#xFFFFFFFFFFFFFFF") 16))
(check-equal -18446744073709551600 (* (string->number "#xFFFFFFFFFFFFFFF") -16))
(check-equal 18446462598732775425 (* (string->number "#xFFFFFFFFFFFF") #xFFFF))

;;
;; %execute-proc
;;

(check-equal zero (%execute-proc (lambda () 'zero)))

(define test-execute-global '())
(define (test-execute-zero) 'zero)
(define (test-execute-zero-set) (set! test-execute-global 'zero) (list 'zero))

(check-equal zero (%execute-proc test-execute-zero))
(check-equal (zero) (%execute-proc test-execute-zero-set))
(check-equal zero test-execute-global)

(define (test-execute-one arg1) (list 'one arg1))

(check-equal (one 1) (%execute-proc test-execute-one 1))

(define (test-execute-two arg1 arg2) (set! test-execute-global 'two) (+ arg1 arg2))

(check-equal 3 (%execute-proc test-execute-two 1 2))
(check-equal two test-execute-global)

(define (test-execute-three arg1 arg2 arg3)
    (let ((total (+ arg1 arg2 arg3)))
        (set! test-execute-global 'three)
        total))

(check-equal 6 (%execute-proc test-execute-three 1 2 3))
(check-equal three test-execute-global)

(check-error (implementation-restriction call-with-current-continuation)
    (%execute-proc (lambda () (call/cc (lambda (k) k)))))

(define (test-execute-call/cc k a1 a2)
    (let ((total (+ a1 a2)))
        (k total)))

(check-equal 30
    (call/cc
        (lambda (k)
            (%execute-proc test-execute-call/cc k 10 20)
            'call/cc)))

;;
;; Custom binary and textual ports
;;

;; Tests from github.comm/scheme-requests-for-implementation/srfi-192

; To avoid importing srfi-1
(define (list-tabulate n proc)
    (define (tabulate i)
        (if (< i n)
            (cons (proc i) (tabulate (+ i 1)))
            '()))
    (tabulate 0))

; binary input, no port positioning
(define data (apply bytevector (list-tabulate 1000 (lambda (i) (modulo i 256)))))
(define pos 0)
(define closed #f)
(define p (make-custom-binary-input-port "binary-input"
    (lambda (buf start count)   ; read!
        (let ((size (min count (- (bytevector-length data) pos))))
            (bytevector-copy! buf start data pos (+ pos size))
            (set! pos (+ pos size))
            size))
    #f                          ; get-position
    #f                          ; set-position
    (lambda () (set! closed #t)) ; close
    ))

(check-equal #t (port? p))
(check-equal #t (binary-port? p))
(check-equal #f (textual-port? p))
(check-equal #t (input-port? p))
(check-equal #f (output-port? p))
(check-equal #f (port-has-port-position? p))
(check-equal #f (port-has-set-port-position!? p))

(check-equal 0 (read-u8 p))
(check-equal 1 (read-u8 p))
(check-equal 2 (peek-u8 p))
(check-equal 2 (read-u8 p))
(check-equal #t (equal? (bytevector-copy data 3) (read-bytevector 997 p)))

(check-equal #t (eq? (eof-object) (read-u8 p)))
(check-equal #t (begin (close-port p) closed))

; binary input, with port positioning
(define pos 0)
(define saved-pos #f)
(define saved-pos2 #f)
(define closed #f)
(define p (make-custom-binary-input-port "binary-input"
    (lambda (buf start count)   ; read!
        (let ((size (min count (- (bytevector-length data) pos))))
            (bytevector-copy! buf start data pos (+ pos size))
            (set! pos (+ pos size))
            size))
    (lambda () pos)             ; get-position
    (lambda (k) (set! pos k))   ; set-position
    (lambda () (set! closed #t)) ;close
    ))

(check-equal #t (port? p))
(check-equal #t (binary-port? p))
(check-equal #f (textual-port? p))
(check-equal #t (input-port? p))
(check-equal #f (output-port? p))
(check-equal #t (port-has-port-position? p))
(check-equal #t (port-has-set-port-position!? p))

(check-equal 0 (read-u8 p))
(check-equal 1 (read-u8 p))
(check-equal 2 (peek-u8 p))
(set! saved-pos (port-position p))
(check-equal 2 (read-u8 p))
(set! saved-pos2 (port-position p))

(check-equal #t (equal? (bytevector-copy data 3) (read-bytevector 997 p)))

(check-equal #t (eq? (eof-object) (read-u8 p)))

(set-port-position! p saved-pos)
(check-equal 2 (read-u8 p))

(set-port-position! p saved-pos2)
(check-equal 3 (read-u8 p))

(check-equal #t (begin (close-port p) closed))

; binary output, with port positioning
(define sink (make-vector 2000 #f))
(define pos 0)
(define saved-pos #f)
(define closed #f)
(define flushed #f)
(define p (make-custom-binary-output-port "binary-output"
    (lambda (buf start count)   ;write!
        (do ((i start (+ i 1))
                (j pos (+ j 1)))
            ((>= i (+ start count)) (set! pos j))
            (vector-set! sink j (bytevector-u8-ref buf i)))
        count)
    (lambda () pos)             ;get-position
    (lambda (k) (set! pos k))   ;set-position!
    (lambda () (set! closed #t)) ; close
    (lambda () (set! flushed #t)) ; flush
    ))

(check-equal #t (port? p))
(check-equal #t (binary-port? p))
(check-equal #f (textual-port? p))
(check-equal #f (input-port? p))
(check-equal #t (output-port? p))
(check-equal #t (port-has-port-position? p))
(check-equal #t (port-has-set-port-position!? p))

(write-u8 3 p)
(write-u8 1 p)
(write-u8 4 p)
(flush-output-port p)
(check-equal #t flushed)

(set! saved-pos (port-position p))

(check-equal #(3 1 4) (vector-copy sink 0 pos))

(write-bytevector '#u8(1 5 9 2 6) p)
(flush-output-port p)
(check-equal #(3 1 4 1 5 9 2 6) (vector-copy sink 0 pos))

(set-port-position! p saved-pos)
(for-each (lambda (b) (write-u8 b p)) '(5 3 5))
(flush-output-port p)
(check-equal #(3 1 4 5 3 5) (vector-copy sink 0 pos))
(check-equal #(3 1 4 5 3 5 2 6) (vector-copy sink 0 (+ pos 2)))

(check-equal #t (begin (close-port p) closed))

; binary input/output, with port positioning
(define original-size 500)             ; writing may extend the size
(define pos 0)
(define saved-pos #f)
(define flushed #f)
(define closed #f)
(define p (make-custom-binary-input/output-port "binary i/o"
    (lambda (buf start count)   ; read!
        (let ((size (min count (- original-size pos))))
                (bytevector-copy! buf start data pos (+ pos size))
                (set! pos (+ pos size))
            size))
    (lambda (buf start count)   ;write!
        (let ((size (min count (- (bytevector-length data) pos))))
                (bytevector-copy! data pos buf start (+ start size))
                (set! pos (+ pos size))
                (set! original-size (max original-size pos))
            size))
    (lambda () pos)             ;get-position
    (lambda (k) (set! pos k))   ;set-position!
    (lambda () (set! closed #t)) ; close
    (lambda () (set! flushed #t)) ; flush
    ))

(check-equal #t (port? p))
(check-equal #t (binary-port? p))
(check-equal #f (textual-port? p))
(check-equal #t (input-port? p))
(check-equal #t (output-port? p))
(check-equal #t (port-has-port-position? p))
(check-equal #t (port-has-set-port-position!? p))

(check-equal 0 (read-u8 p))
(check-equal 1 (read-u8 p))
(check-equal 2 (read-u8 p))
(set! saved-pos (port-position p))
(check-equal #t (equal? (bytevector-copy data 3 original-size) (read-bytevector 997 p)))

(write-bytevector '#u8(255 255 255) p)
(check-equal #u8(255 255 255) (bytevector-copy data 500 503))
(write-u8 254 p)
(check-equal #u8(255 255 255 254) (bytevector-copy data 500 504))
(write-u8 254 p)

(check-equal #t (eq? (eof-object) (read-u8 p)))

(set-port-position! p saved-pos)
(check-equal 3 (peek-u8 p))

(write-u8 100 p)
(set-port-position! p saved-pos)
(check-equal 100 (read-u8 p))
(check-equal 4 (read-u8 p))

(define (string-tabulate n proc)
    (define s (make-string n))
    (define (tabulate i)
        (if (< i n)
            (begin
                (string-set! s i (proc i))
                (tabulate (+ i 1)))
            s))
    (tabulate 0))

; textual input, no port positioning
(define data (string-tabulate 1000 (lambda (i) (integer->char (+ #x3000 i)))))

(define pos 0)
(define closed #f)
(define p (make-custom-textual-input-port "textual-input"
    (lambda (buf start count)   ; read!
        (let ((size (min count (- (string-length data) pos))))
            (unless (zero? size)
                (if (string? buf)
                    (begin
                        (string-copy! buf start data pos (+ pos size))
                        (set! pos (+ pos size)))
                    (do ((i 0 (+ i 1))
                            (j pos (+ j 1)))
                        ((= i size) (set! pos j))
                    (vector-set! buf (+ start i) (string-ref data j)))))
            size))
    #f                          ; get-position
    #f                          ; set-position
    (lambda () (set! closed #t)); close
    ))

(check-equal #t (port? p))
(check-equal #f (binary-port? p))
(check-equal #t (textual-port? p))
(check-equal #t (input-port? p))
(check-equal #f (output-port? p))
(check-equal #f (port-has-port-position? p))
(check-equal #f (port-has-set-port-position!? p))

(check-equal #t (eqv? (string-ref data 0) (read-char p)))
(check-equal #t (eqv? (string-ref data 1) (read-char p)))
(check-equal #t (eqv? (string-ref data 2) (peek-char p)))
(check-equal #t (eqv? (string-ref data 2) (read-char p)))

(check-equal #t (equal? (string-copy data 3) (read-string 997 p)))
(check-equal #t (equal? (eof-object) (read-char p)))

(close-port p)
(check-equal #t closed)

; textual input, port positioning
(define pos 0)
(define saved-pos #f)
(define closed #f)
(define p (make-custom-textual-input-port "textual-input"
    (lambda (buf start count)   ; read!
        (let ((size (min count (- (string-length data) pos))))
            (unless (zero? size)
                (if (string? buf)
                    (begin
                        (string-copy! buf start data pos (+ pos size))
                        (set! pos (+ pos size)))
                    (do ((i 0 (+ i 1))
                            (j pos (+ j 1)))
                        ((= i size) (set! pos j))
                    (vector-set! buf (+ start i) (string-ref data j)))))
            size))
    (lambda () pos)             ; get-position
    (lambda (k) (set! pos k))   ; set-position
    (lambda () (set! closed #t)); close
    ))

(check-equal #t (port? p))
(check-equal #f (binary-port? p))
(check-equal #t (textual-port? p))
(check-equal #t (input-port? p))
(check-equal #f (output-port? p))
(check-equal #t (port-has-port-position? p))
(check-equal #t (port-has-set-port-position!? p))

(check-equal #t (eqv? (string-ref data 0) (read-char p)))
(check-equal #t (eqv? (string-ref data 1) (read-char p)))
(check-equal #t (eqv? (string-ref data 2) (peek-char p)))
(set! saved-pos (port-position p))
(check-equal #t (eqv? (string-ref data 2) (read-char p)))
(check-equal #t (eqv? (string-ref data 3) (peek-char p)))

(check-equal #t (equal? (string-copy data 3) (read-string 997 p)))
(check-equal #t (equal? (eof-object) (read-char p)))

(set-port-position! p saved-pos)
(check-equal #t (eqv? (string-ref data 2) (peek-char p)))

(close-port p)
(check-equal #t closed)

; textual output, port positioning
(define data (apply bytevector (list-tabulate 1000 (lambda (i) (modulo i 256)))))
(define sink (make-vector 2000 #f))
(define pos 0)
(define saved-pos #f)
(define closed #f)
(define flushed #f)
(define p (make-custom-textual-output-port "textual-output"
    (lambda (buf start count)   ;write!
        (do ((i start (+ i 1))
                (j pos (+ j 1)))
            ((>= i (+ start count)) (set! pos j))
            (vector-set! sink j (if (string? buf) (string-ref buf i) (vector-ref buf i))))
        count)
    (lambda () pos)             ;get-position
    (lambda (k) (set! pos k))   ;set-position!
    (lambda () (set! closed #t)) ; close
    (lambda () (set! flushed #t)) ; flush
    ))

(check-equal #t (port? p))
(check-equal #f (binary-port? p))
(check-equal #t (textual-port? p))
(check-equal #f (input-port? p))
(check-equal #t (output-port? p))
(check-equal #t (port-has-port-position? p))
(check-equal #t (port-has-set-port-position!? p))

(write-char #\a p)
(write-char #\b p)
(write-char #\c p)
(flush-output-port p)
(check-equal #t flushed)
(set! saved-pos (port-position p))
(check-equal #(#\a #\b #\c) (vector-copy sink 0 pos))

(write-string "Quack" p)
(flush-output-port p)
(check-equal #(#\a #\b #\c #\Q #\u #\a #\c #\k) (vector-copy sink 0 pos))

(set-port-position! p saved-pos)
(write-string "Cli" p)
(flush-output-port p)
(check-equal #(#\a #\b #\c #\C #\l #\i) (vector-copy sink 0 pos))
(check-equal #(#\a #\b #\c #\C #\l #\i #\c #\k) (vector-copy sink 0 (+ pos 2)))

(close-port p)
(check-equal #t closed)

(check-equal #t (file-error? (make-file-error "bad")))

(check-equal #t (i/o-invalid-position-error? (make-i/o-invalid-position-error 0)))

;;
;; Transcoded Ports
;;

;; make-codec
;; make-transcorder
;; transcoded-port

(check-equal #t (eq? (make-codec "iso_8859-1") (latin-1-codec)))
(check-equal #f (eq? (make-codec "us-ascii") (latin-1-codec)))
(check-equal #t (eq? (make-codec "unicode-1-1-utf-8") (utf-8-codec)))
(check-equal #t (eq? (make-codec "utf8") (utf-8-codec)))
(check-equal #f (eq? (utf-8-codec) (utf-16-codec)))
(check-equal #t (eq? (make-codec "utf-16") (utf-16-codec)))

(check-error (assertion-violation) (make-codec "not-a-valid-codec"))

(define exc (guard (o (else o)) (make-codec "not-a-valid-codec")))
(check-equal #t (unknown-encoding-error? exc))
(check-equal "not-a-valid-codec" (unknown-encoding-error-name exc))

(check-equal #t (eq? (native-eol-style) (cond-expand (windows 'crlf) (else 'lf))))

(define (tst-string m)
    (let ((s (make-string m)))
        (define (set-ch n m o)
            (if (< n m)
                (begin
                    (string-set! s n (integer->char (+ n o)))
                    (set-ch (+ n 1) m o))))
        (set-ch 0 m (char->integer #\!))
        s))

(define (tst-read-port port)
    (define (read-port port n)
        (let ((obj (read-char port)))
            (if (not (eof-object? obj))
                (begin
                    (if (or (not (char? obj)) (not (= n (char->integer obj))))
                        (error "expected character" (integer->char n) obj))
                    (read-port port (+ n 1))))))
    (read-port port (char->integer #\!)))

(define utf-8-tc (make-transcoder (utf-8-codec) 'none 'replace))
(call-with-port (transcoded-port (open-binary-output-file "output.utf8") utf-8-tc)
    (lambda (port)
        (display (tst-string 5000) port)))

(call-with-port (transcoded-port (open-binary-input-file "output.utf8") utf-8-tc) tst-read-port)

(define utf-16-tc (make-transcoder (utf-16-codec) 'none 'replace))
(call-with-port (transcoded-port (open-binary-output-file "output.utf16") utf-16-tc)
    (lambda (port)
        (display (tst-string 5000) port)))

(call-with-port (transcoded-port (open-binary-input-file "output.utf16") utf-16-tc) tst-read-port)

;; Tests from github.comm/scheme-requests-for-implementation/srfi-192

(define *native-tc* (make-transcoder (utf-8-codec) 'lf 'replace))
(define (native-transcoder) *native-tc*)

(check-equal "ABCD" (bytevector->string '#u8(#x41 #x42 #x43 #x44) (native-transcoder)))
(check-equal
    (#\A #\B #\C #\xa1 #\xa2 #\xa3 #\X #\Y #\Z #\xc1 #\xc2 #\xc3)
    (string->list
        (bytevector->string '#u8(#x41 #x42 #x43 #xa1 #xa2 #xa3 #x58 #x59 #x5a #xc1 #xc2 #xc3)
                (make-transcoder (latin-1-codec) (native-eol-style) 'replace))))

; XXX: need to remove the byte order mark if there
;(check-equal
;    (#\A #\B #\x3000 #\xc1 #\C #\D)
(check-equal
    (#\xfeff #\A #\B #\x3000 #\xc1 #\C #\D)
    (string->list
        (bytevector->string
                '#u8(#xff #xfe #x41 #x00 #x42 #x00 #x00 #x30 #xc1 #x00 #x43 #x00 #x44 #x00)
                (make-transcoder (utf-16-codec) (native-eol-style) 'replace))))

(check-equal
    (#\A #\B #\x3000 #\xc1 #\C #\D)
    (string->list
        (bytevector->string '#u8(#x41 #x00 #x42 #x00 #x00 #x30 #xc1 #x00 #x43 #x00 #x44 #x00)
                (make-transcoder (utf-16-codec) (native-eol-style) 'replace))))

(check-equal #u8(#x41 #x42 #x43 #x44)
    (string->bytevector "ABCD" (native-transcoder)))
(check-equal #u8(#x41 #x42 #x43 #x44)
     (string->bytevector "ABCD" (make-transcoder (latin-1-codec) (native-eol-style) 'raise)))

(check-error (assertion-violation)
    (string->bytevector "ABC\xFF;DEF"
            (make-transcoder (make-codec "ascii") (native-eol-style) 'raise)))

(define exc
    (guard (o (else o))
        (string->bytevector "ABC\xFF;DEF"
                (make-transcoder (make-codec "ascii") (native-eol-style) 'raise))))
(check-equal #t (i/o-encoding-error? exc))
(check-equal #\xFF (i/o-encoding-error-char exc))

(check-error (assertion-violation)
    (string->bytevector "ABC\x100;DEF"
            (make-transcoder (latin-1-codec) (native-eol-style) 'raise)))

(define exc
    (guard (o (else o))
        (string->bytevector "ABC\x100;DEF"
                (make-transcoder (latin-1-codec) (native-eol-style) 'raise))))
(check-equal #t (i/o-encoding-error? exc))
(check-equal #\x100 (i/o-encoding-error-char exc))

(check-error (assertion-violation)
    (bytevector->string #u8(60 61 62 128 63 64)
            (make-transcoder (make-codec "ascii") (native-eol-style) 'raise)))

(define exc
    (guard (o (else o))
        (bytevector->string #u8(60 61 62 128 63 64)
                (make-transcoder (make-codec "ascii") (native-eol-style) 'raise))))
(check-equal #t (i/o-decoding-error? exc))
(check-equal #f (i/o-encoding-error? exc))

(check-equal
    (#\A #\B #\return #\C #\return #\newline #\X #\newline #\Y #\Z)
    (string->list
        (bytevector->string '#u8(#x41 #x42 #x0d #x43 #x0d #x0a #x58 #x0a #x59 #x5a)
                (make-transcoder (latin-1-codec) 'none 'replace))))

(check-equal
    (#\A #\B #\newline #\C #\newline #\X #\newline #\Y #\Z)
    (string->list
        (bytevector->string '#u8(#x41 #x42 #x0d #x43 #x0d #x0a #x58 #x0a #x59 #x5a)
                (make-transcoder (latin-1-codec) 'lf 'replace))))

(check-equal
    (#\A #\B #\newline #\C #\newline #\X #\newline #\Y #\Z)
    (string->list
        (bytevector->string '#u8(#x41 #x42 #x0d #x43 #x0d #x0a #x58 #x0a #x59 #x5a)
                (make-transcoder (latin-1-codec) 'crlf 'replace))))

(check-equal
    #u8(#x41 #x0a #x0d #x42 #x0d #x0a #x43 #x0a #x44 #x0d #x45 #x0a #x0d #x0d #x0a #x0d #x46)
    (string->bytevector
            (list->string '(#\A #\newline #\return #\B #\return #\newline #\C #\newline
                    #\D #\return #\E #\newline #\return #\return #\newline #\return #\F))
            (make-transcoder (latin-1-codec) 'none 'replace)))

(check-equal
    #u8(#x41 #x0a #x0a #x42 #x0a #x43 #x0a #x44 #x0a #x45 #x0a #x0a #x0a #x0a #x46)
    (string->bytevector
            (list->string '(#\A #\newline #\return #\B #\return #\newline #\C #\newline
                    #\D #\return #\E #\newline #\return #\return #\newline #\return #\F))
            (make-transcoder (latin-1-codec) 'lf 'replace)))

(check-equal
    (#\A #\newline #\return #\B #\return #\newline #\C #\newline
            #\D #\return #\E #\newline #\return #\return #\newline #\return #\F)
    (string->list (list->string '(#\A #\newline #\return #\B #\return #\newline #\C #\newline
            #\D #\return #\E #\newline #\return #\return #\newline #\return #\F))))

(check-equal
    #u8(#x41 #x0d #x0a #x0d #x0a #x42 #x0d #x0a #x43 #x0d #x0a #x44 #x0d #x0a #x45
            #x0d #x0a #x0d #x0a #x0d #x0a #x0d #x0a #x46)
    (string->bytevector
            (list->string '(#\A #\newline #\return #\B #\return #\newline #\C #\newline
                    #\D #\return #\E #\newline #\return #\return #\newline #\return #\F))
            (make-transcoder (latin-1-codec) 'crlf 'replace)))

