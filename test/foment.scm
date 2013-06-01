;;;
;;; Foment
;;;

;;
;; ---- syntax ----
;;

;; letrec-values

;; letrec*-values


;; srfi-39
;; parameterize

(define radix (make-parameter 10))

(define write-shared (make-parameter #f
    (lambda (x)
        (if (boolean? x)
            x
            (error "only booleans are accepted by write-shared")))))

(must-equal 10 (radix))
(radix 2)
(must-equal 2 (radix))
(must-raise (assertion-violation error) (write-shared 0))

;(define prompt
;    (make-parameter 123
;        (lambda (x)
;            (if (string? x)
;                x
;                (with-output-to-string (lambda () (write x)))))))

;(prompt)       ==>  "123"
;(prompt ">")
;(prompt)       ==>  ">"

;(radix)                                              ==>  2
;(parameterize ((radix 16)) (radix))                  ==>  16
;(radix)                                              ==>  2

;(define (f n) (number->string n (radix)))

;(f 10)                                               ==>  "1010"
;(parameterize ((radix 8)) (f 10))                    ==>  "12"
;(parameterize ((radix 8) (prompt (f 10))) (prompt))  ==>  "1010"

