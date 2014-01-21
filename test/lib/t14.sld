(define-library (lib t14)
    (import (scheme base))
    (export lib-t14-a lib-t14-b)
    (begin
        (define (r- a b) (- b a))
        (define (lib-t14-a x y) (r- x y))
        (define (lib-t14-b x y)
            (define (rev- a b) (- b a))
            (rev- x y))))
