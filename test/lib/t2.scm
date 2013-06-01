(define-library (lib t2)
    (import (scheme base) (lib t1))
    (begin
        (define (lib-t2-a) lib-t1-a)
        (define (lib-t2-b) b-lib-t1))
    (export lib-t2-a lib-t2-b))
