(define-library (lib t9)
    (import (scheme base))
    (begin
        (define lib-t9-a 1)
        (define lib-t9-b 2)
        (define lib-t9-c 3)
        (define lib-t9-d 4))
    (export lib-t9-a lib-t9-b lib-t9-c lib-t9-d))
