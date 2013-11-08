(define-library (chibi test)
    (import (scheme base))
    (import (scheme write))
    (export test-begin test-end test test-error)
    (begin
        (define (test-begin msg) (display msg) (newline))
        (define (test-end) #f)
        (define-syntax test
            (syntax-rules ()
                ((text expect expr)
                    (let ((ret expr))
		        (if (not (equal? ret expect))
			    (begin
			        (display "failed: ")
				(write 'expr)
				(display ": ")
				(write ret)
				(newline)))))))
        (define-syntax test-error
            (syntax-rules ()
                ((test-error expr)
                    (guard (exc
                        (else #f))
                        expr
                        (display "failed: ")
                        (write 'expr)
                        (display " : no exception raised")
                        (newline)))))))
