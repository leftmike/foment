(define-library (lib t1)
    (import (scheme base))
    (begin
        (define lib-t1-a 10)
        (define lib-t1-b 20)
        (define lib-t1-c 30))
    (export lib-t1-a (rename lib-t1-b b-lib-t1)))
