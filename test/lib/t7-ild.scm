(import (scheme base))
(begin
    (define a 1000)
    (define (b) a))
(export (rename a lib-t7-a) (rename b lib-t7-b))
