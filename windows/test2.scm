(import (foment base))

;; From Chibi Scheme
;; Adapted from Bawden's algorithm.
(define (rationaliz x e)
    (define (sr x y return)
        (let ((fx (floor x)) (fy (floor y)))
(write x) (display " ") (write fx) (display " ") (write y) (display " ") (write fy) (newline)
            (cond
                ((>= fx x)
                    (return fx 1))
                ((= fx fy)
                    (sr (/ (- y fy)) (/ (- x fx)) (lambda (n d) (return (+ d (* fx n)) n))))
                (else
                    (return (+ fx 1) 1)))))
    (let ((return (if (negative? x) (lambda (num den) (/ (- num) den)) /))
            (x (abs x))
            (e (abs e)))
(write x) (display " ") (write e) (newline)
        (sr (- x e) (+ x e) return)))

(write (rationaliz 0.3 1/10))
(newline)(newline)
(write (rationaliz (exact 0.3) 1/10))
