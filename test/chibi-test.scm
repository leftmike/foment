(define-library (chibi test)
    (import (scheme base))
    (import (scheme write))
    (export test-begin test-end test)
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
				(newline)))))))))
