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

(import (scheme box))

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
(check-error (assertion-violation bitwise-and) (first-set-bit 12.34))
(check-error (assertion-violation bitwise-and) (first-set-bit 12/34))

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

(import (scheme list))

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

;;
;; ---- SRFI 128: Comparators ----
;;

(import (foment base))
(import (scheme char))
(import (scheme comparator))

(test-when (eq? (cdr (assq 'collector (config))) 'generational)
    (display "warning: skipping comparator tests because of bugs in generational collector")
    (newline))

(test-when (not (eq? (cdr (assq 'collector (config))) 'generational))

(define default-comparator (make-default-comparator))
(define boolean-comparator
    (make-comparator boolean? eq? (lambda (x y) (and (not x) y)) boolean-hash))
(define char-comparator
    (make-comparator char? char=? char<? char-hash))
(define string-ci-comparator
    (make-comparator string? string-ci=? string-ci<? string-ci-hash))
(define symbol-comparator
    (make-comparator symbol? eq?
            (lambda (obj1 obj2) (string<? (symbol->string obj1) (symbol->string obj2)))
            symbol-hash))
(define fail-comparator
    (make-comparator (lambda (obj) #t) eq? #f #f))
(define number-comparator
    (make-comparator number? = < number-hash))
(define pnb-comparator
    (make-pair-comparator number-comparator boolean-comparator))
(define ln-comparator
    (make-list-comparator number-comparator list? null? car cdr))
(define vn-comparator
    (make-vector-comparator number-comparator vector? vector-length vector-ref))
(define eq-comparator (make-eq-comparator))
(define eqv-comparator (make-eqv-comparator))
(define equal-comparator (make-equal-comparator))

(check-equal #t (comparator? (make-comparator (lambda (obj) #t) eq? #f #f)))
(check-equal #t (comparator? (make-default-comparator)))
(check-equal #f (comparator? (list 'make-comparator (lambda (obj) #t) eq? #f #f)))
(check-equal #t (comparator? boolean-comparator))
(check-equal #t (comparator? symbol-comparator))

(check-equal #t (comparator-ordered? boolean-comparator))
(check-equal #f (comparator-ordered? (make-comparator (lambda (obj) #t) eq? #f #f)))
(check-equal #f (comparator-ordered? fail-comparator))

(check-equal #t (comparator-hashable? boolean-comparator))
(check-equal #f (comparator-hashable? (make-comparator (lambda (obj) #t) eq? #f #f)))
(check-equal #f (comparator-hashable? fail-comparator))

(check-error (assertion-violation no-ordering-predicate)
    ((comparator-ordering-predicate fail-comparator) 1 2))
(check-error (assertion-violation no-hash-function)
    ((comparator-hash-function fail-comparator) 1))

(check-equal #t (comparator-test-type pnb-comparator '(12 . #t)))
(check-equal #f (comparator-test-type pnb-comparator '(#t . 12)))
(check-equal #f (comparator-test-type pnb-comparator '(#t 12)))
(check-equal #f (comparator-test-type pnb-comparator #t))
(check-equal #f (comparator-test-type pnb-comparator 12))
(check-equal #t (=? pnb-comparator '(12 . #t) '(12 . #t)))
(check-equal #f (=? pnb-comparator '(12 . #t) '(11 . #t)))
(check-equal #f (=? pnb-comparator '(12 . #t) '(12 . #f)))
(check-equal #t (<? pnb-comparator '(12 . #t) '(13 . #t)))
(check-equal #t (<? pnb-comparator '(12 . #f) '(12 . #t)))
(check-equal #f (<? pnb-comparator '(12 . #t) '(12 . #t)))
(check-equal #f (<? pnb-comparator '(13 . #t) '(12 . #t)))
(check-equal #f (<? pnb-comparator '(12 . #t) '(12 . #f)))
(check-equal #t
    (= (comparator-hash pnb-comparator '(12 . #t)) (comparator-hash pnb-comparator '(12 . #t))))
(check-equal #f
    (= (comparator-hash pnb-comparator '(13 . #t)) (comparator-hash pnb-comparator '(12 . #t))))
(check-equal #f
    (= (comparator-hash pnb-comparator '(12 . #f)) (comparator-hash pnb-comparator '(12 . #t))))

(check-equal #t (comparator-test-type ln-comparator '(12 34)))
(check-equal #f (comparator-test-type ln-comparator '(12 . 34)))
(check-equal #f (comparator-test-type ln-comparator '(12 34 #t)))
(check-equal #f (comparator-test-type ln-comparator 123))
(check-equal #t (=? ln-comparator '(12 34) '(12 34)))
(check-equal #f (=? ln-comparator '(12 34) '(12 34 56)))
(check-equal #f (=? ln-comparator '(12 34) '(12 35)))
(check-equal #f (=? ln-comparator '(12 34) '(13 34)))
(check-equal #t (<? ln-comparator '(12 34) '(12 35)))
(check-equal #f (<? ln-comparator '(12 34) '(12 34)))
(check-equal #f (<? ln-comparator '(12 34 56) '(12 34)))
(check-equal #t (<? ln-comparator '(12 34) '(12 34 56)))
(check-equal #f (<? ln-comparator '(13 34) '(12 34)))
(check-equal #f (<? ln-comparator '(12 35) '(12 34)))

(check-equal #t (comparator-test-type vn-comparator #(12 34)))
(check-equal #f (comparator-test-type vn-comparator '(12 34)))
(check-equal #f (comparator-test-type vn-comparator #(12 34 #t)))
(check-equal #f (comparator-test-type vn-comparator 123))
(check-equal #t (=? vn-comparator #(12 34) #(12 34)))
(check-equal #f (=? vn-comparator #(12 34) #(12 34 56)))
(check-equal #f (=? vn-comparator #(12 34) #(12 35)))
(check-equal #f (=? vn-comparator #(12 34) #(13 34)))
(check-equal #t (<? vn-comparator #(12 34) #(12 35)))
(check-equal #f (<? vn-comparator #(12 34) #(12 34)))
(check-equal #f (<? vn-comparator #(12 34 56) #(12 34)))
(check-equal #t (<? vn-comparator #(12 34) #(12 34 56)))
(check-equal #f (<? vn-comparator #(13 34) #(12 34)))
(check-equal #f (<? vn-comparator #(12 35) #(12 34)))

(check-equal #t (comparator-test-type eq-comparator #t))
(check-equal #t (comparator-test-type eq-comparator (lambda (obj) obj)))
(check-equal #t (comparator-test-type eq-comparator 123))
(check-equal #t (comparator-test-type eq-comparator '(1 2 3)))
(check-equal #t (comparator-test-type eq-comparator cons))
(check-equal #t (=? eq-comparator 123 123))
(check-equal #f (=? eq-comparator 123 123.456))
(check-equal #f (=? eq-comparator '(1 2 3) '(1 2 3)))
(check-equal #t (let ((lst '(1 2 3))) (=? eq-comparator lst lst)))
(check-equal #f (=? eq-comparator #(1 2 3) #(1 2 3)))
(check-equal #t (let ((vec '#(1 2 3))) (=? eq-comparator vec vec)))
(check-equal #t (eq? default-hash (comparator-hash-function eq-comparator)))

(check-equal #t (comparator-test-type eqv-comparator #t))
(check-equal #t (comparator-test-type eqv-comparator (lambda (obj) obj)))
(check-equal #t (comparator-test-type eqv-comparator 123))
(check-equal #t (comparator-test-type eqv-comparator '(1 2 3)))
(check-equal #t (comparator-test-type eqv-comparator cons))
(check-equal #t (=? eqv-comparator 123 123))
(check-equal #f (=? eqv-comparator 123 123.456))
(check-equal #f (=? eqv-comparator '(1 2 3) '(1 2 3)))
(check-equal #t (let ((lst '(1 2 3))) (=? eqv-comparator lst lst)))
(check-equal #f (=? eqv-comparator #(1 2 3) #(1 2 3)))
(check-equal #t (let ((vec '#(1 2 3))) (=? eqv-comparator vec vec)))
(check-equal #t (eq? default-hash (comparator-hash-function eqv-comparator)))

(check-equal #t (comparator-test-type equal-comparator #t))
(check-equal #t (comparator-test-type equal-comparator (lambda (obj) obj)))
(check-equal #t (comparator-test-type equal-comparator 123))
(check-equal #t (comparator-test-type equal-comparator '(1 2 3)))
(check-equal #t (comparator-test-type equal-comparator cons))
(check-equal #t (=? equal-comparator 123 123))
(check-equal #f (=? equal-comparator 123 123.456))
(check-equal #t (=? equal-comparator '(1 2 3) '(1 2 3)))
(check-equal #t (let ((lst '(1 2 3))) (=? equal-comparator lst lst)))
(check-equal #t (=? equal-comparator #(1 2 3) #(1 2 3)))
(check-equal #t (let ((vec '#(1 2 3))) (=? equal-comparator vec vec)))
(check-equal #t (eq? default-hash (comparator-hash-function equal-comparator)))

(check-equal #t (= (boolean-hash #t) (boolean-hash #t)))
(check-equal #t (= (boolean-hash #f) (boolean-hash #f)))
(check-equal #f (= (boolean-hash #f) (boolean-hash #t)))

(check-equal #t (= (char-hash #\a) (char-hash #\a)))
(check-equal #f (= (char-hash #\a) (char-hash #\A)))
(check-equal #f (= (char-hash #\a) (char-hash #\b)))

(check-equal #t (= (char-ci-hash #\a) (char-ci-hash #\a)))
(check-equal #t (= (char-ci-hash #\a) (char-ci-hash #\A)))
(check-equal #f (= (char-ci-hash #\a) (char-ci-hash #\b)))

(check-equal #t (= (string-hash "abcdef") (string-hash "abcdef")))
(check-equal #f (= (string-hash "abcdef") (string-hash "Abcdef")))
(check-equal #f (= (string-hash "abcdef") (string-hash "bcdefg")))

(check-equal #t (= (string-ci-hash "abcdef") (string-ci-hash "abcdef")))
(check-equal #t (= (string-ci-hash "abcdef") (string-ci-hash "Abcdef")))
(check-equal #f (= (string-ci-hash "abcdef") (string-ci-hash "bcdefg")))

(check-equal #t (= (symbol-hash 'abc) (symbol-hash (string->symbol "abc"))))
(check-equal #f (= (symbol-hash 'abc) (symbol-hash 'abcd)))

(check-equal #t (= (number-hash 123) (number-hash 123)))
(check-equal #f (= (number-hash 123) (number-hash 456)))

(check-equal #t (= (number-hash 123.456) (number-hash 123.456)))
(check-equal #f (= (number-hash 123.456) (number-hash 456.789)))

(check-equal #t (<? default-comparator '() '(1 . 2)))
(check-equal #f (<? default-comparator '(1 . 2) '()))
(check-equal #t (<? default-comparator #f #t))
(check-equal #f (<? default-comparator #t #f))

(check-equal #t (eq? default-hash (comparator-hash-function default-comparator)))

(check-equal #t (comparator-test-type boolean-comparator #t))
(check-equal #f (comparator-test-type boolean-comparator 't))
(check-equal #f (comparator-test-type boolean-comparator '()))
(check-error (assertion-violation comparator-check-type)
    (comparator-check-type boolean-comparator 123))
(check-equal #t (=? boolean-comparator #f #f))
(check-equal #f (=? boolean-comparator #t #f))
(check-equal -1 (comparator-if<=> boolean-comparator #f #t -1 0 1))
(check-equal 0 (comparator-if<=> boolean-comparator #f #f -1 0 1))
(check-equal 1 (comparator-if<=> boolean-comparator #t #f -1 0 1))
(check-equal #t (= (comparator-hash boolean-comparator #f)
    (comparator-hash boolean-comparator #f)))
(check-equal #f (= (comparator-hash boolean-comparator #t)
    (comparator-hash boolean-comparator #f)))

(check-equal #t (comparator-test-type char-comparator #\A))
(check-equal #f (comparator-test-type char-comparator 'a))
(check-equal #t (=? char-comparator #\z #\z))
(check-equal #f (=? char-comparator #\Z #\z))
(check-equal -1 (comparator-if<=> char-comparator #\a #\f -1 0 1))
(check-equal 0 (comparator-if<=> char-comparator #\Q #\Q -1 0 1))
(check-equal 1 (comparator-if<=> char-comparator #\F #\A -1 0 1))
(check-equal #t (= (comparator-hash char-comparator #\w)
    (comparator-hash char-comparator #\w)))
(check-equal #f (= (comparator-hash char-comparator #\w)
    (comparator-hash char-comparator #\x)))

(check-equal #t (comparator-test-type string-ci-comparator "abc"))
(check-equal #f (comparator-test-type string-ci-comparator #\a))
(check-equal #t (=? string-ci-comparator "xyz" "xyz"))
(check-equal #t (=? string-ci-comparator "XYZ" "xyz"))
(check-equal #f (=? string-ci-comparator "xyz" "zyx"))
(check-equal -1 (comparator-if<=> string-ci-comparator "abc" "def" -1 0 1))
(check-equal -1 (comparator-if<=> string-ci-comparator "ABC" "def" -1 0 1))
(check-equal -1 (comparator-if<=> string-ci-comparator "abc" "DEF" -1 0 1))
(check-equal 0 (comparator-if<=> string-ci-comparator "ghi" "ghi" -1 0 1))
(check-equal 0 (comparator-if<=> string-ci-comparator "ghi" "GHI" -1 0 1))
(check-equal 1 (comparator-if<=> string-ci-comparator "mno" "jkl" -1 0 1))
(check-equal 1 (comparator-if<=> string-ci-comparator "MNO" "jkl" -1 0 1))
(check-equal 1 (comparator-if<=> string-ci-comparator "mno" "JKL" -1 0 1))
(check-equal #t (= (comparator-hash string-ci-comparator "xyz")
    (comparator-hash string-ci-comparator "xyz")))
(check-equal #t (= (comparator-hash string-ci-comparator "xyz")
    (comparator-hash string-ci-comparator "XYZ")))
(check-equal #f (= (comparator-hash string-ci-comparator "xyz")
    (comparator-hash string-ci-comparator "zyx")))

(check-equal #t (comparator-test-type symbol-comparator 'abc))
(check-equal #f (comparator-test-type symbol-comparator "abc"))
(check-equal #t (=? symbol-comparator 'xyz 'xyz))
(check-equal #f (=? symbol-comparator 'XYZ 'xyz))
(check-equal -1 (comparator-if<=> symbol-comparator 'abc 'def -1 0 1))
(check-equal 0 (comparator-if<=> symbol-comparator 'ghi 'ghi -1 0 1))
(check-equal 1 (comparator-if<=> symbol-comparator 'mno 'jkl -1 0 1))
(check-equal #t (= (comparator-hash symbol-comparator 'pqr)
    (comparator-hash symbol-comparator 'pqr)))
(check-equal #f (= (comparator-hash symbol-comparator 'stu)
    (comparator-hash symbol-comparator 'vwx)))

(check-equal #t (<? default-comparator #f #t))
(check-equal #f (<? default-comparator #f #f))
(check-equal #f (<? default-comparator #t #t))
(check-equal #f (<? default-comparator #t #f))

;; Tests from the reference implementation of comparators.

  (define (vector-cdr vec)
    (let* ((len (vector-length vec))
           (result (make-vector (- len 1))))
      (let loop ((n 1))
        (cond
          ((= n len) result)
          (else (vector-set! result (- n 1) (vector-ref vec n))
                (loop (+ n 1)))))))

  (check-equal #(2 3 4) (vector-cdr '#(1 2 3 4)))
  (check-equal #() (vector-cdr '#(1)))

;  (define default-comparator (make-default-comparator))
  (define real-comparator (make-comparator real? = < number-hash))
  (define degenerate-comparator (make-comparator (lambda (x) #t) equal? #f #f))
;  (define boolean-comparator
;    (make-comparator boolean? eq? (lambda (x y) (and (not x) y)) boolean-hash))
  (define bool-pair-comparator (make-pair-comparator boolean-comparator boolean-comparator))
  (define num-list-comparator
    (make-list-comparator real-comparator list? null? car cdr))
  (define num-vector-comparator
    (make-vector-comparator real-comparator vector? vector-length vector-ref))
  (define vector-qua-list-comparator
    (make-list-comparator
      real-comparator
      vector?
      (lambda (vec) (= 0 (vector-length vec)))
      (lambda (vec) (vector-ref vec 0))
      vector-cdr))
  (define list-qua-vector-comparator
     (make-vector-comparator default-comparator list? length list-ref))
;  (define eq-comparator (make-eq-comparator))
;  (define eqv-comparator (make-eqv-comparator))
;  (define equal-comparator (make-equal-comparator))
;  (define symbol-comparator
;    (make-comparator
;      symbol?
;      eq?
;      (lambda (a b) (string<? (symbol->string a) (symbol->string b)))
;      symbol-hash))

    (check-equal #t (comparator? real-comparator))
    (check-equal #t (not (comparator? =)))
    (check-equal #t (comparator-ordered? real-comparator))
    (check-equal #t (comparator-hashable? real-comparator))
    (check-equal #t (not (comparator-ordered? degenerate-comparator)))
    (check-equal #t (not (comparator-hashable? degenerate-comparator)))

    (check-equal #t (=? boolean-comparator #t #t))
    (check-equal #t (not (=? boolean-comparator #t #f)))
    (check-equal #t (<? boolean-comparator #f #t))
    (check-equal #t (not (<? boolean-comparator #t #t)))
    (check-equal #t (not (<? boolean-comparator #t #f)))

    (check-equal #t (comparator-test-type bool-pair-comparator '(#t . #f)))
    (check-equal #t (not (comparator-test-type bool-pair-comparator 32)))
    (check-equal #t (not (comparator-test-type bool-pair-comparator '(32 . #f))))
    (check-equal #t (not (comparator-test-type bool-pair-comparator '(#t . 32))))
    (check-equal #t (not (comparator-test-type bool-pair-comparator '(32 . 34))))
    (check-equal #t (=? bool-pair-comparator '(#t . #t) '(#t . #t)))
    (check-equal #t (not (=? bool-pair-comparator '(#t . #t) '(#f . #t))))
    (check-equal #t (not (=? bool-pair-comparator '(#t . #t) '(#t . #f))))
    (check-equal #t (<? bool-pair-comparator '(#f . #t) '(#t . #t)))
    (check-equal #t (<? bool-pair-comparator '(#t . #f) '(#t . #t)))
    (check-equal #t (not (<? bool-pair-comparator '(#t . #t) '(#t . #t))))
    (check-equal #t (not (<? bool-pair-comparator '(#t . #t) '(#f . #t))))
    (check-equal #t (not (<? bool-pair-comparator '(#f . #t) '(#f . #f))))

    (check-equal #t (comparator-test-type num-vector-comparator '#(1 2 3)))
    (check-equal #t (comparator-test-type num-vector-comparator '#()))
    (check-equal #t (not (comparator-test-type num-vector-comparator 1)))
    (check-equal #t (not (comparator-test-type num-vector-comparator '#(a 2 3))))
    (check-equal #t (not (comparator-test-type num-vector-comparator '#(1 b 3))))
    (check-equal #t (not (comparator-test-type num-vector-comparator '#(1 2 c))))
    (check-equal #t (=? num-vector-comparator '#(1 2 3) '#(1 2 3)))
    (check-equal #t (not (=? num-vector-comparator '#(1 2 3) '#(4 5 6))))
    (check-equal #t (not (=? num-vector-comparator '#(1 2 3) '#(1 5 6))))
    (check-equal #t (not (=? num-vector-comparator '#(1 2 3) '#(1 2 6))))
    (check-equal #t (<? num-vector-comparator '#(1 2) '#(1 2 3)))
    (check-equal #t (<? num-vector-comparator '#(1 2 3) '#(2 3 4)))
    (check-equal #t (<? num-vector-comparator '#(1 2 3) '#(1 3 4)))
    (check-equal #t (<? num-vector-comparator '#(1 2 3) '#(1 2 4)))
    (check-equal #t (<? num-vector-comparator '#(3 4) '#(1 2 3)))
    (check-equal #t (not (<? num-vector-comparator '#(1 2 3) '#(1 2 3))))
    (check-equal #t (not (<? num-vector-comparator '#(1 2 3) '#(1 2))))
    (check-equal #t (not (<? num-vector-comparator '#(1 2 3) '#(0 2 3))))
    (check-equal #t (not (<? num-vector-comparator '#(1 2 3) '#(1 1 3))))

    (check-equal #t (not (<? vector-qua-list-comparator '#(3 4) '#(1 2 3))))
    (check-equal #t (<? list-qua-vector-comparator '(3 4) '(1 2 3)))

    (define bool-pair (cons #t #f))
    (define bool-pair-2 (cons #t #f))
    (define reverse-bool-pair (cons #f #t))
    (check-equal #t (=? eq-comparator #t #t))
    (check-equal #t (not (=? eq-comparator #f #t)))
    (check-equal #t (=? eqv-comparator bool-pair bool-pair))
    (check-equal #t (not (=? eqv-comparator bool-pair bool-pair-2)))
    (check-equal #t (=? equal-comparator bool-pair bool-pair-2))
    (check-equal #t (not (=? equal-comparator bool-pair reverse-bool-pair)))

    (check-equal #t (exact-integer? (boolean-hash #f)))
    (check-equal #t (not (negative? (boolean-hash #t))))
    (check-equal #t (exact-integer? (char-hash #\a)))
    (check-equal #t (not (negative? (char-hash #\b))))
    (check-equal #t (exact-integer? (char-ci-hash #\a)))
    (check-equal #t (not (negative? (char-ci-hash #\b))))
    (check-equal #t (= (char-ci-hash #\a) (char-ci-hash #\A)))
    (check-equal #t (exact-integer? (string-hash "f")))
    (check-equal #t (not (negative? (string-hash "g"))))
    (check-equal #t (exact-integer? (string-ci-hash "f")))
    (check-equal #t (not (negative? (string-ci-hash "g"))))
    (check-equal #t (= (string-ci-hash "f") (string-ci-hash "F")))
    (check-equal #t (exact-integer? (symbol-hash 'f)))
    (check-equal #t (not (negative? (symbol-hash 't))))
    (check-equal #t (exact-integer? (number-hash 3)))
    (check-equal #t (not (negative? (number-hash 3))))
    (check-equal #t (exact-integer? (number-hash -3)))
    (check-equal #t (not (negative? (number-hash -3))))
    (check-equal #t (exact-integer? (number-hash 3.0)))
    (check-equal #t (not (negative? (number-hash 3.0))))

    (check-equal #t (<? default-comparator '() '(a)))
    (check-equal #t (not (=? default-comparator '() '(a))))
    (check-equal #t (=? default-comparator #t #t))
    (check-equal #t (not (=? default-comparator #t #f)))
    (check-equal #t (<? default-comparator #f #t))
    (check-equal #t (not (<? default-comparator #t #t)))
    (check-equal #t (=? default-comparator #\a #\a))
    (check-equal #t (<? default-comparator #\a #\b))

    (check-equal #t (comparator-test-type default-comparator '()))
    (check-equal #t (comparator-test-type default-comparator #t))
    (check-equal #t (comparator-test-type default-comparator #\t))
    (check-equal #t (comparator-test-type default-comparator '(a)))
    (check-equal #t (comparator-test-type default-comparator 'a))
    (check-equal #t (comparator-test-type default-comparator (make-bytevector 10)))
    (check-equal #t (comparator-test-type default-comparator 10))
    (check-equal #t (comparator-test-type default-comparator 10.0))
    (check-equal #t (comparator-test-type default-comparator "10.0"))
    (check-equal #t (comparator-test-type default-comparator '#(10)))

    (check-equal #t (=? default-comparator '(#t . #t) '(#t . #t)))
    (check-equal #t (not (=? default-comparator '(#t . #t) '(#f . #t))))
    (check-equal #t (not (=? default-comparator '(#t . #t) '(#t . #f))))
    (check-equal #t (<? default-comparator '(#f . #t) '(#t . #t)))
    (check-equal #t (<? default-comparator '(#t . #f) '(#t . #t)))
    (check-equal #t (not (<? default-comparator '(#t . #t) '(#t . #t))))
    (check-equal #t (not (<? default-comparator '(#t . #t) '(#f . #t))))
    (check-equal #t (not (<? default-comparator '#(#f #t) '#(#f #f))))

    (check-equal #t (=? default-comparator '#(#t #t) '#(#t #t)))
    (check-equal #t (not (=? default-comparator '#(#t #t) '#(#f #t))))
    (check-equal #t (not (=? default-comparator '#(#t #t) '#(#t #f))))
    (check-equal #t (<? default-comparator '#(#f #t) '#(#t #t)))
    (check-equal #t (<? default-comparator '#(#t #f) '#(#t #t)))
    (check-equal #t (not (<? default-comparator '#(#t #t) '#(#t #t))))
    (check-equal #t (not (<? default-comparator '#(#t #t) '#(#f #t))))
    (check-equal #t (not (<? default-comparator '#(#f #t) '#(#f #f))))

    (check-equal #t (= (comparator-hash default-comparator #t) (boolean-hash #t)))
    (check-equal #t (= (comparator-hash default-comparator #\t) (char-hash #\t)))
    (check-equal #t (= (comparator-hash default-comparator "t") (string-hash "t")))
    (check-equal #t (= (comparator-hash default-comparator 't) (symbol-hash 't)))
    (check-equal #t (= (comparator-hash default-comparator 10) (number-hash 10)))
    (check-equal #t (= (comparator-hash default-comparator 10.0) (number-hash 10.0)))

    (comparator-register-default!
      (make-comparator procedure? (lambda (a b) #t) (lambda (a b) #f) (lambda (obj) 200)))
    (check-equal #t (=? default-comparator (lambda () #t) (lambda () #f)))
    (check-equal #t (not (<? default-comparator (lambda () #t) (lambda () #f))))
    (check-equal 200 (comparator-hash default-comparator (lambda () #t)))

    (define ttp (lambda (x) #t))
    (define eqp (lambda (x y) #t))
    (define orp (lambda (x y) #t))
    (define hf (lambda (x) 0))
    (define comp (make-comparator ttp eqp orp hf))
    (check-equal #t (equal? ttp (comparator-type-test-predicate comp)))
    (check-equal #t (equal? eqp (comparator-equality-predicate comp)))
    (check-equal #t (equal? orp (comparator-ordering-predicate comp)))
    (check-equal #t (equal? hf (comparator-hash-function comp)))

    (check-equal #t (comparator-test-type real-comparator 3))
    (check-equal #t (comparator-test-type real-comparator 3.0))
    (check-equal #t (not (comparator-test-type real-comparator "3.0")))
    (check-equal #t (comparator-check-type boolean-comparator #t))
    (check-error (assertion-violation comparator-check-type)
            (comparator-check-type boolean-comparator 't))

    (check-equal #t (=? real-comparator 2 2.0 2))
    (check-equal #t (<? real-comparator 2 3.0 4))
    (check-equal #t (>? real-comparator 4.0 3.0 2))
    (check-equal #t (<=? real-comparator 2.0 2 3.0))
    (check-equal #t (>=? real-comparator 3 3.0 2))
    (check-equal #t (not (=? real-comparator 1 2 3)))
    (check-equal #t (not (<? real-comparator 3 1 2)))
    (check-equal #t (not (>? real-comparator 1 2 3)))
    (check-equal #t (not (<=? real-comparator 4 3 3)))
    (check-equal #t (not (>=? real-comparator 3 4 4.0)))

    (check-equal less (comparator-if<=> real-comparator 1 2 'less 'equal 'greater))
    (check-equal equal (comparator-if<=> real-comparator 1 1 'less 'equal 'greater))
    (check-equal greater (comparator-if<=> real-comparator 2 1 'less 'equal 'greater))
    (check-equal less (comparator-if<=> "1" "2" 'less 'equal 'greater))
    (check-equal equal (comparator-if<=> "1" "1" 'less 'equal 'greater))
    (check-equal greater (comparator-if<=> "2" "1" 'less 'equal 'greater))

    (check-equal #t (exact-integer? (hash-bound)))
    (check-equal #t (exact-integer? (hash-salt)))
    (check-equal #t (< (hash-salt) (hash-bound)))
)

;;
;; ---- SRFI 125: Hash Tables ----
;;

(import (foment base))
(import (scheme hash-table))
(import (scheme comparator))
(import (scheme list))

(define default (make-default-comparator))

(check-equal #t (hash-table? (make-eq-hash-table)))
(check-equal #f (hash-table? #(1 2 3 4)))
(check-equal #f (hash-table? '(1 2 3 4)))
(check-equal #t (hash-table? (make-hash-table default)))
(check-equal #t (hash-table? (make-hash-table string=? string-hash 2345)))
(check-equal #t (hash-table? (make-hash-table string-ci=?)))
(check-equal #t (hash-table? (make-hash-table string=? string-hash 2345 'weak-keys)))
(check-equal #t (hash-table? (make-hash-table string-ci=? 'thread-safe)))

(check-error (assertion-violation make-hash-table) (make-hash-table char=?))
(check-error (assertion-violation make-hash-table) (make-hash-table 1234))
(check-error (assertion-violation make-hash-table) (make-hash-table string=? 'bad-food))
(check-error (assertion-violation make-hash-table)
        (make-hash-table string=? string-hash 'weak-knees))
(check-error (assertion-violation make-hash-table)
        (make-hash-table string=? string-hash 'weak-keys 'ephemeral-keys))

(check-error (assertion-violation hash-table) (hash-table))
(check-error (assertion-violation make-hash-table) (hash-table char=?))

(check-equal #t (hash-table? (hash-table default)))

(define htbl (hash-table default 'a "a" 'b "b" 'c "c" 'd "d"))
(check-equal "a" (hash-table-ref/default htbl 'a #f))
(check-equal "b" (hash-table-ref/default htbl 'b #f))
(check-equal "c" (hash-table-ref/default htbl 'c #f))
(check-equal "d" (hash-table-ref/default htbl 'd #f))
(check-equal #f (hash-table-ref/default htbl 'e #f))

(check-error (assertion-violation hash-table-set!) (hash-table-set! htbl 'e "e"))
(check-equal #f (hash-table-ref/default htbl 'e #f))

(check-equal #t
    (hash-table=? default
        (hash-table default 'a 1 'b 2 'c 3 'd 4)
        (hash-table default 'a 1 'b 2 'c 3 'd 4)))
(check-equal #f
    (hash-table=? default
        (hash-table default 'a 1 'b 2 'c 3 'd 5)
        (hash-table default 'a 1 'b 2 'c 3 'd 4)))
(check-equal #f
    (hash-table=? default
        (hash-table default 'a 1 'b 2 'c 3)
        (hash-table default 'a 1 'b 2 'c 3 'd 4)))
(check-equal #f
    (hash-table=? default
        (hash-table default 'a 1 'b 2 'c 3 'd 4)
        (hash-table default 'a 1 'b 2 'c 3)))
(check-equal #f
    (hash-table=? default
        (hash-table default 'a 1 'b 2 'c 3 'e 4)
        (hash-table default 'a 1 'b 2 'c 3 'd 4)))

(check-error (assertion-violation hash-table-unfold) (hash-table-unfold))
(check-error (assertion-violation make-hash-table)
    (hash-table-unfold
        (lambda (seed) (> seed 8))
        (lambda (seed) (values (number->string seed) seed))
        (lambda (seed) (+ seed 1))
        1 char=?))

(check-equal #t
    (hash-table=? default
        (hash-table default "1" 1 "2" 2 "3" 3 "4" 4 "5" 5 "6" 6 "7" 7 "8" 8)
        (hash-table-unfold
            (lambda (seed) (> seed 8))
            (lambda (seed) (values (number->string seed) seed))
            (lambda (seed) (+ seed 1))
            1 default)))

(check-equal 8
    (hash-table-size
        (hash-table-unfold
            (lambda (seed) (> seed 8))
            (lambda (seed) (values (number->string seed) seed))
            (lambda (seed) (+ seed 1))
            1 default)))

(check-error (assertion-violation alist->hash-table) (alist->hash-table '()))
(check-error (assertion-violation make-hash-table) (alist->hash-table '() char=?))
(check-error (assertion-violation car) (alist->hash-table '(a) default))

(check-equal #t
    (hash-table=? default
        (hash-table default 'a 1 'b 2 'c 3 'd 4 'e 5)
        (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4) (e . 5)) default)))

(check-equal #t
    (hash-table=? default
        (hash-table default 'a 1 'b 2 'c 3)
        (alist->hash-table '((a . 1) (b . 2) (c . 3) (a . 4) (b . 5)) default)))

(check-error (assertion-violation hash-table-contains?)
    (hash-table-contains? htbl))
(check-error (assertion-violation hash-table-contains?)
    (hash-table-contains? htbl 123 456))
(check-equal #t (hash-table-contains? (hash-table default 'a 1) 'a))
(check-equal #f (hash-table-contains? (hash-table default 'a 1) 'b))

(check-equal #t (hash-table-empty? (hash-table default)))
(check-equal #f (hash-table-empty? (hash-table default 'a 1)))

(check-equal #t (hash-table-mutable? (make-hash-table default)))
(check-equal #f (hash-table-mutable? (hash-table default)))
(check-equal #t
    (hash-table-mutable?
        (hash-table-unfold
            (lambda (seed) (> seed 8))
            (lambda (seed) (values (number->string seed) seed))
            (lambda (seed) (+ seed 1))
            1 default)))
(check-equal #t (hash-table-mutable? (alist->hash-table '() default)))

(define htbl2 (hash-table default 'a 1 'b 2 'c 3 'd 4))

(check-error (assertion-violation hash-table-ref) (hash-table-ref))
(check-error (assertion-violation hash-table-ref) (hash-table-ref htbl2))
(check-error (assertion-violation hash-table-ref) (hash-table-ref htbl2 'e))
(check-equal 1 (hash-table-ref htbl2 'a))
(check-equal 10 (hash-table-ref htbl2 'e (lambda () 10)))
(check-equal 30 (hash-table-ref htbl2 'c (lambda () #f) (lambda (val) (* val 10))))

(check-error (assertion-violation hash-table-ref/default) (hash-table-ref/default htbl2))
(check-error (assertion-violation hash-table-ref/default) (hash-table-ref/default htbl2 'e))
(check-equal 1 (hash-table-ref/default htbl2 'a 10))
(check-equal 10 (hash-table-ref/default htbl2 'e 10))

(define (test-hash-table initial test expected)
    (let ((htbl (alist->hash-table initial default)))
        (test htbl)
        (hash-table=? default htbl (alist->hash-table expected default))))

(check-equal #t
    (test-hash-table
        '((a . 1) (b . 2) (c . 3) (d . 4) (e . 5))
        (lambda (htbl)
            (hash-table-set! htbl 'a 10)
            (hash-table-set! htbl 'b 20 'c 30)
            (hash-table-set! htbl))
        '((a . 10) (b . 20) (c . 30) (d . 4) (e . 5))))
(check-error (assertion-violation hash-table-set!)
    (hash-table-set! (hash-table default 'a 1) 'a 10))

(check-equal #t
    (test-hash-table
        '((a . 1) (b . 2) (c . 3) (d . 4) (e . 5))
        (lambda (htbl)
            (hash-table-delete! htbl 'a)
            (hash-table-delete! htbl 'b 'c)
            (hash-table-delete! htbl)
            (hash-table-delete! htbl 'f))
        '((d . 4) (e . 5))))
(check-error (assertion-violation hash-table-delete!)
    (hash-table-delete! (hash-table default 'a 1) 'a))

(check-equal #t
    (test-hash-table
        '((a . 1) (b . 2) (c . 3) (d . 4) (e . 5))
        (lambda (htbl)
            (hash-table-intern! htbl 'a (lambda () 100))
            (hash-table-intern! htbl 'f (lambda () 6)))
        '((a . 1) (b . 2) (c . 3) (d . 4) (e . 5) (f . 6))))
(check-error (assertion-violation hash-table-intern!)
    (hash-table-intern! (hash-table default 'a 1) 'b 2))

(check-equal #t
    (test-hash-table
        '((a . 1) (b . 2) (c . 3) (d . 4) (e . 5))
        (lambda (htbl)
            (hash-table-update! htbl 'a (lambda (val) (* val 100)))
            (hash-table-update! htbl 'f (lambda (val) (- val)) (lambda () 6)))
        '((a . 100) (b . 2) (c . 3) (d . 4) (e . 5) (f . -6))))
(check-error (assertion-violation hash-table-set!)
    (hash-table-update! (hash-table default 'a 1) 'a (lambda (val) (* val 10))))

(check-equal #t
    (test-hash-table
        '((a . 1) (b . 2) (c . 3) (d . 4) (e . 5))
        (lambda (htbl)
            (hash-table-update!/default htbl 'a (lambda (val) (* val 100)) -1)
            (hash-table-update!/default htbl 'f (lambda (val) (- val)) 6))
        '((a . 100) (b . 2) (c . 3) (d . 4) (e . 5) (f . -6))))
(check-error (assertion-violation hash-table-set!)
    (hash-table-update!/default (hash-table default 'a 1) 'a (lambda (val) (* val 10)) 6))

(check-equal #t
    (let* ((alist '((a . 1) (b . 2) (c . 3) (d . 4) (e . 5)))
            (htbl (alist->hash-table alist default)))
        (define (accum lst)
            (if (> (hash-table-size htbl) 0)
                (accum (cons (let-values (((key val) (hash-table-pop! htbl)))
                                 (cons key val)) lst))
                lst))
        (hash-table=? default (alist->hash-table (accum '()) default)
            (alist->hash-table alist default))))

(check-equal (a . 1)
    (let-values (((key val) (hash-table-pop! (alist->hash-table '((a . 1)) default))))
        (cons key val)))
(check-error (assertion-violation hash-table-pop!) (hash-table-pop! (hash-table default 'a 1)))
(check-error (assertion-violation %hash-table-pop!)
    (hash-table-pop! (alist->hash-table '() default)))

(check-equal 0
    (let ((htbl (alist->hash-table '((a . 1) (b . 2)) default)))
        (hash-table-clear! htbl)
        (hash-table-size htbl)))
(check-error (assertion-violation hash-table-clear!) (hash-table-clear! (hash-table default 'a 1)))

(check-equal 0 (hash-table-size (hash-table default)))
(check-equal 3 (hash-table-size (hash-table default 'a 1 'b 2 'c 3)))

(check-equal #t
    (lset= eq? '(a b c d)
        (hash-table-keys (hash-table default 'a 1 'b 2 'c 3 'd 4))))

(check-equal #t
    (lset= eq? '(1 2 3 4)
        (hash-table-values (hash-table default 'a 1 'b 2 'c 3 'd 4))))

(check-equal #t
    (lset= equal? '((a . 1) (b . 2) (c . 3) (d . 4))
        (let-values (((keys values) (hash-table-entries (hash-table default 'a 1 'b 2 'c 3 'd 4))))
            (map cons keys values))))

(check-equal 4
    (hash-table-find
        (lambda (key val) (if (eq? key 'd) val #f))
        (hash-table default 'a 1 'b 2 'c 3 'd 4)
        (lambda () 'not-found)))

(check-equal not-found
    (hash-table-find
        (lambda (key val) (if (eq? key 'e) val #f))
        (hash-table default 'a 1 'b 2 'c 3 'd 4)
        (lambda () 'not-found)))

(check-equal 3
    (hash-table-count (lambda (key val) (odd? val))
        (hash-table default 'a 1 'b 2 'c 3 'd 4 'e 5)))

(check-equal #t
    (hash-table=? default (hash-table default 'a 10 'b 20 'c 30 'd 40)
        (hash-table-map
            (lambda (val) (* val 10))
            default
            (hash-table default 'a 1 'b 2 'c 3 'd 4))))

(check-equal #t
    (lset= equal? '((a . 1) (b . 2) (c . 3) (d . 4))
        (let ((lst '()))
            (hash-table-for-each (lambda (key val) (set! lst (cons (cons key val) lst)))
                    (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default))
            lst)))

(check-error (assertion-violation hash-table-map!)
    (hash-table-map! (lambda (key val) (* val 10)) (hash-table default 'a 1 'b 2)))

(check-equal #t
    (hash-table=? default (alist->hash-table '((a . 1) (b . 4) (c . 9) (d . 16)) default)
        (let ((htbl (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default)))
            (hash-table-map! (lambda (key val) (* val val)) htbl)
            htbl)))

(check-equal #t
    (lset= equal? '((a . 1) (b . 2) (c . 3) (d . 4))
        (hash-table-map->list (lambda (key val) (cons key val))
            (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default))))

(check-equal #t
    (lset= equal? '((a . 1) (b . 2) (c . 3) (d . 4))
        (hash-table-fold (lambda (key val lst) (cons (cons key val) lst)) '()
            (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default))))

(check-equal #t
    (hash-table=? default (alist->hash-table '((a . 1) (c . 3)) default)
        (let ((htbl (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default)))
            (hash-table-prune! (lambda (key val) (even? val)) htbl)
            htbl)))

(check-equal #t
    (let ((htbl (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default)))
        (hash-table=? default htbl (hash-table-copy htbl))))

(check-equal #t
    (let ((htbl (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default)))
        (hash-table=? default htbl (hash-table-copy htbl #t))))

(check-equal #t
    (let ((htbl (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default)))
        (hash-table=? default htbl (hash-table-copy htbl #f))))

(check-error (assertion-violation hash-table-set!)
    (let ((htbl (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default)))
        (hash-table-set! (hash-table-copy htbl) 'a 10)))

(check-error (assertion-violation hash-table-set!)
    (let ((htbl (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default)))
        (hash-table-set! (hash-table-copy htbl #f) 'a 10)))

(check-equal #f
    (let* ((htbl (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default))
            (copy (hash-table-copy htbl #t)))
        (hash-table-set! copy 'a 10)
        (hash-table=? default htbl copy)))

(check-equal #t
    (lset= equal? '((a . 1) (b . 2) (c . 3) (d . 4))
        (hash-table->alist (alist->hash-table '((a . 1) (b . 2) (c . 3) (d . 4)) default))))

(define (test-hash-set initial1 initial2 proc expected)
    (let ((htbl1 (alist->hash-table initial1 default))
            (htbl2 (alist->hash-table initial2 default)))
        (if (not (eq? htbl1 (proc htbl1 htbl2)))
            #f
            (lset= equal? expected (hash-table->alist htbl1)))))

(check-equal #t
    (test-hash-set
        '((a . 1) (b . 2) (c . 3))
        '((c . 4) (d . 5) (e . 6))
        hash-table-union!
        '((a . 1) (b . 2) (c . 3) (d . 5) (e . 6))))

(check-equal #t
    (test-hash-set
        '((a . 1) (b . 2) (c . 3))
        '((c . 4) (d . 5) (e . 6))
        hash-table-intersection!
        '((c . 3))))

(check-equal #t
    (test-hash-set
        '((a . 1) (b . 2) (c . 3))
        '((c . 4) (d . 5) (e . 6))
        hash-table-difference!
        '((a . 1) (b . 2))))

(check-equal #t
    (test-hash-set
        '((a . 1) (b . 2) (c . 3))
        '((c . 4) (d . 5) (e . 6))
        hash-table-xor!
        '((a . 1) (b . 2) (d . 5) (e . 6))))

(define (test-hash-table-add htbl size max make-key)
    (let ((vec (make-vector size #f)))
        (define (test-add n)
            (if (< n max)
                (let* ((idx (random size))
                        (key (make-key idx)))
                    (if (not (vector-ref vec idx))
                        (begin
                            (vector-set! vec idx #t)
                            (hash-table-set! htbl key idx)))
                    (test-add (+ n 1)))))
        (test-add 0)
        vec))

(define (test-hash-table-ref htbl vec size make-key)
    (define (test-ref idx cnt)
        (if (< idx size)
            (begin
                (if (vector-ref vec idx)
                    (let ((key (make-key idx)))
                        (if (not (= (hash-table-ref/default htbl key 'fail) idx))
                            (begin
                                (display "failed: hash-table-ref/default: ")
                                (display idx)
                                (newline)
                                (test-ref (+ idx 1) cnt))
                            (test-ref (+ idx 1) (+ cnt 1))))
                    (test-ref (+ idx 1) cnt)))
            cnt))
    (test-ref 0 0))

(define (make-string-key idx)
    (number->string idx))

(define htbl (make-hash-table string=? string-hash))
(define vec (test-hash-table-add htbl 1024 512 make-string-key))
(check-equal #t (= (test-hash-table-ref htbl vec 1024 make-string-key) (hash-table-size htbl)))

;;
;; ---- SRFI 133: Vector Library (R7RS-compatible) ----
;;

(import (scheme vector))

(check-equal #(a b c d) (vector-concatenate '(#(a b) #(c d))))

(check-equal #f (vector-empty? '#(a)))
(check-equal #f (vector-empty? '#(())))
(check-equal #f (vector-empty? '#(#())))
(check-equal #t (vector-empty? '#()))

(check-equal #t (vector= eq? '#(a b c d) '#(a b c d)))
(check-equal #f (vector= eq? '#(a b c d) '#(a b d c)))
(check-equal #f (vector= = '#(1 2 3 4 5) '#(1 2 3 4)))
(check-equal #t (vector= = '#(1 2 3 4) '#(1 2 3 4)))
(check-equal #t (vector= eq?))
(check-equal #t (vector= eq? '#(a)))
(check-equal #f (vector= eq? (vector (vector 'a)) (vector (vector 'a))))
(check-equal #t (vector= equal? (vector (vector 'a)) (vector (vector 'a))))
(check-equal #t
    (vector= = '#(1 2 3 4) '#(1 2 3 4) '#(1 2 3 4) '#(1 2 3 4)))
(check-equal #f
    (vector= = '#(1 2 3 4 5) '#(1 2 3 4) '#(1 2 3 4) '#(1 2 3 4)))
(check-equal #f
    (vector= = '#(1 2 3 4) '#(1 2 3) '#(1 2 3 4) '#(1 2 3 4)))
(check-equal #f
    (vector= = '#(1 2 3 4) '#(1 2 3 4) '#(1 2 3 4 5) '#(1 2 3 4)))
(check-equal #f
    (vector= = '#(1 2 3 4) '#(1 2 3 4) '#(1 2 3 4) '#(1 2 3)))
(check-equal #f
    (vector= = '#(9 2 3 4) '#(1 2 3 4) '#(1 2 3 4) '#(1 2 3 4)))
(check-equal #f
    (vector= = '#(1 2 3 4) '#(1 9 3 4) '#(1 2 3 4) '#(1 2 3 4)))
(check-equal #f
    (vector= = '#(1 2 3 4) '#(1 2 3 4) '#(1 2 9 4) '#(1 2 3 4)))
(check-equal #f
    (vector= = '#(1 2 3 4) '#(1 2 3 4) '#(1 2 3 4) '#(1 2 3 9)))

(check-equal 6
    (vector-fold (lambda (len str) (max (string-length str) len)) 0
            '#("abc" "defghi" "jklmn" "pqrs")))
(check-equal (d c b a) (vector-fold (lambda (tail elt) (cons elt tail)) '() '#(a b c d)))
(check-equal 4
    (vector-fold (lambda (counter n) (if (even? n) (+ counter 1) counter)) 0
            '#(1 2 3 4 5 6 7 8)))

(check-equal (a b c d)
    (vector-fold-right (lambda (tail elt) (cons elt tail)) '() '#(a b c d)))

(check-equal #(1 4 9 16)
    (vector-map (lambda (x) (* x x)) (vector-unfold (lambda (i x) (values x (+ x 1))) 4 1)))
(check-equal #(5 8 9 8 5)
    (vector-map (lambda (x y) (* x y))
            (vector-unfold (lambda (i x) (values x (+ x 1))) 5 1)
            (vector-unfold (lambda (i x) (values x (- x 1))) 5 5)))
(check-equal #t
    (let* ((count 0)
           (ret (vector-map (lambda (ignored-elt) (set! count (+ count 1)) count) '#(a b))))
        (or (equal? ret #(1 2)) (equal? ret #(2 1)))))

(check-equal #(1 4 9 16)
    (vector-map (lambda (x) (* x x)) (vector-unfold (lambda (i x) (values x (+ x 1))) 4 1)))
(check-equal #(5 8 9 8 5)
    (vector-map (lambda (x y) (* x y))
            (vector-unfold (lambda (i x) (values x (+ x 1))) 5 1)
            (vector-unfold (lambda (i x) (values x (- x 1))) 5 5)))
(check-equal #t
    (let* ((count 0)
           (ret (vector-map (lambda (ignored-elt) (set! count (+ count 1)) count) '#(a b))))
        (or (equal? ret #(1 2)) (equal? ret #(2 1)))))

(check-equal #(1 4 9 16)
    (let ((vec (vector-unfold (lambda (i x) (values x (+ x 1))) 4 1)))
        (vector-map! (lambda (x) (* x x)) vec)
        vec))
(check-equal #(5 8 9 8 5)
    (let ((vec1 (vector-unfold (lambda (i x) (values x (+ x 1))) 5 1))
          (vec2 (vector-unfold (lambda (i x) (values x (- x 1))) 5 5)))
        (vector-map! (lambda (x y) (* x y)) vec1 vec2)
        vec1))
(check-equal #t
    (let* ((count 0)
           (ret (vector 'a 'b)))
        (vector-map! (lambda (ignored-elt) (set! count (+ count 1)) count) ret)
        (or (equal? ret #(1 2)) (equal? ret #(2 1)))))

(check-equal 3 (vector-count even? '#(3 1 4 1 5 9 2 5 6)))
(check-equal 2 (vector-count < '#(1 3 6 9) '#(2 4 6 8 10 12)))

(check-equal #(3 4 8 9 14 23 25 30 36) (vector-cumulate + 0 '#(3 1 4 1 5 9 2 5 6)))

(check-equal 2 (vector-index even? '#(3 1 4 1 5 9)))
(check-equal 1 (vector-index < '#(3 1 4 1 5 9 2 5 6) '#(2 7 1 8 2)))
(check-equal #f (vector-index = '#(3 1 4 1 5 9 2 5 6) '#(2 7 1 8 2)))

(check-equal 5 (vector-index-right odd? '#(3 1 4 1 5 9 6)))
(check-equal 3 (vector-index-right < '#(3 1 4 1 5) '#(2 7 1 8 2)))

(check-equal 2 (vector-skip number? '#(1 2 a b 3 4 c d)))
(check-equal 2 (vector-skip = '#(1 2 3 4 5) '#(1 2 -3 4)))

(check-equal 7 (vector-skip-right number? '#(1 2 a b 3 4 c d)))
(check-equal 3 (vector-skip-right = '#(1 2 3 4 5) '#(1 2 -3 -4 5)))

(check-equal (#f 0 1 2 3 4 5 6 7 #f)
    (let ((vec '#(1 2 3 4 5 6 7 8)))
        (map (lambda (val) (vector-binary-search vec val -)) '(0 1 2 3 4 5 6 7 8 9))))
(check-equal (#f 0 1 2 3 4 5 6 #f)
    (let ((vec '#(1 2 3 4 5 6 7)))
        (map (lambda (val) (vector-binary-search vec val -)) '(0 1 2 3 4 5 6 7 8))))

(check-equal #t (vector-any number? '#(1 2 x y z)))
(check-equal #t (vector-any < '#(1 2 3 4 5) '#(2 1 3 4 5)))
(check-equal #f (vector-any number? '#(a b c d e)))
(check-equal #f (vector-any > '#(1 2 3 4 5) '#(1 2 3 4 5)))
(check-equal yes (vector-any (lambda (x) (if (number? x) 'yes #f)) '#(1 2 x y z)))

(check-equal #f (vector-every number? '#(1 2 x y z)))
(check-equal #t (vector-every number? '#(1 2 3 4 5)))
(check-equal #f (vector-every < '#(1 2 3) '#(2 3 3)))
(check-equal #t (vector-every < '#(1 2 3) '#(2 3 4)))
(check-equal nope
    (vector-every (lambda (x) (if (= x 1) 'yeah 'nope)) '#(1 2 3 4 5)))

(check-equal #(2 1 3)
    (let ((v (vector 1 2 3)))
        (vector-swap! v 0 1)
        v))
(check-equal #(1 3 2)
    (let ((v (vector 1 2 3)))
        (vector-swap! v 2 1)
        v))

(check-equal #(4 3 2 1)
    (let ((v (vector 1 2 3 4)))
        (vector-reverse! v)
        v))
(check-equal #(5 4 3 2 1)
    (let ((v (vector 1 2 3 4 5)))
        (vector-reverse! v)
        v))
(check-equal #(1 4 3 2)
    (let ((v (vector 1 2 3 4)))
        (vector-reverse! v 1)
        v))
(check-equal #(1 5 4 3 2)
    (let ((v (vector 1 2 3 4 5)))
        (vector-reverse! v 1)
        v))
(check-equal #(1 3 2 4)
    (let ((v (vector 1 2 3 4)))
        (vector-reverse! v 1 3)
        v))
(check-equal #(1 4 3 2 5)
    (let ((v (vector 1 2 3 4 5)))
        (vector-reverse! v 1 4)
        v))

(check-equal (#(1 2 3 a b c d) 3)
    (call-with-values
        (lambda () (vector-partition number? '#(a 1 b 2 c 3 d)))
        (lambda (vec cnt) (list vec cnt))))
(check-equal (#(1 2 3 4 5) 5)
    (call-with-values
        (lambda () (vector-partition number? '#(1 2 3 4 5)))
        (lambda (vec cnt) (list vec cnt))))
(check-equal (#(a b c d) 0)
    (call-with-values
        (lambda () (vector-partition number? '#(a b c d)))
        (lambda (vec cnt) (list vec cnt))))

(check-equal #(0 -1 -2 -3 -4 -5 -6 -7 -8 -9)
    (vector-unfold (lambda (i x) (values x (- x 1))) 10 0))
(check-equal #(0 1 2 3 4 5) (vector-unfold values 6))
(check-equal #(0 3 4 9 8 15 12 21)
    (vector-unfold (lambda (i x y z) (values (if (even? x) y z) (+ x 1) (+ y 2) (+ z 3))) 8 0 0 0))

(check-equal #(0 0 3 4 9 8 15 12 21 0)
    (let ((vec (make-vector 10 0)))
        (vector-unfold!
            (lambda (i x y z) (values (if (even? x) y z) (+ x 1) (+ y 2) (+ z 3)))
            vec 1 9 0 0 0)
        vec))

(check-equal #((0 . 4) (1 . 3) (2 . 2) (3 . 1) (4 . 0))
    (vector-unfold-right (lambda (i x) (values (cons i x) (+ x 1))) 5 0))
(check-equal #(5 4 3 2 1 0)
    (let ((vec (vector 0 1 2 3 4 5)))
        (vector-unfold-right
            (lambda (i x) (values (vector-ref vec x) (+ x 1)))
            (vector-length vec)
            0)))

(check-equal #(1 2 3 4) (vector-reverse-copy '#(5 4 3 2 1 0) 1 5))
(check-equal #(10 5 4 3 2 60)
    (let ((vec (vector 10 20 30 40 50 60)))
        (vector-reverse-copy! vec 1 #(0 1 2 3 4 5 6 7 8) 2 6)
        vec))

(check-equal #(a b h i) (vector-append-subvectors '#(a b c d e) 0 2 '#(f g h i j) 2 4))
(check-equal #(b c d h i j q r)
    (vector-append-subvectors '#(a b c d e) 1 4 '#(f g h i j) 2 5 #(k l m n o p q r s t) 6 8))

(check-equal (3 2 1) (reverse-vector->list '#(1 2 3)))
(check-equal (3 2) (reverse-vector->list '#(1 2 3) 1))
(check-equal (2 1) (reverse-vector->list '#(1 2 3) 0 2))
(check-equal #(3 2 1) (reverse-list->vector '(1 2 3)))

;;
;; ---- SRFI 14: Character-set Library ----
;;

(import (scheme charset))

(check-equal #t (char-set? (char-set #\A)))
(check-equal #f (char-set? "abcd"))
(check-equal #f (char-set? #\A))

(check-equal #t (char-set=))
(check-equal #t (char-set= (char-set #\A)))
(check-equal #t (char-set= (char-set) (char-set)))
(check-equal #t (char-set= (char-set #\a #\Z) (char-set #\a #\Z)))
(check-equal #t (char-set= (char-set #\a #\z #\A #\Z) (char-set #\A #\z #\a #\Z)))
(check-equal #f (char-set= (char-set #\a #\^ #\Z) (char-set #\a #\Z)))
(check-equal #f (char-set= (char-set #\a #\Z) (char-set #\a #\^ #\Z)))
(check-equal #f (char-set= (char-set #\A #\Z) (char-set #\a #\Z)))
(check-equal #f (char-set= (char-set #\a #\Z) (char-set #\A #\Z)))
(check-equal #f (char-set= (char-set #\a #\z) (char-set #\a #\Z)))
(check-equal #f (char-set= (char-set #\a #\Z) (char-set #\a #\z)))
(check-equal #t (char-set= (char-set #\a #\b #\c #\d) (char-set #\a #\b #\c #\d)))
(check-equal #f (char-set= (char-set #\a #\b #\d) (char-set #\a #\b #\c #\d)))
(check-equal #t
    (char-set= (char-set #\a #\A #\B #\C #\D #\E #\Z) (char-set #\a #\A #\B #\C #\D #\E #\Z)))
(check-equal #t (char-set= (char-set #\a #\b #\c) (char-set #\a #\b #\c) (char-set #\a #\b #\c)))
(check-equal #f (char-set= (char-set #\a #\b #\c) (char-set #\a #\b #\c) (char-set #\a #\b)))

(check-equal #t (char-set<=))
(check-equal #t (char-set<= (char-set #\A)))
(check-equal #t (char-set<= (char-set) (char-set)))
(check-equal #t (char-set<= (char-set #\a #\Z) (char-set #\a #\Z)))
(check-equal #t (char-set<= (char-set #\a #\Z) (char-set #\a #\A #\B #\C #\Z)))
(check-equal #f (char-set<= (char-set #\a #\z #\Z) (char-set #\a #\A #\B #\C #\Z)))
(check-equal #t (char-set<= (char-set) (char-set #\a)))
(check-equal #t (char-set<= (char-set) (char-set #\a) (char-set #\a #\b) (char-set #\a #\b #\c)))
(check-equal #t
    (char-set<=
        (char-set #\l #\m #\n)
        (char-set #\a #\b #\c #\l #\m #\n #\x #\y #\z)))
(check-equal #t
    (char-set<=
        (char-set #\l #\m #\n)
        (char-set #\a #\b #\c #\k #\l #\m #\n #\x #\y #\z)))
(check-equal #t
    (char-set<=
        (char-set #\l #\m #\n)
        (char-set #\a #\b #\c #\k #\l #\m #\n #\o #\x #\y #\z)))

(check-equal #f
    (char-set<=
        (char-set #\l #\m #\n)
        (char-set #\a #\b #\c #\l #\m #\x #\y #\z)))
(check-equal #f
    (char-set<=
        (char-set #\l #\m #\n)
        (char-set #\a #\b #\c #\k #\l #\n #\x #\y #\z)))
(check-equal #f
    (char-set<=
        (char-set #\l #\m #\n)
        (char-set #\a #\b #\c #\k #\l #\m #\o #\x #\y #\z)))

(check-equal #t (>= (char-set-hash (char-set)) 0))
(check-equal #t (>= (char-set-hash (char-set #\a #\b #\c)) 0))
(check-equal #t
    (=
        (char-set-hash (char-set))
        (char-set-hash (char-set))))
(check-equal #t
    (=
        (char-set-hash (char-set #\a #\b #\c))
        (char-set-hash (char-set #\a #\b #\c))))
(check-equal #f
    (=
        (char-set-hash (char-set #\a #\b #\c))
        (char-set-hash (char-set #\x #\y #\z))))
(check-equal #f
    (=
        (char-set-hash (char-set #\a #\b #\c) 123)
        (char-set-hash (char-set #\a #\b #\c))))

(check-equal #t (end-of-char-set? (char-set-cursor (char-set))))
(check-equal #f (end-of-char-set? (char-set-cursor (char-set #\A))))
(check-equal #\A
    (let* ((cset (char-set #\A))
            (cursor (char-set-cursor cset)))
        (char-set-ref cset cursor)))
(check-equal (#\A #\B #\C #\Q #\X #\a #\d #\e #\x #\y #\z)
    (let ((cset (char-set #\Q #\d #\C #\a #\z #\X #\x #\y #\e #\B #\A)))
        (define (walk cursor)
            (if (end-of-char-set? cursor)
                '()
                (cons (char-set-ref cset cursor) (walk (char-set-cursor-next cset cursor)))))
        (walk (char-set-cursor cset))))

(check-equal (#\c #\b #\a)
    (char-set-fold cons '() (char-set #\a #\b #\c)))

(check-equal 5
    (char-set-fold (lambda (ch cnt) (+ cnt 1)) 0 (char-set #\1 #\2 #\3 #\4 #\5)))

(check-equal 3
    (char-set-fold
        (lambda (ch cnt) (if (char-numeric? ch) (+ cnt 1) cnt))
        0 (char-set #\a #\2 #\b #\4 #\c #\6 #\d)))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\y #\z)
        (char-set-unfold car null? cdr '(#\a #\b #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\y #\z)
        (char-set-unfold car null? cdr '(#\a #\b) (char-set #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\y #\z)
        (char-set-unfold! car null? cdr '(#\a #\b) (char-set #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\A #\B #\C)
        (char-set-map char-upcase (char-set #\a #\b #\c))))

(check-equal (#\A #\B #\C #\D)
    (let ((lst '()))
        (char-set-for-each
            (lambda (ch) (set! lst (cons ch lst)))
            (char-set #\A #\B #\C #\D))
        lst))

(check-equal #t
    (char-set=
        (char-set #\A #\B #\C)
        (char-set-map char-upcase (char-set #\a #\b #\c))))

(check-equal #t
    (let ((cset (char-set #\a #\B #\c #\D)))
        (char-set= cset (char-set-copy cset))))

(check-equal #t
    (char-set=
        (list->char-set '(#\1 #\2 #\3 #\a #\b #\c))
        (string->char-set "abc123")))

(check-equal #t
    (char-set=
        (list->char-set '(#\1 #\2 #\3) (char-set #\a #\b #\c))
        (char-set #\1 #\2 #\3 #\a #\b #\c)))

(check-equal #t
    (char-set=
        (string->char-set "123" (char-set #\a #\b #\c))
        (char-set #\1 #\2 #\3 #\a #\b #\c)))

(check-equal #t
    (char-set=
        (list->char-set '(#\1 #\2 #\3 #\a #\b #\c) (char-set #\x #\y #\z))
        (char-set #\x #\y #\z #\1 #\2 #\3 #\a #\b #\c)))

(check-equal #t
    (char-set=
        (string->char-set "xyz123" (char-set #\a #\b #\c))
        (char-set #\x #\y #\z #\1 #\2 #\3 #\a #\b #\c)))

(check-equal #t
    (char-set=
        (char-set #\2 #\4 #\6)
        (char-set-filter char-numeric? (char-set #\a #\2 #\b #\4 #\c #\6 #\d))))

(check-equal #t
    (char-set=
        (char-set #\2 #\4 #\6 #\x #\y #\z)
        (char-set-filter char-numeric? (char-set #\a #\2 #\b #\4 #\c #\6 #\d)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\2 #\4 #\6 #\x #\y #\z)
        (char-set-filter! char-numeric? (char-set #\a #\2 #\b #\4 #\c #\6 #\d)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c)
        (->char-set "abc")))

(check-equal #t
    (char-set=
        (char-set #\a)
        (->char-set #\a)))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c)
        (->char-set (char-set #\a #\b #\c))))

(check-equal 0 (char-set-size (char-set)))
(check-equal 5 (char-set-size (char-set #\a #\b #\c #\d #\e)))

(check-equal 3
    (char-set-count char-numeric? (char-set #\a #\2 #\b #\4 #\c #\6 #\d)))

(check-equal (#\c #\b #\a)
    (char-set->list (char-set #\a #\b #\c)))

(check-equal "cba"
    (char-set->string (char-set #\a #\b #\c)))

(check-equal #t (char-set-contains? (char-set #\a #\b #\c) #\b))
(check-equal #f (char-set-contains? (char-set #\a #\b #\c) #\B))

(check-equal #f
    (char-set-every char-numeric? (char-set #\a #\2 #\b #\4 #\c #\6 #\d)))

(check-equal #t
    (char-set-every char-numeric? (char-set #\2 #\4 #\6)))

(check-equal #t
    (char-set-any char-numeric? (char-set #\a #\2 #\b #\4 #\c #\6 #\d)))

(check-equal #\A
    (char-set-any (lambda (ch) (and (char-upper-case? ch) ch)) (char-set #\a #\b #\A)))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c #\x #\y #\z)
        (char-set-adjoin (char-set #\x #\y #\z) #\a #\b #\c)))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-delete (char-set #\a #\b #\c #\x #\y #\z) #\a #\b #\c)))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-delete (char-set #\x #\y #\z) #\a #\b #\c)))

(check-equal #t
    (char-set=
        char-set:empty
        (char-set-union)))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-union (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c #\x #\y #\z)
        (char-set-union
            (char-set #\a #\b #\c)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c #\x #\y #\z)
        (char-set-union
            (char-set #\a #\b #\c #\x #\y #\z)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\m #\n #\l #\c #\x #\y #\z)
        (char-set-union
            (char-set #\a #\b #\c)
            (char-set #\x #\y #\z)
            (char-set #\m #\n #\l))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c)
        (char-set-union
            (char-set #\a #\b #\c)
            char-set:empty)))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\m #\n #\l #\c #\x #\y #\z)
        (char-set-union
            char-set:empty
            (char-set #\a #\b #\c)
            char-set:empty
            (char-set #\x #\y #\z)
            char-set:empty
            (char-set #\m #\n #\l))))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-union! (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c #\x #\y #\z)
        (char-set-union!
            (char-set #\a #\b #\c)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c #\x #\y #\z)
        (char-set-union!
            (char-set #\a #\b #\c #\x #\y #\z)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\m #\n #\l #\c #\x #\y #\z)
        (char-set-union!
            (char-set #\a #\b #\c)
            (char-set #\x #\y #\z)
            (char-set #\m #\n #\l))))

(check-equal #t
    (char-set=
        char-set:full
        (char-set-intersection)))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-intersection (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        char-set:empty
        (char-set-intersection
            (char-set #\a #\b #\c)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-intersection
            (char-set #\a #\b #\c #\x #\y #\z)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-intersection
            (char-set #\x #\y #\z)
            (char-set #\a #\b #\c #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-intersection
            char-set:ascii
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\A #\B #\C #\X #\Y #\Z)
        (char-set-intersection
            char-set:ascii
            (char-set #\A #\B #\C #\X #\Y #\Z))))

(check-equal #t
    (char-set=
        (char-set #\A #\B #\C #\X #\Y #\Z)
        (char-set-intersection
            (char-set #\A #\B #\C #\X #\Y #\Z)
            char-set:ascii)))

(check-equal #t
    (char-set=
        (char-set #\A #\B #\C #\X #\Y #\Z)
        (char-set-intersection
            char-set:ascii
            (char-set #\A #\B #\C)
            (char-set #\X #\Y #\Z))))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-intersection! (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        char-set:empty
        (char-set-intersection!
            (char-set #\a #\b #\c)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-intersection!
            (char-set #\a #\b #\c #\x #\y #\z)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-intersection!
            (char-set #\x #\y #\z)
            (char-set #\a #\b #\c #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-intersection!
            char-set:ascii
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\A #\B #\C #\X #\Y #\Z)
        (char-set-intersection!
            char-set:ascii
            (char-set #\A #\B #\C #\X #\Y #\Z))))

(check-equal #t
    (char-set=
        (char-set #\A #\B #\C #\X #\Y #\Z)
        (char-set-intersection!
            (char-set #\A #\B #\C #\X #\Y #\Z)
            char-set:ascii)))

(check-equal #t
    (char-set=
        (char-set #\A #\B #\C #\X #\Y #\Z)
        (char-set-intersection!
            char-set:ascii
            (char-set #\A #\B #\C)
            (char-set #\X #\Y #\Z))))

(check-equal #t
    (char-set=
        (char-set)
        (char-set-intersection
            (char-set #\a #\b #\c)
            (char-set #\d #\f #\g))))

(check-equal #t
    (char-set=
        (char-set)
        (char-set-intersection
            (char-set #\d #\f #\g)
            (char-set #\a #\b #\c))))

(check-equal #t
    (char-set=
        (char-set #\d #\e #\h #\i)
        (char-set-intersection
            (char-set #\a #\b #\c #\d #\e #\f #\g #\h #\i #\j)
            (char-set #\d #\e #\h #\i))))

(check-equal #t
    (char-set=
        (char-set #\d #\e #\h #\i #\j)
        (char-set-intersection
            (char-set #\a #\b #\c #\d #\e #\f #\g #\h #\i #\j)
            (char-set #\A #\B #\C #\d #\e #\h #\i #\j #\k #\l))))

(check-equal #t
    (char-set=
        (char-set #\d #\e #\h #\i)
        (char-set-intersection
            (char-set #\d #\e #\h #\i)
            (char-set #\a #\b #\c #\d #\e #\f #\g #\h #\i #\j))))

(check-equal #t
    (char-set=
        (char-set #\d #\e #\h #\i #\j)
        (char-set-intersection
            (char-set #\A #\B #\C #\d #\e #\h #\i #\j #\k #\l)
            (char-set #\a #\b #\c #\d #\e #\f #\g #\h #\i #\j))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c)
        (char-set-difference
            (char-set #\a #\b #\c))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c)
        (char-set-difference
            (char-set #\a #\b #\c)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-difference
            (char-set #\x #\y #\z)
            (char-set #\a #\b #\c))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c)
        (char-set-difference
            (char-set #\a #\b #\c #\x #\y #\z)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-difference
            (char-set #\a #\b #\c #\x #\y #\z)
            (char-set #\a #\b #\c))))

(check-equal #t
    (char-set=
        char-set:empty
        (char-set-difference
            (char-set #\x #\y #\z)
            (char-set #\a #\b #\c #\x #\y #\z))))

(check-equal #t
    (char-set=
        char-set:empty
        (char-set-difference
            (char-set #\a #\b #\c)
            (char-set #\a #\b #\c #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c #\l #\m #\n)
        (char-set-difference
            (char-set #\a #\b #\c #\l #\m #\n #\x #\y #\z)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c #\l #\n)
        (char-set-difference
            (char-set #\a #\b #\c #\l #\m #\n #\x #\y #\z)
            (char-set #\m #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\A #\B #\C #\x #\y #\z)
        (char-set-difference
            (char-set #\A #\B #\C #\a #\b #\c #\x #\y #\z)
            (char-set #\a #\b #\c))))

(check-equal #t
    (char-set=
        char-set:empty
        (char-set-xor)))

(check-equal #t
    (char-set=
        (char-set #\x #\y #\z)
        (char-set-xor (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c #\x #\y #\z)
        (char-set-xor
            (char-set #\a #\b #\c)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c)
        (char-set-xor
            (char-set #\a #\b #\c #\x #\y #\z)
            (char-set #\x #\y #\z))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\m #\n #\l #\c #\x #\y #\z)
        (char-set-xor
            (char-set #\a #\b #\c)
            (char-set #\x #\y #\z)
            (char-set #\m #\n #\l))))

(check-equal #t
    (char-set=
        (char-set #\a #\b #\c #\n #\l #\y #\z)
        (char-set-xor
            (char-set #\a #\b #\c #\m #\x)
            (char-set #\x #\y #\z)
            (char-set #\m #\n #\l))))

(check-equal 128 (char-set-size char-set:ascii))
(check-equal #t (char-set-contains? char-set:ascii #\a))
(check-equal #f (char-set-contains? char-set:ascii (integer->char 128)))

(check-equal 0 (char-set-size char-set:empty))
(check-equal #f (char-set-contains? char-set:empty #\a))

;; From Chibi Scheme

(check-equal #f (char-set? 5))

(check-equal #t (char-set? (char-set #\a #\e #\i #\o #\u)))

(check-equal #t (char-set=))
(check-equal #t (char-set= (char-set)))

(check-equal #t (char-set= (char-set #\a #\e #\i #\o #\u) (string->char-set "ioeauaiii")))

(check-equal #f (char-set= (char-set #\e #\i #\o #\u) (string->char-set "ioeauaiii")))

(check-equal #t (char-set<=))
(check-equal #t (char-set<= (char-set)))

(check-equal #t (char-set<= (char-set #\a #\e #\i #\o #\u) (string->char-set "ioeauaiii")))

(check-equal #t (char-set<= (char-set #\e #\i #\o #\u) (string->char-set "ioeauaiii")))

(check-equal #t (<= 0 (char-set-hash char-set:ascii 100) 99))

(check-equal 4 (char-set-fold (lambda (c i) (+ i 1)) 0 (char-set #\e #\i #\o #\u #\e #\e)))

(check-equal #t
    (char-set=
        (string->char-set "eiaou2468013579999")
        (char-set-unfold car null? cdr
            '(#\a #\e #\i #\o #\u #\u #\u)
            (char-set-intersection char-set:ascii
                (char-set #\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9)))))

(check-equal #t
    (char-set=
        (string->char-set "eiaou246801357999")
        (char-set-unfold! car null? cdr '(#\a #\e #\i #\o #\u) (string->char-set "0123456789"))))

(check-equal #f
    (char-set=
        (string->char-set "eiaou246801357")
        (char-set-unfold! car null? cdr '(#\a #\e #\i #\o #\u) (string->char-set "0123456789"))))

(check-equal #t
    (char-set=
        (string->char-set "97531")
        (let ((cs (string->char-set "0123456789")))
            (char-set-for-each
                (lambda (c) (set! cs (char-set-delete cs c)))
                (string->char-set "02468000"))
            cs)))

(check-equal #f
    (let ((cs (string->char-set "0123456789")))
        (char-set-for-each
            (lambda (c) (set! cs (char-set-delete cs c)))
            (string->char-set "02468"))
        (char-set= cs (string->char-set "7531"))))

(check-equal #t
    (char-set=
        (string->char-set "IOUAEEEE")
        (char-set-map char-upcase (string->char-set "aeiou"))))

(check-equal #f
    (char-set=
        (char-set-map char-upcase (string->char-set "aeiou"))
        (string->char-set "OUAEEEE")))

(check-equal #t
    (char-set=
        (string->char-set "aeiou")
        (char-set-copy (string->char-set "aeiou"))))

(check-equal #t
    (char-set=
        (string->char-set "xy")
        (char-set #\x #\y)))

(check-equal #f (char-set= (char-set #\x #\y #\z) (string->char-set "xy")))

(check-equal #t
    (char-set=
        (string->char-set "xy")
        (list->char-set '(#\x #\y))))

(check-equal #f (char-set= (string->char-set "axy") (list->char-set '(#\x #\y))))

(check-equal #t
    (char-set=
        (string->char-set "xy12345")
        (list->char-set '(#\x #\y) (string->char-set "12345"))))

(check-equal #f
    (char-set=
        (string->char-set "y12345")
        (list->char-set '(#\x #\y) (string->char-set "12345"))))

(check-equal #t
    (char-set=
        (string->char-set "xy12345")
        (list->char-set! '(#\x #\y) (string->char-set "12345"))))

(check-equal #f
    (char-set=
        (string->char-set "y12345")
        (list->char-set! '(#\x #\y) (string->char-set "12345"))))

(define (vowel? ch)
    (member ch '(#\a #\e #\i #\o #\u)))

(check-equal #t
    (char-set=
        (string->char-set "aeiou12345")
        (char-set-filter vowel? char-set:ascii (string->char-set "12345"))))

(check-equal #f
    (char-set=
        (string->char-set "aeou12345")
        (char-set-filter vowel? char-set:ascii (string->char-set "12345"))))

(check-equal #t
    (char-set=
        (string->char-set "aeiou12345")
        (char-set-filter! vowel? char-set:ascii (string->char-set "12345"))))

(check-equal #f
    (char-set=
        (string->char-set "aeou12345")
        (char-set-filter! vowel? char-set:ascii (string->char-set "12345"))))

(check-equal #t
    (char-set=
        (string->char-set "abcdef12345")
        (ucs-range->char-set 97 103 #t (string->char-set "12345"))))
(check-equal #f
    (char-set=
        (string->char-set "abcef12345")
        (ucs-range->char-set 97 103 #t (string->char-set "12345"))))

(check-equal #t
    (char-set=
            (string->char-set "abcdef12345")
            (ucs-range->char-set! 97 103 #t (string->char-set "12345"))))
(check-equal #f
    (char-set=
        (string->char-set "abcef12345")
        (ucs-range->char-set! 97 103 #t (string->char-set "12345"))))

(check-equal #t
    (char-set= (->char-set #\x) (->char-set "x") (->char-set (char-set #\x))))

(check-equal #f
    (char-set= (->char-set #\x) (->char-set "y") (->char-set (char-set #\x))))

(check-equal 10
    (char-set-size (char-set-intersection char-set:ascii (string->char-set "0123456789"))))

(check-equal 5 (char-set-count vowel? char-set:ascii))

(check-equal (#\x) (char-set->list (char-set #\x)))
(check-equal #f (equal? '(#\X) (char-set->list (char-set #\x))))

(check-equal "x" (char-set->string (char-set #\x)))
(check-equal #f (equal? "X" (char-set->string (char-set #\x))))

(check-equal #t (char-set-contains? (->char-set "xyz") #\x))
(check-equal #f (char-set-contains? (->char-set "xyz") #\a))

(check-equal #t (char-set-every char-lower-case? (->char-set "abcd")))
(check-equal #f (char-set-every char-lower-case? (->char-set "abcD")))
(check-equal #t (char-set-any char-lower-case? (->char-set "abcd")))
(check-equal #f (char-set-any char-lower-case? (->char-set "ABCD")))

(check-equal #t
    (char-set=
        (->char-set "ABCD")
        (let ((cs (->char-set "abcd")))
            (let lp ((cur (char-set-cursor cs)) (ans '()))
                (if (end-of-char-set? cur) (list->char-set ans)
                    (lp (char-set-cursor-next cs cur)
                        (cons (char-upcase (char-set-ref cs cur)) ans)))))))

(check-equal #t
    (char-set=
        (->char-set "123xa")
        (char-set-adjoin (->char-set "123") #\x #\a)))
(check-equal #f (char-set= (char-set-adjoin (->char-set "123") #\x #\a) (->char-set "123x")))
(check-equal #t
    (char-set=
        (->char-set "123xa")
        (char-set-adjoin! (->char-set "123") #\x #\a)))
(check-equal #f (char-set= (char-set-adjoin! (->char-set "123") #\x #\a) (->char-set "123x")))

(check-equal #t
    (char-set=
        (->char-set "13")
        (char-set-delete (->char-set "123") #\2 #\a #\2)))
(check-equal #f (char-set= (char-set-delete (->char-set "123") #\2 #\a #\2) (->char-set "13a")))
(check-equal #t
    (char-set=
        (->char-set "13")
        (char-set-delete! (->char-set "123") #\2 #\a #\2)))
(check-equal #f (char-set= (char-set-delete! (->char-set "123") #\2 #\a #\2) (->char-set "13a")))

(define digit (char-set #\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9))
(define hex-digit
    (char-set #\0 #\1 #\2 #\3 #\4 #\5 #\6 #\7 #\8 #\9 #\a #\b #\c #\d #\e #\f
        #\A #\B #\C #\D #\E #\F))

(check-equal #t
    (char-set=
        (->char-set "abcdefABCDEF")
        (char-set-intersection hex-digit (char-set-complement digit))))
(check-equal #t
    (char-set=
        (->char-set "abcdefABCDEF")
        (char-set-intersection! (char-set-complement! (->char-set "0123456789")) hex-digit)))

(check-equal #t
    (char-set=
        (->char-set "abcdefABCDEFghijkl0123456789")
        (char-set-union hex-digit (->char-set "abcdefghijkl"))))
(check-equal #t
    (char-set=
        (->char-set "abcdefABCDEFghijkl0123456789")
        (char-set-union! (->char-set "abcdefghijkl") hex-digit)))

(check-equal #t
    (char-set=
        (->char-set "ghijklmn")
        (char-set-difference (->char-set "abcdefghijklmn") hex-digit)))
(check-equal #t
    (char-set=
        (->char-set "ghijklmn")
        (char-set-difference! (->char-set "abcdefghijklmn") hex-digit)))

(check-equal #t
    (char-set=
        (->char-set "abcdefABCDEF")
        (char-set-xor (->char-set "0123456789") hex-digit)))
(check-equal #t
    (char-set=
        (->char-set "abcdefABCDEF")
        (char-set-xor! (->char-set "0123456789") hex-digit)))

(define letter (string->char-set "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"))

(check-equal (#t . #t)
    (call-with-values
        (lambda ()
            (char-set-diff+intersection hex-digit letter))
        (lambda (d i)
            (cons
                (char-set= d (->char-set "0123456789"))
                (char-set= i (->char-set "abcdefABCDEF"))))))

(check-equal (#t . #t)
    (call-with-values
        (lambda ()
            (char-set-diff+intersection! (char-set-copy hex-digit) (char-set-copy letter)))
        (lambda (d i)
            (cons
                (char-set= d (->char-set "0123456789"))
                (char-set= i (->char-set "abcdefABCDEF"))))))

