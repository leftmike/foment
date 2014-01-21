(define-library (lib t15)
    (import (scheme base) (lib t14))
    (export lib-t15-a lib-t15-b)
    (begin
        (define (lib-t15-a p q) (lib-t14-a p q))
        (define (lib-t15-b p q) (lib-t14-b p q))))
