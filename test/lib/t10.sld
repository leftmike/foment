(define-library (lib t10)
    (import (scheme base))
    (begin
        (define lib-t10-a 1)
        (define lib-t10-b 2)
        (define lib-t10-c 3)
        (define lib-t10-d 4))
    (export lib-t10-a lib-t10-b lib-t10-c lib-t10-d))
