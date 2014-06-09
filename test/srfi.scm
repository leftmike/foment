;;;
;;; SRFIs
;;;

;;
;; ---- SRFI 112: Environment Inquiry ----
;;

(import (srfi 112))

(check-equal #t (or (string? (implementation-name)) (eq? (implementation-name) #f)))
(check-error (assertion-violation implementation-name) (implementation-name 1))

(check-equal #t (or (string? (implementation-version)) (eq? (implementation-version) #f)))
(check-error (assertion-violation implementation-version) (implementation-version 1))

(check-equal #t (or (string? (cpu-architecture)) (eq? (cpu-architecture) #f)))
(check-error (assertion-violation cpu-architecture) (cpu-architecture 1))

(check-equal #t (or (string? (machine-name)) (eq? (machine-name) #f)))
(check-error (assertion-violation machine-name) (machine-name 1))

(check-equal #t (or (string? (os-name)) (eq? (os-name) #f)))
(check-error (assertion-violation os-name) (os-name 1))

(check-equal #t (or (string? (os-version)) (eq? (os-version) #f)))
(check-error (assertion-violation os-version) (os-version 1))

;;
;; ---- SRFI 111: Boxes ----
;;

(import (srfi 111))

(check-equal 10 (unbox (box 10)))
(check-equal #t (equal? (box 20) (box 20)))
(check-equal #f (eq? (box 30) (box 30)))

(check-error (assertion-violation box) (box))
(check-error (assertion-violation box) (box 1 2))

(check-equal #t (box? (box 10)))
(check-equal #f (box? 10))
(check-equal #f (box? #(10)))
(check-equal #f (box? (cons 1 2)))

(check-error (assertion-violation box?) (box?))
(check-error (assertion-violation box?) (box? 1 2))

(check-equal 10 (unbox (box 10)))

(check-error (assertion-violation unbox) (unbox))
(check-error (assertion-violation unbox) (unbox (box 1) 2))
(check-error (assertion-violation unbox) (unbox (cons 1 2)))

(define b (box 10))
(check-equal 10 (unbox b))
(set-box! b 20)
(check-equal 20 (unbox b))
(check-equal #t (equal? b (box 20)))
(check-equal #f (eq? b (box 20)))

(check-error (assertion-violation set-box!) (set-box! b))
(check-error (assertion-violation set-box!) (set-box! b 2 3))
(check-error (assertion-violation set-box!) (set-box! (cons 1 2) 2))

;;
;; ---- SRFI 60: Integers As Bits ----
;;

(import (srfi 60))

(check-equal "1000" (number->string (bitwise-and #b1100 #b1010) 2))

(check-equal -1 (bitwise-and))
(check-equal 12345 (bitwise-and 12345))
(check-equal 4145 (bitwise-and 12345 54321))
(check-equal 4145 (bitwise-and 54321 12345))
(check-equal 49 (bitwise-and 12345 54321 10101010101))
(check-equal 16 (bitwise-and 12345 12345678901234567890123456789012345678901234567890))
(check-equal 39235487929048208904815248
    (bitwise-and 12345678901234567890123456789012345678901234567890 987654321987654321987654321))

(check-error (assertion-violation bitwise-and) (bitwise-and 1234 56.78))
(check-error (assertion-violation bitwise-and) (bitwise-and 1234 56/78))
(check-error (assertion-violation bitwise-and) (bitwise-and 12.34 5678))
(check-error (assertion-violation bitwise-and) (bitwise-and 12/34 5678))

(check-equal 0 (bitwise-ior))
(check-equal 12345 (bitwise-ior 12345))
(check-equal 62521( bitwise-ior 12345 54321))
(check-equal 62521( bitwise-ior 54321 12345))
(check-equal 1014205 (bitwise-ior 12345 1010101))
(check-equal 1014205 (bitwise-ior 1010101 12345))
(check-equal 123456789123456789123456789123456789123456789123465021
    (bitwise-ior 12345 123456789123456789123456789123456789123456789123456789))
(check-equal 123456789123456789123456789123456789123456789123465021
    (bitwise-ior 123456789123456789123456789123456789123456789123456789 12345))
(check-equal 1234123412341276660008185256654991575253956765272574
    (bitwise-ior 1234123412341234123412341234123412341234123412341234
            5678567856785678567856785678567856785678))

(check-error (assertion-violation bitwise-ior) (bitwise-ior 1234 56.78))
(check-error (assertion-violation bitwise-ior) (bitwise-ior 1234 56/78))
(check-error (assertion-violation bitwise-ior) (bitwise-ior 12.34 5678))
(check-error (assertion-violation bitwise-ior) (bitwise-ior 12/34 5678))

(check-equal 0 (bitwise-xor))
(check-equal 12345 (bitwise-xor 12345))
(check-equal 58376 (bitwise-xor 12345 54321))
(check-equal 58376 (bitwise-xor 54321 12345))
(check-equal 10 (bitwise-xor #b10001 #b11011))
(check-equal 678967896789678967896789678967896789678967896789678967884524
    (bitwise-xor 12345 678967896789678967896789678967896789678967896789678967896789))
(check-equal 7088580165427581030224296127424816853124929578319024555
    (bitwise-xor 1234512345123451234512345123451234512345123451234512345
            6789067890678906789067890678906789067890678906789067890))

(check-error (assertion-violation bitwise-xor) (bitwise-xor 1234 56.78))
(check-error (assertion-violation bitwise-xor) (bitwise-xor 1234 56/78))
(check-error (assertion-violation bitwise-xor) (bitwise-xor 12.34 5678))
(check-error (assertion-violation bitwise-xor) (bitwise-xor 12/34 5678))

(check-equal -12346 (bitwise-not 12345))
(check-equal -6789067890678906789067890678906789067890678906789067890678906789067891
    (bitwise-not 6789067890678906789067890678906789067890678906789067890678906789067890))

(check-error (assertion-violation bitwise-not) (bitwise-not 12.34))
(check-error (assertion-violation bitwise-not) (bitwise-not 12/34))
(check-error (assertion-violation bitwise-not) (bitwise-not))
(check-error (assertion-violation bitwise-not) (bitwise-not 12 34))

(check-equal #f (any-bits-set? #b0100 #b1011))
(check-equal #t (any-bits-set? #b0100 #b0111))