(define-library (lib t5)
    (import (scheme base))
    (begin
        (define a 1000)
        (define (b) a))
    (export (rename a lib-t5-a) (rename b lib-t5-b)))
