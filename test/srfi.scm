;;;
;;; SRFIs
;;;

;;
;; ---- SRFI 112: Environment Inquiry ----
;;

(import (srfi 112))

(must-equal #t (or (string? (implementation-name)) (eq? (implementation-name) #f)))
(must-raise (assertion-violation implementation-name) (implementation-name 1))

(must-equal #t (or (string? (implementation-version)) (eq? (implementation-version) #f)))
(must-raise (assertion-violation implementation-version) (implementation-version 1))

(must-equal #t (or (string? (cpu-architecture)) (eq? (cpu-architecture) #f)))
(must-raise (assertion-violation cpu-architecture) (cpu-architecture 1))

(must-equal #t (or (string? (machine-name)) (eq? (machine-name) #f)))
(must-raise (assertion-violation machine-name) (machine-name 1))

(must-equal #t (or (string? (os-name)) (eq? (os-name) #f)))
(must-raise (assertion-violation os-name) (os-name 1))

(must-equal #t (or (string? (os-version)) (eq? (os-version) #f)))
(must-raise (assertion-violation os-version) (os-version 1))

;;
;; ---- SRFI 111: Boxes ----
;;

(import (srfi 111))

(must-equal 10 (unbox (box 10)))
(must-equal #t (equal? (box 20) (box 20)))
(must-equal #f (eq? (box 30) (box 30)))

(must-raise (assertion-violation box) (box))
(must-raise (assertion-violation box) (box 1 2))

(must-equal #t (box? (box 10)))
(must-equal #f (box? 10))
(must-equal #f (box? #(10)))
(must-equal #f (box? (cons 1 2)))

(must-raise (assertion-violation box?) (box?))
(must-raise (assertion-violation box?) (box? 1 2))

(must-equal 10 (unbox (box 10)))

(must-raise (assertion-violation unbox) (unbox))
(must-raise (assertion-violation unbox) (unbox (box 1) 2))
(must-raise (assertion-violation unbox) (unbox (cons 1 2)))

(define b (box 10))
(must-equal 10 (unbox b))
(set-box! b 20)
(must-equal 20 (unbox b))
(must-equal #t (equal? b (box 20)))
(must-equal #f (eq? b (box 20)))

(must-raise (assertion-violation set-box!) (set-box! b))
(must-raise (assertion-violation set-box!) (set-box! b 2 3))
(must-raise (assertion-violation set-box!) (set-box! (cons 1 2) 2))

