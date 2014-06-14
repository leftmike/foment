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

(check-equal 4 (bit-count #b10101010))
(check-equal 0 (bit-count 0))
(check-equal 1 (bit-count -2))

(check-error (assertion-violation bit-count) (bit-count))
(check-error (assertion-violation bit-count) (bit-count 12 34))
(check-error (assertion-violation bit-count) (bit-count 12.34))
(check-error (assertion-violation bit-count) (bit-count 12/34))

(check-equal 8 (integer-length #b10101010))
(check-equal 0 (integer-length 0))
(check-equal 4 (integer-length #b1111))

(check-error (assertion-violation integer-length) (integer-length))
(check-error (assertion-violation integer-length) (integer-length 12 34))
(check-error (assertion-violation integer-length) (integer-length 12.34))
(check-error (assertion-violation integer-length) (integer-length 12/34))

(check-equal -1 (first-set-bit 0))
(check-equal -1 (first-set-bit 0))
(check-equal 0 (first-set-bit -1))
(check-equal 0 (first-set-bit 1))
(check-equal 1 (first-set-bit -2))
(check-equal 1 (first-set-bit 2))
(check-equal 0 (first-set-bit -3))
(check-equal 0 (first-set-bit 3))
(check-equal 2 (first-set-bit -4))
(check-equal 2 (first-set-bit 4))
(check-equal 0 (first-set-bit -5))
(check-equal 0 (first-set-bit 5))
(check-equal 1 (first-set-bit -6))
(check-equal 1 (first-set-bit 6))
(check-equal 0 (first-set-bit -7))
(check-equal 0 (first-set-bit 7))
(check-equal 3 (first-set-bit -8))
(check-equal 3 (first-set-bit 8))
(check-equal 0 (first-set-bit -9))
(check-equal 0 (first-set-bit 9))
(check-equal 1 (first-set-bit -10))
(check-equal 1 (first-set-bit 10))
(check-equal 0 (first-set-bit -11))
(check-equal 0 (first-set-bit 11))
(check-equal 2 (first-set-bit -12))
(check-equal 2 (first-set-bit 12))
(check-equal 0 (first-set-bit -13))
(check-equal 0 (first-set-bit 13))
(check-equal 1 (first-set-bit -14))
(check-equal 1 (first-set-bit 14))
(check-equal 0 (first-set-bit -15))
(check-equal 0 (first-set-bit 15))
(check-equal 4 (first-set-bit -16))
(check-equal 4 (first-set-bit 16))
(check-equal 123 (first-set-bit (arithmetic-shift 1 123)))
(check-equal 123 (first-set-bit (arithmetic-shift -1 123)))

(check-error (assertion-violation first-set-bit) (first-set-bit))
(check-error (assertion-violation first-set-bit) (first-set-bit 12 34))
(check-error (assertion-violation first-set-bit) (first-set-bit 12.34))
(check-error (assertion-violation first-set-bit) (first-set-bit 12/34))

(check-equal #t (logbit? 0 #b1101))
(check-equal #f (logbit? 1 #b1101))
(check-equal #t (logbit? 2 #b1101))
(check-equal #t (logbit? 3 #b1101))
(check-equal #f (logbit? 4 #b1101))

(check-equal "1" (number->string (copy-bit 0 0 #t) 2))
(check-equal "100" (number->string (copy-bit 2 0 #t) 2))
(check-equal "1011" (number->string (copy-bit 2 #b1111 #f) 2))

(check-equal "1010" (number->string (bit-field #b1101101010 0 4) 2))
(check-equal "10110" (number->string (bit-field #b1101101010 4 9) 2))

(check-equal "1101100000" (number->string (copy-bit-field #b1101101010 0 0 4) 2))
(check-equal "1101101111" (number->string (copy-bit-field #b1101101010 -1 0 4) 2))
(check-equal "110100111110000" (number->string (copy-bit-field #b110100100010000 -1 5 9) 2))

(check-equal "1000" (number->string (arithmetic-shift #b1 3) 2))
(check-equal "101" (number->string (arithmetic-shift #b1010 -1) 2))

(check-equal "111010110111100110100010101" (number->string (arithmetic-shift 123456789 0) 2))
(check-equal "11101011011110011010001010100000" (number->string (arithmetic-shift 123456789 5) 2))
(check-equal "-11101011011110011010001010100000"
    (number->string (arithmetic-shift -123456789 5) 2))
(check-equal "1110101101111001101000101010000000000000"
    (number->string (arithmetic-shift 123456789 13) 2))
(check-equal "-1110101101111001101000101100010000000"
    (number->string (arithmetic-shift -987654321 7) 2))
(check-equal "1111000100100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    (number->string (arithmetic-shift 123456 101) 2))
(check-equal "-1111000100100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000"
    (number->string (arithmetic-shift -123456 101) 2))
(check-equal "1000011100100111111101100011011010011010101011111000001111001010000101010000001001100111010001111010111110001100011111110001100101101100111000111111000010101101001000000000000000000000000"
    (number->string (arithmetic-shift 12345678901234567890123456789012345678901234567890 23) 2))
(check-equal "-10000111001001111111011000110110100110101010111110000011110010100001010100000010011001110100011110101111100011000111111100011001011011001110001111110000101011010010000000000000000000000000000"
    (number->string (arithmetic-shift -12345678901234567890123456789012345678901234567890 27) 2))
(check-equal "1110101101111001101000101" (number->string (arithmetic-shift 123456789 -2) 2))
(check-equal "-1110101101111001101000110" (number->string (arithmetic-shift -123456789 -2) 2))
(check-equal "1110101101" (number->string (arithmetic-shift 123456789 -17) 2))
(check-equal "0" (number->string (arithmetic-shift 123456789 -101) 2))
(check-equal "-1" (number->string (arithmetic-shift -123456789 -101) 2))
(check-equal "1001110101010111101010000010011100100101111000001111001100010000101110011111110000111101001000011011110001100001101111011010010011100100011001100111111110001100101101100111000111111000"
    (number->string (arithmetic-shift 123456789012345678901234567890123456789012345678901234567890 -13) 2))
(check-equal "-1001110101010111101010000010011100100101111000001111001100010000101110011111110000111101001000011011110001100001101111011010010011100100011001100111111110001100101101100111000111111001"
    (number->string (arithmetic-shift -123456789012345678901234567890123456789012345678901234567890 -13) 2))
(check-equal "100111010101011110101000001001110010010111100000111100110001000010111001111111000011110100100001"
    (number->string (arithmetic-shift 123456789012345678901234567890123456789012345678901234567890 -101) 2))
(check-equal "-100111010101011110101000001001110010010111100000111100110001000010111001111111000011110100100010"
    (number->string (arithmetic-shift -123456789012345678901234567890123456789012345678901234567890 -101) 2))
(check-equal "1001110101010111"
    (number->string (arithmetic-shift 123456789012345678901234567890123456789012345678901234567890 -181) 2))
(check-equal "-1001110101011000"
    (number->string (arithmetic-shift -123456789012345678901234567890123456789012345678901234567890 -181) 2))
(check-equal "0"
    (number->string (arithmetic-shift 123456789012345678901234567890123456789012345678901234567890 -201) 2))
(check-equal "-1"
    (number->string (arithmetic-shift -123456789012345678901234567890123456789012345678901234567890 -201) 2))

(check-error (assertion-violation arithmetic-shift) (arithmetic-shift))
(check-error (assertion-violation arithmetic-shift) (arithmetic-shift 12))
(check-error (assertion-violation arithmetic-shift) (arithmetic-shift 12 34.56))
(check-error (assertion-violation arithmetic-shift) (arithmetic-shift 12 34/56))
(check-error (assertion-violation arithmetic-shift) (arithmetic-shift 12.34 56))
(check-error (assertion-violation arithmetic-shift) (arithmetic-shift 12/34 56))

(check-equal "10" (number->string (rotate-bit-field #b0100 3 0 4) 2))
(check-equal "10" (number->string (rotate-bit-field #b0100 -1 0 4) 2))
(check-equal "110100010010000" (number->string (rotate-bit-field #b110100100010000 -1 5 9) 2))
(check-equal "110100000110000" (number->string (rotate-bit-field #b110100100010000 1 5 9) 2))

(check-equal "e5" (number->string (reverse-bit-field #xa7 0 8) 16))
