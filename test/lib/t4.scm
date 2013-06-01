(define-library (lib t2)
    (import (scheme base) (lib t1))
    (begin
        (define (lib-t2-a) lib-t1-a)
        (define (lib-t2-c) lib-t1-c)))
    (export lib-t2-a lib-t2-c))
