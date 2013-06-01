(define-library (lib t11)
    (import (scheme base))
    (begin
        (define lib-t11-a 1)
        (define lib-t11-b 2)
        (define lib-t11-c 3)
        (define lib-t11-d 4))
    (export lib-t11-a lib-t11-b lib-t11-c lib-t11-d))
