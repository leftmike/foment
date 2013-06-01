(define-library (lib t6)
    (import (scheme base) (lib t5))
    (begin
        (define (a) lib-t5-a)
        (define (b) (lib-t5-b)))
    (export (rename a lib-t6-a) (rename b lib-t6-b)))
