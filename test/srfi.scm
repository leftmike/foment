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

;;
;; ---- SRFI 1: List Library ----
;;

(import (srfi 1))

(check-equal (a) (cons 'a '()))
(check-equal ((a) b c d) (cons '(a) '(b c d)))
(check-equal ("a" b c) (cons "a" '(b c)))
(check-equal (a . 3) (cons 'a 3))
(check-equal ((a b) . c) (cons '(a b) 'c))

(check-equal (a 7 c) (list 'a (+ 3 4) 'c))
(check-equal () (list))

(check-equal (a . b) (xcons 'b 'a))

(check-equal (1 2 3 . 4) (cons* 1 2 3 4))
(check-equal 1 (cons* 1))

(check-equal (c c c c) (make-list 4 'c))
(check-equal () (make-list 0))
(check-equal 10 (length (make-list 10)))

(check-equal (0 1 2 3) (list-tabulate 4 values))
(check-equal () (list-tabulate 0 values))

(define cl (circular-list 'a 'b 'c))
(check-equal #t (eq? cl (cdddr cl)))
(check-equal (a b a b a b a b) (take (circular-list 'a 'b) 8))
(check-equal #f (list? (circular-list 1 2 3 4 5)))

(check-equal (0 1 2 3 4 5) (iota 6))
(check-equal (0 -0.1 -0.2 -0.3 -0.4) (iota 5 0 -0.1))

(check-equal #t (proper-list? '(a b c)))
(check-equal #t (proper-list? '()))
(check-equal #f (proper-list? '(a b . c)))
(check-equal #f (proper-list? 'a))
(check-equal #f (proper-list? (circular-list 1 2 3 4 5 6)))

(check-equal #f (circular-list? '(a b c)))
(check-equal #f (circular-list? '()))
(check-equal #f (circular-list? '(a b . c)))
(check-equal #f (circular-list? 'a))
(check-equal #t (circular-list? (circular-list 1 2 3 4 5 6)))
(check-equal #t (circular-list? (cons 0 (circular-list 1 2 3 4 5 6))))

(check-equal #f (dotted-list? '(a b c)))
(check-equal #f (dotted-list? '()))
(check-equal #t (dotted-list? '(a b . c)))
(check-equal #t (dotted-list? 'a))
(check-equal #f (dotted-list? (circular-list 1 2 3 4 5 6)))

(check-equal #t (not-pair? 'a))
(check-equal #t (not-pair? #()))
(check-equal #f (not-pair? (cons 1 2)))
(check-equal #t (not-pair? '()))

(check-equal #f (null-list? (cons 1 2)))
(check-equal #t (null-list? '()))

(check-equal #t (list= eq? '(1 2 3 4) '(1 2 3 4)))
(check-equal #f (list= eq? '(1 2 3) '(1 2 3 4)))
(check-equal #f (list= eq? '(1 2 3 4) '(1 2 3)))
(check-equal #f (list= eq? '(1 2 3 4) '(1 2 3 5)))
(check-equal #t (list= eq? '(1 2 3 4) '(1 2 3 4) '(1 2 3 4)))
(check-equal #f (list= eq? '(1 2 3) '(1 2 3 4) '(1 2 3 4)))
(check-equal #f (list= eq? '(1 2 3 4) '(1 2 3) '(1 2 3 4)))
(check-equal #f (list= eq? '(1 2 3 4) '(1 2 3 4) '(1 2 3)))
(check-equal #f (list= eq? '(1 2 3 4) '(1 2 3 4) '(1 2 3 5)))
(check-equal #f (list= eq? '(1 2 3 4) '(1 2 4) '(1 2 3 4)))
(check-equal #f (list= eq? '(1 2 6 4) '(1 2 3 4) '(1 2 3 4)))

(check-equal 1 (first '(1 2 3 4 5 6 7 8 9 10)))
(check-equal 2 (second '(1 2 3 4 5 6 7 8 9 10)))
(check-equal 3 (third '(1 2 3 4 5 6 7 8 9 10)))
(check-equal 4 (fourth '(1 2 3 4 5 6 7 8 9 10)))
(check-equal 5 (fifth '(1 2 3 4 5 6 7 8 9 10)))
(check-equal 6 (sixth '(1 2 3 4 5 6 7 8 9 10)))
(check-equal 7 (seventh '(1 2 3 4 5 6 7 8 9 10)))
(check-equal 8 (eighth '(1 2 3 4 5 6 7 8 9 10)))
(check-equal 9 (ninth '(1 2 3 4 5 6 7 8 9 10)))
(check-equal 10 (tenth '(1 2 3 4 5 6 7 8 9 10)))

(check-equal (1 2 3) (take '(1 2 3 4 5 6) 3))
(check-equal () (take '(1 2 3 4 5 6) 0))

(check-equal (4 5 6) (drop '(1 2 3 4 5 6) 3))
(check-equal () (drop '(1 2 3 4 5 6) 6))

(check-equal (d e) (take-right '(a b c d e) 2))
(check-equal (2 3 . d) (take-right '(1 2 3 . d) 2))
(check-equal d (take-right '(1 2 3 . d) 0))

(check-equal (a b c) (drop-right '(a b c d e) 2))
(check-equal (1) (drop-right '(1 2 3 . d) 2))
(check-equal (1 2 3) (drop-right '(1 2 3 . d) 0))

(check-equal (1 3) (take! (circular-list 1 3 5) 8))
(check-equal (1 2 3) (take! '(1 2 3 4 5 6) 3))
(check-equal () (take! '(1 2 3 4 5 6) 0))

(check-equal (a b c) (drop-right! '(a b c d e) 2))
(check-equal (1) (drop-right! '(1 2 3 . d) 2))
(check-equal (1 2 3) (drop-right! '(1 2 3 . d) 0))

(check-equal ((1 2 3) (4 5 6 7 8))
    (let-values (((pre suf) (split-at '(1 2 3 4 5 6 7 8) 3))) (list pre suf)))
(check-equal (() (1 2 3 4 5 6 7 8))
    (let-values (((pre suf) (split-at '(1 2 3 4 5 6 7 8) 0))) (list pre suf)))
(check-equal ((1 2 3 4 5 6 7 8) ())
    (let-values (((pre suf) (split-at '(1 2 3 4 5 6 7 8) 8))) (list pre suf)))

(check-equal ((1 2 3) (4 5 6 7 8))
    (let-values (((pre suf) (split-at! '(1 2 3 4 5 6 7 8) 3))) (list pre suf)))
(check-equal (() (1 2 3 4 5 6 7 8))
    (let-values (((pre suf) (split-at! '(1 2 3 4 5 6 7 8) 0))) (list pre suf)))
(check-equal ((1 2 3 4 5 6 7 8) ())
    (let-values (((pre suf) (split-at! '(1 2 3 4 5 6 7 8) 8))) (list pre suf)))

(check-equal c (last '(a b c)))

(check-equal (c) (last-pair '(a b c)))

(check-equal (x y) (append '(x) '(y)))
(check-equal (a b c d) (append '(a) '(b c d)))
(check-equal (a b c d) (append '(a) '() '(b c d)))
(check-equal (a b c d) (append '() '(a) '() '(b c d) '()))
(check-equal (a (b) (c)) (append '(a (b)) '((c))))
(check-equal (a b c . d) (append '(a b) '(c . d)))
(check-equal a (append '() 'a))
(check-equal (x y) (append '(x y)))
(check-equal () (append))

(check-equal (x y) (append! '(x) '(y)))
(check-equal (a b c d) (append! '(a) '(b c d)))
(check-equal (a b c d) (append! '(a) '() '(b c d)))
(check-equal (a b c d) (append! '() '(a) '() '(b c d) '()))
(check-equal (a (b) (c)) (append! '(a (b)) '((c))))
(check-equal (a b c . d) (append! '(a b) '(c . d)))
(check-equal a (append! '() 'a))
(check-equal (x y) (append! '(x y)))
(check-equal () (append!))

(check-equal (x y) (concatenate '((x) (y))))
(check-equal (a b c d) (concatenate '((a) (b c d))))
(check-equal (a b c d) (concatenate '((a) () (b c d))))
(check-equal (a b c d) (concatenate '(() (a) () (b c d) ())))
(check-equal (a (b) (c)) (concatenate '((a (b)) ((c)))))
(check-equal (a b c . d) (concatenate '((a b) (c . d))))
(check-equal a (concatenate '(() a)))
(check-equal (x y) (concatenate '((x y))))
(check-equal () (concatenate '()))

(check-equal (x y) (concatenate! '((x) (y))))
(check-equal (a b c d) (concatenate! '((a) (b c d))))
(check-equal (a b c d) (concatenate! '((a) () (b c d))))
(check-equal (a b c d) (concatenate! '(() (a) () (b c d) ())))
(check-equal (a (b) (c)) (concatenate! '((a (b)) ((c)))))
(check-equal (a b c . d) (concatenate! '((a b) (c . d))))
(check-equal a (concatenate! '(() a)))
(check-equal (x y) (concatenate! '((x y))))
(check-equal () (concatenate! '()))

(check-equal (c b a) (reverse '(a b c)))
(check-equal ((e (f)) d (b c) a) (reverse '(a (b c) d (e (f)))))

(check-equal (c b a) (reverse! '(a b c)))
(check-equal ((e (f)) d (b c) a) (reverse! '(a (b c) d (e (f)))))

(check-equal (3 2 1 4 5) (append-reverse '(1 2 3) '(4 5)))
(check-equal (3 2 1) (append-reverse '(1 2 3) '()))
(check-equal (4 5) (append-reverse '() '(4 5)))

(check-equal (3 2 1 4 5) (append-reverse! '(1 2 3) '(4 5)))
(check-equal (3 2 1) (append-reverse! '(1 2 3) '()))
(check-equal (4 5) (append-reverse! '() '(4 5)))

(check-equal ((one 1 odd) (two 2 even) (three 3 odd))
    (zip '(one two three) '(1 2 3) '(odd even odd even odd even odd even)))
(check-equal ((1) (2) (3)) (zip '(1 2 3)))
(check-equal ((3 #f) (1 #t) (4 #f) (1 #t)) (zip '(3 1 4 1) (circular-list #f #t)))

(check-equal (1 2 3) (unzip1 '((1 2 3) (2 3 4) (3 4 5))))
(check-equal ((1 2 3) (one two three))
     (let-values (((lst1 lst2) (unzip2 '((1 one) (2 two) (3 three))))) (list lst1 lst2)))
(check-equal (1 2 3 4)
    (unzip1 '((1 one a #\a "a") (2 two b #\b "b") (3 three c #\c "c") (4 four d #\d "d"))))
(check-equal ((1 2 3 4) (one two three four))
    (let-values (((lst1 lst2)
            (unzip2 '((1 one a #\a "a") (2 two b #\b "b") (3 three c #\c "c") (4 four d #\d "d")))))
        (list lst1 lst2)))
(check-equal ((1 2 3 4) (one two three four) (a b c d))
    (let-values (((lst1 lst2 lst3)
            (unzip3 '((1 one a #\a "a") (2 two b #\b "b") (3 three c #\c "c") (4 four d #\d "d")))))
        (list lst1 lst2 lst3)))
(check-equal ((1 2 3 4) (one two three four) (a b c d) (#\a #\b #\c #\d))
    (let-values (((lst1 lst2 lst3 lst4)
            (unzip4 '((1 one a #\a "a") (2 two b #\b "b") (3 three c #\c "c") (4 four d #\d "d")))))
        (list lst1 lst2 lst3 lst4)))
(check-equal ((1 2 3 4) (one two three four) (a b c d) (#\a #\b #\c #\d) ("a" "b" "c" "d"))
    (let-values (((lst1 lst2 lst3 lst4 lst5)
            (unzip5 '((1 one a #\a "a") (2 two b #\b "b") (3 three c #\c "c") (4 four d #\d "d")))))
        (list lst1 lst2 lst3 lst4 lst5)))

(check-equal 3 (count even? '(3 1 4 1 5 9 2 5 6)))
(check-equal 3 (count < '(1 2 4 8) '(2 4 6 8 10 12 14 16)))
(check-equal 2 (count < '(3 1 4 1) (circular-list 1 10)))

(check-equal 10 (fold + 0 '(1 2 3 4)))
(check-equal (5 4 3 2 1) (fold cons '() '(1 2 3 4 5)))
(check-equal 3
    (fold (lambda (x count) (if (symbol? x) (+ count 1) count)) 0
            '(a "a" #\a 1 2 3 (b c) d #(e f g) h)))
(check-equal 10
    (fold (lambda (s max-len) (max max-len (string-length s))) 0
            '("abc" "def" "1234567890" "123456789" "wxyz")))
(check-equal (c 3 b 2 a 1) (fold cons* '() '(a b c) '(1 2 3 4 5)))
(check-equal (10 8 6)
    (fold (lambda (frst snd val) (cons (+ frst snd) val)) '() '(1 2 3 4) '(5 6 7)))

(check-equal (1 3 5 7)
    (fold-right (lambda (n lst) (if (odd? n) (cons n lst) lst)) '() '(1 2 3 4 5 6 7 8)))
(check-equal (a 1 b 2 c 3) (fold-right cons* '() '(a b c) '(1 2 3 4 5)))
(check-equal (6 8 10)
    (fold-right (lambda (frst snd val) (cons (+ frst snd) val)) '() '(1 2 3 4) '(5 6 7)))

(check-equal (5 4 3 2 1)
    (pair-fold (lambda (pair tail) (set-cdr! pair tail) pair) '() '(1 2 3 4 5)))
(check-equal 10
    (pair-fold (lambda (s max-len) (max max-len (string-length (car s)))) 0
            '("abc" "def" "1234567890" "123456789" "wxyz")))
(check-equal (10 8 6)
    (pair-fold (lambda (frst snd val) (cons (+ (car frst) (car snd)) val)) '() '(1 2 3 4) '(5 6 7)))

(check-equal (1 3 5 7)
    (pair-fold-right (lambda (n lst) (if (odd? (car n)) (cons (car n) lst) lst)) '()
            '(1 2 3 4 5 6 7 8)))
(check-equal (6 8 10)
    (pair-fold-right (lambda (frst snd val) (cons (+ (car frst) (car snd)) val)) '()
            '(1 2 3 4) '(5 6 7)))

(check-equal 10 (reduce max 0 '(8 4 3 -5 10 9)))

(check-equal (1 2 3 4 5 6 7 8 9) (reduce-right append '() '((1 2 3) (4 5) (6 7 8) (9))))

(check-equal (1 4 9 16 25 36 49 64 81 100)
    (unfold (lambda (x) (> x 10)) (lambda (x) (* x x)) (lambda (x) (+ x 1)) 1))

(check-equal (1 2 3 4 5) (unfold null-list? car cdr '(1 2 3 4 5)))
(check-equal (1 2 3 4 5) (unfold null-list? car cdr '(1 2) (lambda (x) '(3 4 5))))

(check-equal (1 4 9 16 25 36 49 64 81 100)
    (unfold-right zero? (lambda (x) (* x x)) (lambda (x) (- x 1)) 10))
(check-equal (5 4 3 2 1) (unfold-right null-list? car cdr '(1 2 3 4 5)))
(check-equal (3 2 1 4 5) (unfold-right null-list? car cdr '(1 2 3) '(4 5)))
(check-equal (3 2 1) (unfold-right null-list? car cdr '(1 2 3) '()))
(check-equal (4 5) (unfold-right null-list? car cdr '() '(4 5)))

(check-equal (b e h) (map cadr '((a b) (d e) (g h))))
(check-equal (1 4 27 256 3125) (map (lambda (n) (expt n n)) '(1 2 3 4 5)))
(check-equal (5 7 9) (map + '(1 2 3) '(4 5 6)))
(check-equal #t
    (let ((ret (let ((count 0)) (map (lambda (ignored) (set! count (+ count 1)) count) '(a b)))))
        (or (equal? ret '(1 2)) (equal? ret '(2 1)))))
(check-equal (4 1 5 1) (map + '(3 1 4 1) (circular-list 1 0)))

(check-equal #(0 1 4 9 16)
    (let ((v (make-vector 5))) (for-each (lambda (i) (vector-set! v i (* i i))) '(0 1 2 3 4)) v))
(check-equal #(1 1 3 3 5)
    (let ((v (make-vector 5)))
        (for-each (lambda (a b) (vector-set! v a (+ a b))) '(0 1 2 3 4) (circular-list 1 0)) v))

(check-equal (1 -1 3 -3 8 -8) (append-map (lambda (x) (list x (- x))) '(1 3 8)))
(check-equal (1 0 2 1 3 0 4 1 5 0)
    (append-map (lambda (x y) (list x y)) '(1 2 3 4 5) (circular-list 0 1)))

(check-equal (1 -1 3 -3 8 -8) (append-map! (lambda (x) (list x (- x))) '(1 3 8)))
(check-equal (1 0 2 1 3 0 4 1 5 0)
    (append-map! (lambda (x y) (list x y)) '(1 2 3 4 5) (circular-list 0 1)))

(check-equal (b e h) (map! cadr '((a b) (d e) (g h))))
(check-equal (1 4 27 256 3125) (map! (lambda (n) (expt n n)) '(1 2 3 4 5)))
(check-equal (5 7 9) (map! + '(1 2 3) '(4 5 6)))
(check-equal #t
    (let ((ret (let ((count 0)) (map! (lambda (ignored) (set! count (+ count 1)) count) '(a b)))))
        (or (equal? ret '(1 2)) (equal? ret '(2 1)))))
(check-equal (4 1 5 1) (map! + '(3 1 4 1) (circular-list 1 0)))

(check-equal (b e h) (map-in-order cadr '((a b) (d e) (g h))))
(check-equal (1 4 27 256 3125) (map-in-order (lambda (n) (expt n n)) '(1 2 3 4 5)))
(check-equal (5 7 9) (map-in-order + '(1 2 3) '(4 5 6)))
(check-equal #t
    (let ((ret (let ((count 0))
                   (map-in-order (lambda (ignored) (set! count (+ count 1)) count) '(a b)))))
        (or (equal? ret '(1 2)) (equal? ret '(2 1)))))
(check-equal (4 1 5 1) (map-in-order + '(3 1 4 1) (circular-list 1 0)))

(check-equal #((0 1 2 3) (1 2 3) (2 3) (3))
    (let ((v (make-vector 4)))
        (pair-for-each (lambda (lst) (vector-set! v (car lst) lst)) '(0 1 2 3)) v))
(check-equal #((0 . a) (1 . b) (2 . c) (3 . d))
    (let ((v (make-vector 4)))
        (pair-for-each (lambda (fst snd) (vector-set! v (car fst) (cons (car fst) (car snd))))
                '(0 1 2 3) '(a b c d))
        v))

(check-equal (1 9 49) (filter-map (lambda (x) (and (number? x) (* x x))) '(a 1 b 3 c 7)))
(check-equal (0 2 4 6)
    (filter-map (lambda (n b) (and b n)) '(0 1 2 3 4 5 6) (circular-list #t #f)))

(check-equal (0 8 8 -4) (filter even? '(0 7 8 8 43 -4)))
(check-equal () (filter even? '(1 3 5 7)))
(check-equal () (filter even? '()))

(check-equal ((one four five) (2 3 6))
    (let-values (((nums syms) (partition symbol? '(one 2 3 four five 6))))
        (list nums syms)))

(check-equal (0 8 8 -4) (remove odd? '(0 7 8 8 43 -4)))
(check-equal () (remove odd? '(1 3 5 7)))
(check-equal () (remove odd? '()))
(check-equal (7 43) (remove even? '(0 7 8 8 43 -4)))

(check-equal (0 8 8 -4) (filter! even? '(0 7 8 8 43 -4)))
(check-equal () (filter! even? '(1 3 5 7)))
(check-equal () (filter! even? '()))

(check-equal ((one four five) (2 3 6))
    (let-values (((nums syms) (partition! symbol? '(one 2 3 four five 6))))
        (list nums syms)))

(check-equal (0 8 8 -4) (remove! odd? '(0 7 8 8 43 -4)))
(check-equal () (remove! odd? '(1 3 5 7)))
(check-equal () (remove! odd? '()))
(check-equal (7 43) (remove! even? '(0 7 8 8 43 -4)))

(check-equal 2 (find even? '(1 2 3)))
(check-equal #f (find even? '(1 7 3)))
(check-equal 2 (find even? '(1 2 . x)))
(check-equal 6 (find even? (circular-list 1 6 3)))
(check-equal 4 (find even? '(3 1 4 1 5 9)))

(check-equal (2 3) (find-tail even? '(1 2 3)))
(check-equal #f (find-tail even? '(1 7 3)))
(check-equal (2 . x)  (find-tail even? '(1 2 . x)))
(check-equal (4 1 5 9) (find-tail even? '(3 1 4 1 5 9)))
(check-equal (-8 -5 0 0) (find-tail even? '(3 1 37 -8 -5 0 0)))
(check-equal #f (find-tail even? '(3 1 37 -5)))

(check-equal (2 18) (take-while even? '(2 18 3 10 22 9)))
(check-equal () (take-while odd? '(2 18 4 10 22 20)))
(check-equal (2 18 4 10 22 20) (take-while even? '(2 18 4 10 22 20)))

(check-equal (2 18) (take-while! even? '(2 18 3 10 22 9)))
(check-equal () (take-while! odd? '(2 18 4 10 22 20)))
(check-equal (2 18 4 10 22 20) (take-while! even? '(2 18 4 10 22 20)))

(check-equal (3 10 22 9) (drop-while even? '(2 18 3 10 22 9)))

(check-equal ((2 18) (3 10 22 9))
    (let-values (((pre suf) (span even? '(2 18 3 10 22 9)))) (list pre suf)))
(check-equal ((2 18 4 10 22 20) ())
    (let-values (((pre suf) (span even? '(2 18 4 10 22 20)))) (list pre suf)))
(check-equal (() (2 18 4 10 22 20))
    (let-values (((pre suf) (span odd? '(2 18 4 10 22 20)))) (list pre suf)))

(check-equal ((3 1) (4 1 5 9))
    (let-values (((pre suf) (break even? '(3 1 4 1 5 9)))) (list pre suf)))
(check-equal ((2 18 4 10 22 20) ())
    (let-values (((pre suf) (break odd? '(2 18 4 10 22 20)))) (list pre suf)))
(check-equal (() (2 18 4 10 22 20))
    (let-values (((pre suf) (break even? '(2 18 4 10 22 20)))) (list pre suf)))

(check-equal #t (any even? '(1 2 3)))
(check-equal #f (any even? '(1 7 3)))
(check-equal #t (any even? (circular-list 1 6 3)))
(check-equal #t (any integer? '(a 3 b 2.7)))
(check-equal #f (any integer? '(a 3.1 b 2.7)))
(check-equal #t (any < '(3 1 4 1 5) '(2 7 1 8 2)))
(check-equal #f (any even? '()))
(check-equal #f (any < '(3 7 4 1 5) '(2 7)))

(check-equal #t (every even? '(2 4 6 8)))
(check-equal #f (every even? '(2 4 5 8)))
(check-equal #t (every < '(1 2 3 4 5) '(2 3 4)))
(check-equal #t (every even? '()))

(check-equal 2 (list-index even? '(3 1 4 1 5 9)))
(check-equal 1(list-index < '(3 1 4 1 5 9 2 5 6) '(2 7 1 8 2)))
(check-equal #f (list-index = '(3 1 4 1 5 9 2 5 6) '(2 7 1 8 2)))

(check-equal (a b c) (memq 'a '(a b c)))
(check-equal (b c) (memq 'b '(a b c)))
(check-equal #f (memq 'a '(b c d)))
(check-equal #f (memq (list 'a) '(b (a) c)))
(check-equal ((a) c) (member (list 'a) '(b (a) c)))
(check-equal (101 102) (memq 101 '(100 101 102)))
(check-equal (101 102) (memv 101 '(100 101 102)))

(check-equal (b c d) (delete 'a '(a b c d)))
(check-equal (b c d) (delete 'a '(a b a a c d a)))
(check-equal (b c d) (delete 'a '(b c d)))
(check-equal () (delete 'a '(a a a a)))
(check-equal () (delete 'a '()))
(check-equal (11 12 13) (delete 11 '(8 9 10 11 12 13) >))

(check-equal (a b c z) (delete-duplicates '(a b a c a b c z)))
(check-equal ((a . 3) (b . 7) (c . 1))
    (delete-duplicates '((a . 3) (b . 7) (a . 9) (c . 1))
            (lambda (x y) (eq? (car x) (car y)))))

(check-equal (a 1) (assq 'a '((a 1) (b 2) (c 3))))
(check-equal (b 2) (assq 'b '((a 1) (b 2) (c 3))))
(check-equal #f (assq 'd '((a 1) (b 2) (c 3))))
(check-equal #f (assq (list 'a) '(((a)) ((b)) ((c)))))
(check-equal ((a)) (assoc (list 'a) '(((a)) ((b)) ((c)))))
(check-equal (5 7) (assq 5 '((2 3) (5 7) (11 13))))
(check-equal (5 7) (assv 5 '((2 3) (5 7) (11 13))))

(check-equal ((a . 1)) (alist-cons 'a 1 '()))
(check-equal ((a . 1) (b . 2)) (alist-cons 'a 1 '((b . 2))))

(check-equal ((a . 1) (b . 2) (c . 3)) (alist-copy '((a . 1) (b . 2) (c . 3))))
(check-equal () (alist-copy '()))

(check-equal #t (lset<= eq? '(a) '(a b a) '(a b c c)))
(check-equal #f (lset<= eq? '(a b a) '(a) '(a b c c)))
(check-equal #t (lset<= eq?))
(check-equal #t (lset<= eq? '(a)))

(check-equal #t (lset= eq? '(b e a) '(a e b) '(e e b a)))
(check-equal #t (lset= eq?))
(check-equal #t (lset= eq? '(a)))
(check-equal #f (lset= eq? '(b e a d) '(a e b) '(e e b a)))
(check-equal #f (lset= eq? '(b e a) '(a d e b) '(e e b a)))
(check-equal #f (lset= eq? '(b e a) '(a e b) '(e d e b a)))

(check-equal (u o i a b c d c e) (lset-adjoin eq? '(a b c d c e) 'a 'e 'i 'o 'u))

(check-equal (u o i a b c d e) (lset-union eq? '(a b c d e) '(a e i o u)))
(check-equal (x a a c) (lset-union eq? '(a a c) '(x a x)))
(check-equal () (lset-union eq?))
(check-equal (a b c) (lset-union eq? '(a b c)))

(check-equal (a e) (lset-intersection eq? '(a b c d e) '(a e i o u)))
(check-equal (a x a) (lset-intersection eq? '(a x y a) '(x a x z)))
(check-equal (a b c) (lset-intersection eq? '(a b c)))

(check-equal (b c d) (lset-difference eq? '(a b c d e) '(a e i o u)))
(check-equal (a b c) (lset-difference eq? '(a b c)))

(check-equal (b c d i o u) (lset-xor eq? '(a b c d e) '(a e i o u)))
(check-equal () (lset-xor eq?))
(check-equal (a b c d e) (lset-xor eq? '(a b c d e)))

(check-equal ((b c d) (a e))
    (let-values (((diff inter) (lset-diff+intersection eq? '(a b c d e) '(a e i o u))))
        (list diff inter)))
