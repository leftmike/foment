(define-library (chibi test)
    (import (scheme base))
    (import (scheme write))
    (export test-begin test-end test test-error test-values)
    (begin
        (define passed 0)
        (define failed 0)
        (define section '())
        (define failed-section '())

        (define (test-passed)
            (set! passed (+ passed 1)))

        (define (test-failed expr thunk)
            (set! failed (+ failed 1))
            (if (not (eq? failed-section section))
                (begin
                    (set! failed-section section)
                    (display (car failed-section))
                    (newline)))
            (display "failed: ")
            (write expr)
            (thunk)
            (newline))

        (define (test-begin msg)
            (set! section (cons msg section)))

        (define (test-end)
            (set! section (cdr section))
            (if (null? section)
                (begin
                    (display "pass: ") (write passed)
                    (display " fail: ") (write failed)
                    (newline))))

        (define (check-equal? a b)
            (if (equal? a b)
                #t
                (if (and (number? a) (inexact? a) (number? b) (inexact? b))
                    (equal? (number->string a) (number->string b))
                    #f)))

        (define-syntax test
            (syntax-rules ()
                ((test expect expr)
                    (guard (exc
                        (else
                            (test-failed 'expr
                                (lambda ()
                                    (display " exception: ")
                                    (write exc)))))
                        (let ((ret expr))
    		        (if (check-equal? ret expect)
    		            (test-passed)
    		            (test-failed 'expr
    		                (lambda ()
                                    (display " got: ")
                                    (write ret)
                                    (display " expected: ")
                                    (write expect)))))))))

        (define-syntax test-error
            (syntax-rules ()
                ((test-error expr)
                    (guard (exc
                        (else (test-passed)))
                        expr
                        (test-failed 'expr
                            (lambda ()
                                (display " : no exception raised")))))))

        (define-syntax test-values
            (syntax-rules ()
                ((test-values vals expr)
                    (test
                        (call-with-values (lambda () vals) list)
                        (call-with-values (lambda () expr) list)))))
    ))

