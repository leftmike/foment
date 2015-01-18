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
;; ---- SRFI 114: Comparators ----
;;

(import (scheme char))
(import (srfi 114))

(check-equal #t (comparator-test-type boolean-comparator #t))
(check-equal #f (comparator-test-type boolean-comparator 't))
(check-equal #f (comparator-test-type boolean-comparator '()))
(check-error (assertion-violation comparator-check-type)
    (comparator-check-type boolean-comparator 123))
(check-equal #t (comparator-equal? boolean-comparator #f #f))
(check-equal #f (comparator-equal? boolean-comparator #t #f))
(check-equal -1 (comparator-compare boolean-comparator #f #t))
(check-equal 0 (comparator-compare boolean-comparator #f #f))
(check-equal 1 (comparator-compare boolean-comparator #t #f))
(check-equal #t (= (comparator-hash boolean-comparator #f)
    (comparator-hash boolean-comparator #f)))
(check-equal #f (= (comparator-hash boolean-comparator #t)
    (comparator-hash boolean-comparator #f)))

(check-equal #t (comparator-test-type char-comparator #\A))
(check-equal #f (comparator-test-type char-comparator 'a))
(check-equal #t (comparator-equal? char-comparator #\z #\z))
(check-equal #f (comparator-equal? char-comparator #\Z #\z))
(check-equal -1 (comparator-compare char-comparator #\a #\f))
(check-equal 0 (comparator-compare char-comparator #\Q #\Q))
(check-equal 1 (comparator-compare char-comparator #\F #\A))
(check-equal #t (= (comparator-hash char-comparator #\w)
    (comparator-hash char-comparator #\w)))
(check-equal #f (= (comparator-hash char-comparator #\w)
    (comparator-hash char-comparator #\x)))

(check-equal #t (comparator-test-type char-ci-comparator #\A))
(check-equal #f (comparator-test-type char-ci-comparator 'a))
(check-equal #t (comparator-equal? char-ci-comparator #\z #\z))
(check-equal #t (comparator-equal? char-ci-comparator #\Z #\z))
(check-equal #f (comparator-equal? char-ci-comparator #\Z #\y))
(check-equal -1 (comparator-compare char-ci-comparator #\a #\f))
(check-equal -1 (comparator-compare char-ci-comparator #\A #\f))
(check-equal -1 (comparator-compare char-ci-comparator #\a #\F))
(check-equal 0 (comparator-compare char-ci-comparator #\Q #\Q))
(check-equal 0 (comparator-compare char-ci-comparator #\Q #\q))
(check-equal 1 (comparator-compare char-ci-comparator #\F #\A))
(check-equal 1 (comparator-compare char-ci-comparator #\F #\a))
(check-equal 1 (comparator-compare char-ci-comparator #\f #\A))
(check-equal #t (= (comparator-hash char-ci-comparator #\w)
    (comparator-hash char-ci-comparator #\w)))
(check-equal #t (= (comparator-hash char-ci-comparator #\w)
    (comparator-hash char-ci-comparator #\W)))
(check-equal #f (= (comparator-hash char-ci-comparator #\w)
    (comparator-hash char-ci-comparator #\x)))

(check-equal #t (comparator-test-type string-comparator "abc"))
(check-equal #f (comparator-test-type string-comparator #\a))
(check-equal #t (comparator-equal? string-comparator "xyz" "xyz"))
(check-equal #f (comparator-equal? string-comparator "XYZ" "xyz"))
(check-equal -1 (comparator-compare string-comparator "abc" "def"))
(check-equal 0 (comparator-compare string-comparator "ghi" "ghi"))
(check-equal 1 (comparator-compare string-comparator "mno" "jkl"))
(check-equal #t (= (comparator-hash string-comparator "pqr")
    (comparator-hash string-comparator "pqr")))
(check-equal #f (= (comparator-hash string-comparator "stu")
    (comparator-hash string-comparator "vwx")))

(check-equal #t (comparator-test-type string-ci-comparator "abc"))
(check-equal #f (comparator-test-type string-ci-comparator #\a))
(check-equal #t (comparator-equal? string-ci-comparator "xyz" "xyz"))
(check-equal #t (comparator-equal? string-ci-comparator "XYZ" "xyz"))
(check-equal #f (comparator-equal? string-ci-comparator "xyz" "zyx"))
(check-equal -1 (comparator-compare string-ci-comparator "abc" "def"))
(check-equal -1 (comparator-compare string-ci-comparator "ABC" "def"))
(check-equal -1 (comparator-compare string-ci-comparator "abc" "DEF"))
(check-equal 0 (comparator-compare string-ci-comparator "ghi" "ghi"))
(check-equal 0 (comparator-compare string-ci-comparator "ghi" "GHI"))
(check-equal 1 (comparator-compare string-ci-comparator "mno" "jkl"))
(check-equal 1 (comparator-compare string-ci-comparator "MNO" "jkl"))
(check-equal 1 (comparator-compare string-ci-comparator "mno" "JKL"))
(check-equal #t (= (comparator-hash string-ci-comparator "xyz")
    (comparator-hash string-ci-comparator "xyz")))
(check-equal #t (= (comparator-hash string-ci-comparator "xyz")
    (comparator-hash string-ci-comparator "XYZ")))
(check-equal #f (= (comparator-hash string-ci-comparator "xyz")
    (comparator-hash string-ci-comparator "zyx")))

(check-equal #t (comparator-test-type symbol-comparator 'abc))
(check-equal #f (comparator-test-type symbol-comparator "abc"))
(check-equal #t (comparator-equal? symbol-comparator 'xyz 'xyz))
(check-equal #f (comparator-equal? symbol-comparator 'XYZ 'xyz))
(check-equal -1 (comparator-compare symbol-comparator 'abc 'def))
(check-equal 0 (comparator-compare symbol-comparator 'ghi 'ghi))
(check-equal 1 (comparator-compare symbol-comparator 'mno 'jkl))
(check-equal #t (= (comparator-hash symbol-comparator 'pqr)
    (comparator-hash symbol-comparator 'pqr)))
(check-equal #f (= (comparator-hash symbol-comparator 'stu)
    (comparator-hash symbol-comparator 'vwx)))

(check-equal #t (comparator-test-type exact-integer-comparator 123))
(check-equal #t (comparator-equal? exact-integer-comparator 123 123))
(check-equal -1 (comparator-compare exact-integer-comparator 123 456))
(check-equal 0 (comparator-compare exact-integer-comparator 456 456))
(check-equal 1 (comparator-compare exact-integer-comparator 456 123))
(check-equal #t (= (comparator-hash exact-integer-comparator 123)
    (comparator-hash exact-integer-comparator 123)))
(check-equal #f (= (comparator-hash exact-integer-comparator 123)
    (comparator-hash exact-integer-comparator 456)))

(check-equal #t (comparator-test-type integer-comparator 123))
(check-equal #t (comparator-equal? integer-comparator 123 123.))
(check-equal -1 (comparator-compare integer-comparator 123 456.))
(check-equal 0 (comparator-compare integer-comparator 456 456.))
(check-equal 1 (comparator-compare integer-comparator 456 123.))
(check-equal #t (= (comparator-hash integer-comparator 123)
    (comparator-hash integer-comparator 123)))
(check-equal #f (= (comparator-hash integer-comparator 123)
    (comparator-hash integer-comparator 456.)))

(check-equal #t (comparator-test-type rational-comparator 123/17))
(check-equal #t (comparator-equal? rational-comparator 123/17 123/17))
(check-equal -1 (comparator-compare rational-comparator 123/17 456/17))
(check-equal 0 (comparator-compare rational-comparator 456/17 456/17))
(check-equal 1 (comparator-compare rational-comparator 456/17 123/17))
(check-equal #t (= (comparator-hash rational-comparator 123/17)
    (comparator-hash rational-comparator 123/17)))
(check-equal #f (= (comparator-hash rational-comparator 123/17)
    (comparator-hash rational-comparator 456/17)))

(check-equal #t (comparator-test-type real-comparator 123.456))
(check-equal #t (comparator-equal? real-comparator 123.456 123.456))
(check-equal -1 (comparator-compare real-comparator 123.456 456.789))
(check-equal 0 (comparator-compare real-comparator 456.789 456.789))
(check-equal 1 (comparator-compare real-comparator 456.789 123.456))
(check-equal #t (= (comparator-hash real-comparator 123.456)
    (comparator-hash real-comparator 123.456)))
(check-equal #f (= (comparator-hash real-comparator 123.456)
    (comparator-hash real-comparator 456.789)))

(check-equal #t (comparator-test-type complex-comparator 123+456i))
(check-equal #t (comparator-equal? complex-comparator 123+456i 123+456i))
(check-equal -1 (comparator-compare complex-comparator 123+456i 456+789i))
(check-equal 0 (comparator-compare complex-comparator 456+789i 456+789i))
(check-equal 1 (comparator-compare complex-comparator 456+789i 456))
(check-equal 1 (comparator-compare complex-comparator 456+789i 123+456i))
(check-equal #t (= (comparator-hash complex-comparator 123+456i)
    (comparator-hash complex-comparator 123+456i)))
(check-equal #f (= (comparator-hash complex-comparator 123+456i)
    (comparator-hash complex-comparator 456+789i)))

(check-equal #t (comparator-test-type number-comparator 123))
(check-equal #t (comparator-equal? number-comparator 123 123))
(check-equal -1 (comparator-compare number-comparator 123 456))
(check-equal 0 (comparator-compare number-comparator 456 456))
(check-equal 1 (comparator-compare number-comparator 456 123))
(check-equal #t (= (comparator-hash number-comparator 123)
    (comparator-hash number-comparator 123)))
(check-equal #f (= (comparator-hash number-comparator 123)
    (comparator-hash number-comparator 456)))

(check-equal #t (comparator-test-type pair-comparator '(123 . 456)))
(check-equal #t (comparator-equal? pair-comparator '(123 . 456) '(123 . 456)))
(check-equal -1 (comparator-compare pair-comparator '(123 . 456) '(456 . 789)))
(check-equal -1 (comparator-compare pair-comparator '(123 . 456) '(123 . 789)))
(check-equal 0 (comparator-compare pair-comparator '(456 . 789) '(456 . 789)))
(check-equal 1 (comparator-compare pair-comparator '(456 . 789) '(123 . 456)))
(check-equal 1 (comparator-compare pair-comparator '(456 . 789) '(456 . 456)))
(check-equal #t (= (comparator-hash pair-comparator '(123 . 456))
    (comparator-hash pair-comparator '(123 . 456))))
(check-equal #f (= (comparator-hash pair-comparator '(123 . 456))
    (comparator-hash pair-comparator '(456 . 789))))

(check-equal #t (comparator-test-type list-comparator '(123 456)))
(check-equal #t (comparator-equal? list-comparator '(123 456) '(123 456)))
(check-equal -1 (comparator-compare list-comparator '(123 456) '(456 789)))
(check-equal -1 (comparator-compare list-comparator '(123 456) '(123 789)))
(check-equal 0 (comparator-compare list-comparator '(456 789) '(456 789)))
(check-equal 1 (comparator-compare list-comparator '(456 789) '(123 456)))
(check-equal 1 (comparator-compare list-comparator '(456 789) '(456 456)))
(check-equal #t (= (comparator-hash list-comparator '(123 456))
    (comparator-hash list-comparator '(123 456))))
(check-equal #f (= (comparator-hash list-comparator '(123 456))
    (comparator-hash list-comparator '(456 789))))

(check-equal #t (comparator-test-type vector-comparator #(123 456)))
(check-equal #t (comparator-equal? vector-comparator #(123 456) #(123 456)))
(check-equal -1 (comparator-compare vector-comparator #(789 123 456) #(456 789 123 456)))
(check-equal -1 (comparator-compare vector-comparator #(123 456) #(456 789)))
(check-equal -1 (comparator-compare vector-comparator #(123 456) #(123 789)))
(check-equal 0 (comparator-compare vector-comparator #(456 789) #(456 789)))
(check-equal 0 (comparator-compare vector-comparator #() #()))
(check-equal 1 (comparator-compare vector-comparator #(456 789) #(123 456)))
(check-equal 1 (comparator-compare vector-comparator #(456 789) #(456 456)))
(check-equal #t (= (comparator-hash vector-comparator #(123 456))
    (comparator-hash vector-comparator #(123 456))))
(check-equal #t (= (comparator-hash vector-comparator #())
    (comparator-hash vector-comparator #())))
(check-equal #f (= (comparator-hash vector-comparator #(123 456))
    (comparator-hash vector-comparator #(456 789))))

(check-equal #t (comparator-test-type bytevector-comparator #u8(12 34)))
(check-equal #t (comparator-equal? bytevector-comparator #u8(12 34) #u8(12 34)))
(check-equal -1 (comparator-compare bytevector-comparator #u8(12 34) #u8(34 56)))
(check-equal -1 (comparator-compare bytevector-comparator #u8(56 12 34) #u8(34 56 78 90)))
(check-equal -1 (comparator-compare bytevector-comparator #u8(12 34) #u8(12 56)))
(check-equal 0 (comparator-compare bytevector-comparator #u8(34 56) #u8(34 56)))
(check-equal 0 (comparator-compare bytevector-comparator #u8() #u8()))
(check-equal 1 (comparator-compare bytevector-comparator #u8(34 56) #u8(12 34)))
(check-equal 1 (comparator-compare bytevector-comparator #u8(34 56) #u8(34 34)))
(check-equal #t (= (comparator-hash bytevector-comparator #u8())
    (comparator-hash bytevector-comparator #u8())))
(check-equal #t (= (comparator-hash bytevector-comparator #u8(12 34))
    (comparator-hash bytevector-comparator #u8(12 34))))
(check-equal #f (= (comparator-hash bytevector-comparator #u8(12 34))
    (comparator-hash bytevector-comparator #u8(34 56))))

(define irc (make-inexact-real-comparator 1 'round 'min))

(check-equal -1 (comparator-compare irc +nan.0 0))
(check-equal 0 (comparator-compare irc +nan.0 +nan.0))
(check-equal 0 (comparator-compare irc 12.0 12.1))
(check-equal -1 (comparator-compare irc 12.0 12.6))

(define irc (make-inexact-real-comparator 1 'round 'error))

(check-error (assertion-violation make-inexact-real-comparator) (comparator-compare irc +nan.0 0))

(define ilc (make-improper-list-comparator default-comparator))

(check-equal #t (comparator-test-type ilc '(123 456 . 789)))
(check-equal #t (comparator-equal? ilc '(123 456 . 789) '(123 456 . 789)))
(check-equal -1 (comparator-compare ilc '(123 456 . 789) '(456 789 . 123)))
(check-equal -1 (comparator-compare ilc '(123 456 . 789) '(123 789 . 456)))
(check-equal 0 (comparator-compare ilc '(456 789 . 123) '(456 789 . 123)))
(check-equal 1 (comparator-compare ilc '(456 789 . 123) '(123 456 . 789)))
(check-equal 1 (comparator-compare ilc '(456 789 . 123) '(456 456 . 789)))
(check-equal #t (= (comparator-hash ilc '(123 456 . 789))
    (comparator-hash ilc '(123 456 . 789))))
(check-equal #f (= (comparator-hash ilc '(123 456 . 789))
    (comparator-hash ilc '(456 789 . 123))))

(define sc (make-selecting-comparator boolean-comparator char-comparator integer-comparator))

(check-equal #t (comparator-test-type sc #t))
(check-equal #t (comparator-test-type sc #\A))
(check-equal #t (comparator-test-type sc 123))
(check-equal #f (comparator-test-type sc "abc"))
(check-equal #t (comparator-equal? sc #t #t))
(check-equal #f (comparator-equal? sc #\A #\b))
(check-equal #f (comparator-equal? sc 123 456))
(check-error (assertion-violation make-selecting-comparator) (comparator-equal? sc "abc" #t))
(check-error (assertion-violation make-selecting-comparator) (comparator-equal? sc 123 #t))
(check-equal 0 (comparator-compare sc 123 123))
(check-error (assertion-violation make-selecting-comparator) (comparator-compare sc 123 #t))
(check-error (assertion-violation make-selecting-comparator) (comparator-compare sc 123 "abc"))
(check-equal #t (= (comparator-hash sc #\A) (comparator-hash sc #\A)))
(check-equal #f (= (comparator-hash sc #\A) (comparator-hash sc #\a)))
(check-equal #f (= (comparator-hash sc #\A) (comparator-hash sc 123)))
(check-error (assertion-violation make-selecting-comparator) (comparator-hash sc "abc"))

(define rc (make-refining-comparator boolean-comparator char-comparator integer-comparator))

(check-equal #t (comparator-test-type rc #t))
(check-equal #t (comparator-test-type rc #\A))
(check-equal #t (comparator-test-type rc 123))
(check-equal #f (comparator-test-type rc "abc"))
(check-equal #t (comparator-equal? rc #t #t))
(check-equal #f (comparator-equal? rc #\A #\b))
(check-equal #f (comparator-equal? rc 123 456))
(check-error (assertion-violation make-refining-comparator) (comparator-equal? rc "abc" #t))
(check-error (assertion-violation make-refining-comparator) (comparator-equal? rc 123 #t))
(check-equal 0 (comparator-compare rc 123 123))
(check-error (assertion-violation make-refining-comparator) (comparator-compare rc 123 #t))
(check-error (assertion-violation make-refining-comparator) (comparator-compare rc 123 "abc"))
(check-equal #t (= (comparator-hash rc #\A) (comparator-hash rc #\A)))
(check-equal #f (= (comparator-hash rc #\A) (comparator-hash rc #\a)))
(check-equal #f (= (comparator-hash rc #\A) (comparator-hash rc 123)))
(check-error (assertion-violation make-refining-comparator) (comparator-hash rc "abc"))

(define revc (make-reverse-comparator char-comparator))

(check-equal #t (comparator-test-type revc #\A))
(check-equal #f (comparator-test-type revc 'a))
(check-equal #t (comparator-equal? revc #\z #\z))
(check-equal #f (comparator-equal? revc #\Z #\z))
(check-equal 1 (comparator-compare revc #\a #\f))
(check-equal 0 (comparator-compare revc #\Q #\Q))
(check-equal -1 (comparator-compare revc #\F #\A))
(check-equal #t (= (comparator-hash revc #\w) (comparator-hash revc #\w)))
(check-equal #f (= (comparator-hash revc #\w) (comparator-hash revc #\x)))

(check-equal -1 ((make-comparison< char<?) #\a #\b))
(check-equal 0 ((make-comparison< char<?) #\b #\b))
(check-equal 1 ((make-comparison< char<?) #\b #\a))

(check-equal -1 ((make-comparison> char>?) #\a #\b))
(check-equal 0 ((make-comparison> char>?) #\b #\b))
(check-equal 1 ((make-comparison> char>?) #\b #\a))

(check-equal -1 ((make-comparison<= char<=?) #\a #\b))
(check-equal 0 ((make-comparison<= char<=?) #\b #\b))
(check-equal 1 ((make-comparison<= char<=?) #\b #\a))

(check-equal -1 ((make-comparison>= char>=?) #\a #\b))
(check-equal 0 ((make-comparison>= char>=?) #\b #\b))
(check-equal 1 ((make-comparison>= char>=?) #\b #\a))

(check-equal -1 ((make-comparison=/< char=? char<?) #\a #\b))
(check-equal 0 ((make-comparison=/< char=? char<?) #\b #\b))
(check-equal 1 ((make-comparison=/< char=? char<?) #\b #\a))

(check-equal -1 ((make-comparison=/> char=? char>?) #\a #\b))
(check-equal 0 ((make-comparison=/> char=? char>?) #\b #\b))
(check-equal 1 ((make-comparison=/> char=? char>?) #\b #\a))

(check-equal less (if3 ((make-comparison< char<?) #\a #\b) 'less 'equal 'greater))
(check-equal equal (if3 ((make-comparison< char-ci<?) #\a #\A) 'less 'equal 'greater))
(check-equal greater (if3 ((make-comparison< char<?) #\b #\a) 'less 'equal 'greater))
(check-error (assertion-violation if3) (if3 2 #\a #\b #\c))
(check-error (assertion-violation if3) (if3 -2 #\a #\b #\c))

(check-equal not-equal (if=? ((make-comparison> char>?) #\a #\b) 'equal 'not-equal))
(check-equal not-equal (if=? ((make-comparison> char>?) #\b #\a) 'equal 'not-equal))
(check-equal equal (if=? ((make-comparison> char>?) #\a #\a) 'equal 'not-equal))

(check-equal less (if<? ((make-comparison<= char<=?) #\a #\b) 'less 'not))
(check-equal not (if<? ((make-comparison<= char<=?) #\b #\b) 'less 'not))
(check-equal not (if<? ((make-comparison<= char<=?) #\b #\a) 'less 'not))

(check-equal not (if>? ((make-comparison>= char>=?) #\a #\b) 'greater 'not))
(check-equal not (if>? ((make-comparison>= char>=?) #\b #\b) 'greater 'not))
(check-equal greater (if>? ((make-comparison>= char>=?) #\b #\a) 'greater 'not))

(check-equal less-equal (if<=? ((make-comparison=/< char=? char<?) #\a #\b) 'less-equal 'not))
(check-equal less-equal (if<=? ((make-comparison=/< char=? char<?) #\b #\b) 'less-equal 'not))
(check-equal not (if<=? ((make-comparison=/< char=? char<?) #\b #\a) 'less-equal 'not))

(check-equal not (if>=? ((make-comparison=/> char>=? char>?) #\a #\b) 'greater-equal 'not))
(check-equal greater-equal
    (if>=? ((make-comparison=/> char>=? char>?) #\b #\b) 'greater-equal 'not))
(check-equal greater-equal
    (if>=? ((make-comparison=/> char>=? char>?) #\b #\a) 'greater-equal 'not))

(check-equal not-equal (if-not=? ((make-comparison> char>?) #\a #\b) 'not-equal 'equal))
(check-equal not-equal (if-not=? ((make-comparison> char>?) #\b #\a) 'not-equal 'equal))
(check-equal equal (if-not=? ((make-comparison> char>?) #\a #\a) 'not-equal 'equal))

(check-equal #t (=? char-comparator #\a #\a))
(check-equal #t (=? char-comparator #\a #\a #\a))
(check-equal #f (=? char-comparator #\a #\b))
(check-equal #f (=? char-comparator #\a #\a #\b))

(check-equal #f (<? char-comparator #\a #\a))
(check-equal #t (<? char-comparator #\a #\b))
(check-equal #t (<? char-comparator #\a #\b #\c))
(check-equal #f (<? char-comparator #\b #\a))
(check-equal #f (<? char-comparator #\a #\b #\a))

(check-equal #f (>? char-comparator #\a #\a))
(check-equal #t (>? char-comparator #\b #\a))
(check-equal #t (>? char-comparator #\c #\b #\a))
(check-equal #f (>? char-comparator #\a #\b))
(check-equal #f (>? char-comparator #\b #\a #\b))

(check-equal #t (<=? char-comparator #\a #\a))
(check-equal #t (<=? char-comparator #\a #\b))
(check-equal #t (<=? char-comparator #\a #\b #\c))
(check-equal #f (<=? char-comparator #\b #\a))
(check-equal #f (<=? char-comparator #\a #\b #\a))

(check-equal #t (>=? char-comparator #\a #\a))
(check-equal #t (>=? char-comparator #\b #\a))
(check-equal #t (>=? char-comparator #\c #\b #\a))
(check-equal #f (>=? char-comparator #\a #\b))
(check-equal #f (>=? char-comparator #\b #\a #\b))

