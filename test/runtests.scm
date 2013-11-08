;;
;; A program to run the tests.
;;
;; foment runtests.scm <test> ...
;;

(import (foment base))

(write (features))
(newline)

(define pass-count 0)
(define fail-count 0)

(define (run-tests lst)
    (define (fail obj ret)
        (set! fail-count (+ fail-count 1))
        (display "failed: ")
        (write obj)
        (display ": ")
        (write ret)
        (newline))
    (let ((env (interaction-environment '(scheme base))))
        (define (test-check-equal obj)
            (let ((ret (eval (caddr obj) env)))
                (if (equal? (unsyntax (cadr obj)) ret)
                    (set! pass-count (+ pass-count 1))
                    (fail obj ret))))
        (define (test-check-error obj)
            (guard (exc
                ((error-object? exc)
                    (let ((want (unsyntax (cadr obj))))
                        (if (or (not (equal? (car want) (error-object-type exc)))
                                (and (pair? (cdr want))
                                        (not (equal? (cadr want) (error-object-who exc)))))
                            (fail obj exc)
                            (set! pass-count (+ pass-count 1)))))
                (else (fail obj exc)))
                (eval (caddr obj) env)
                (fail obj "no exception raised")))
        (define (test port)
            (let ((obj (read port)))
                (if (not (eof-object? obj))
                    (begin
                        (cond
                            ((and (pair? obj) (eq? (unsyntax (car obj)) 'check-equal))
                                (test-check-equal obj))
                            ((and (pair? obj) (eq? (unsyntax (car obj)) 'check-error))
                                (test-check-error obj))
                            ((and (pair? obj) (eq? (unsyntax (car obj)) 'check-syntax))
                                (test-check-error obj))
                            (else (eval obj env)))
                        (test port)))))
        (define (run name)
            (let ((port (open-input-file name)))
                (want-identifiers port #t)
                (call-with-port port test)))
        (if (not (null? lst))
            (begin
                (display (car lst))
                (newline)
                (run (car lst))
                (run-tests (cdr lst))))))

(run-tests (cdr (command-line)))
(display "pass: ") (display pass-count) (display " fail: ") (display fail-count) (newline)
