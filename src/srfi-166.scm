(define-library (srfi 166)
    (import (foment base))
    (export
        show
        displayed
        written

        escaped
        maybe-escaped
        numeric
        numeric/comma

        nl
        fl
        space-to
        tab-to
        nothing
        each
        each-in-list

        joined/range

        upcased
        downcased
        fn
        with

        call-with-output

        port
        row
        col

        output
        output-default
        writer

        pad-char

        radix
        precision
        decimal-sep
        decimal-align
        sign-rule
        comma-rule
        comma-sep)
    (begin
        (define-syntax %formatter
            (syntax-rules ()
                ((%formatter () expr1 expr2 ...)
                    (%procedure->formatter (lambda () expr1 expr2 ...)))))
        (define-syntax fn
            (syntax-rules ()
                ((fn () expr ... fmt)
                    (%procedure->formatter (lambda () expr ... ((displayed fmt)))))
                ((fn ((id var) . rest) expr ... fmt)
                    (fn rest (let ((id (var))) expr ... fmt)))
                ((fn (id . rest) expr ... fmt)
                    (fn rest (let ((id (id))) expr ... fmt)))))
        (define-syntax with
            (syntax-rules ()
                ((with ((var val) ...) fmt ...)
                    (%with ((var val) ...) () fmt ...))))
        (define-syntax %with
            (syntax-rules ()
                ((%with () ((var val tmp) ...) fmt ...)
                    (fn ()
                        (let ((tmp val) ...)
                            (parameterize ((var tmp) ...) ((displayed fmt))) ...) nothing))
                ((%with ((var val) . rest) (vars ...) fmt ...)
                    (%with rest ((var val tmp) vars ...) fmt ...))))

        (define (show dest . fmts)
            (let ((show-port
                    (if (port? dest)
                        dest
                        (if dest
                            (current-output-port)
                            (open-output-string)))))
                (parameterize
                        ((port show-port) (col 0) (row 0))
                    ((each-in-list fmts))
                    (if (not dest)
                        (get-output-string show-port)))))
        (define (displayed obj)
            (cond
                ((%formatter? obj) obj)
                ((string? obj) (%formatter () ((output) obj)))
                ((char? obj) (%formatter () ((output) (string obj))))
                (else (written obj))))
        (define (written obj)
            (fn () ((writer) obj)))
        (define (written-default obj)
            (%write obj))
        (define (%write obj)
            (let ((write-radix (case (radix) ((2 8 10 16) (radix)) (else 10))))
                (with ((radix write-radix)
                        (precision (if (= write-radix 10) (precision) #f))
                        (sign-rule #f)
                        (comma-rule #f)
                        (decimal-sep #\.))
                    (%write-object obj))))
        (define (%write-object obj)
            (define (%write-number num)
                (each
                    (if (exact? num)
                        (case (radix)
                            ((2) "#b")
                            ((8) "#o")
                            ((16) "#x")
                            (else ""))
                        "")
                    (numeric num)))
            (define (%write-list lst)
                (cond
                    ((null? lst) ")")
                    ((pair? lst)
                        (each " " (%write-object (car lst)) (fn () (%write-list (cdr lst)))))
                    (else (each " . " (%write-object  lst) ")"))))
            (define (%write-vector vec idx)
                (cond
                    ((= idx (vector-length vec)) ")")
                    (else
                        (each
                            (if (> idx 0) " " nothing)
                            (%write-object (vector-ref obj idx))
                            (fn () (%write-vector vec (+ idx 1)))))))
            (cond
                ((number? obj) (%write-number obj))
                ((pair? obj)
                    (each "(" (%write-object (car obj)) (fn () (%write-list (cdr obj)))))
                ((vector? obj) (each "#(" (%write-vector obj 0)))
                (else
                    (let ((port (open-output-string)))
                        (write obj port)
                        (get-output-string port)))))

        (define escaped
            (case-lambda
                ((fmt) (%escaped fmt #\" #\\ #f))
                ((fmt quote-ch) (%escaped fmt quote-ch #\\ #f))
                ((fmt quote-ch esc-ch) (%escaped fmt quote-ch esc-ch #f))
                ((fmt quote-ch esc-ch renamer) (%escaped fmt quote-ch esc-ch renamer))))
        (define (%escaped fmt quote-ch esc-ch renamer)
            (if (not esc-ch)
                (with-output
                    (lambda (output str)
                        (string-for-each
                            (lambda (ch)
                                (if (char=? ch quote-ch)
                                    (output (string quote-ch)))
                                (output (string ch))) str))
                    fmt)
                (with-output
                    (lambda (output str)
                        (string-for-each
                            (lambda (ch)
                                (if (or (char=? ch quote-ch) (char=? ch esc-ch))
                                    (output (string esc-ch ch))
                                    (let ((rch (and renamer (renamer ch))))
                                        (if (char? rch)
                                            (output (string esc-ch rch))
                                            (output (string ch))))))
                            str))
                    fmt)))
        (define maybe-escaped
          (case-lambda
                ((fmt pred) (%maybe-escaped fmt pred #\" #\\ (lambda (ch) #f)))
                ((fmt pred quote-ch) (%maybe-escaped fmt pred quote-ch #\\ (lambda (ch) #f)))
                ((fmt pred quote-ch esc-ch)
                    (%maybe-escaped fmt pred quote-ch esc-ch (lambda (ch) #f)))
                ((fmt pred quote-ch esc-ch renamer)
                    (%maybe-escaped fmt pred quote-ch esc-ch renamer))))
        (define (%maybe-escaped fmt pred quote-ch esc-ch renamer)
            (define (string-pred pred idx str)
                (if (= idx (string-length str))
                    #f
                    (if (pred (string-ref str idx))
                        #t
                        (string-pred pred (+ idx 1) str))))
            (define (need-escape ch)
                (or (char=? ch quote-ch)
                    (eq? ch esc-ch)
                    (renamer ch)
                    (pred ch)))
            (call-with-output fmt
                    (lambda (str)
                      (if (string-pred need-escape 0 str)
                          (each quote-ch (%escaped str quote-ch esc-ch renamer) quote-ch)
                          (displayed str)))))
        (define numeric
            (case-lambda
                ((num)
                    (%numeric num (radix) (precision) (sign-rule) (comma-rule) (comma-sep)
                            (decimal-sep) (decimal-align)))
                ((num radix)
                    (%numeric num radix (precision) (sign-rule) (comma-rule) (comma-sep)
                            (decimal-sep) (decimal-align)))
                ((num radix precision)
                    (%numeric num radix precision (sign-rule) (comma-rule) (comma-sep)
                            (decimal-sep) (decimal-align)))
                ((num radix precision sign-rule)
                    (%numeric num radix precision sign-rule (comma-rule) (comma-sep)
                            (decimal-sep) (decimal-align)))
                ((num radix precision sign-rule comma-rule)
                    (%numeric num radix precision sign-rule comma-rule (comma-sep)
                            (decimal-sep) (decimal-align)))
                ((num radix precision sign-rule comma-rule comma-sep)
                    (%numeric num radix precision sign-rule comma-rule comma-sep
                            (decimal-sep) (decimal-align)))
                ((num radix precision sign-rule comma-rule comma-sep decimal-sep)
                    (%numeric num radix precision sign-rule comma-rule comma-sep
                            decimal-sep (decimal-align)))))
        (define numeric/comma
            (case-lambda
                ((num)
                    (%numeric num (radix) (precision) (sign-rule)
                            (if (comma-rule) (comma-rule) 3) (comma-sep) (decimal-sep)
                            (decimal-align)))
                ((num comma-rule)
                    (%numeric num (radix) (precision) (sign-rule) comma-rule (comma-sep)
                            (decimal-sep) (decimal-align)))
                ((num comma-rule radix)
                    (%numeric num radix (precision) (sign-rule) comma-rule (comma-sep)
                            (decimal-sep) (decimal-align)))
                ((num comma-rule radix precision)
                    (%numeric num radix precision (sign-rule) comma-rule (comma-sep)
                            (decimal-sep) (decimal-align)))
                ((num comma-rule radix precision sign-rule)
                    (%numeric num radix precision sign-rule comma-rule (comma-sep)
                            (decimal-sep) (decimal-align)))))
        (define (%numeric num radix precision sign-rule comma-rule comma-sep decimal-sep
                decimal-align)
            (define (check-comma-rule comma-rule)
                (if (or (not comma-rule) (and (exact-integer? comma-rule) (> comma-rule 0)))
                    #t
                    (if (not (pair? comma-rule))
                        #f
                        (let ((ret #t))
                            (for-each
                                (lambda (n)
                                    (if (not (and (exact-integer? n) (> n 0)))
                                        (set! ret #f)))
                                comma-rule)
                            ret))))
            ; sign-rule: #f, #t, or pair of two strings
            (if (and (not (boolean? sign-rule))
                    (or (not (pair? sign-rule)) (not (string? (car sign-rule)))
                            (not (string? (cdr sign-rule)))))
                (full-error 'assertion-violation 'numeric #f
                        "numeric: expected sign-rule of #f, #t, or a pair of strings"
                        sign-rule))
            (fn ()
                (define %comma-sep
                    (if (char? comma-sep)
                        comma-sep
                        (if (eq? decimal-sep #\,) #\. #\,)))
                (define %decimal-sep
                    (if (char? decimal-sep)
                        decimal-sep
                        (if (eq? comma-sep #\.) #\, #\.)))
                (define (align str decimal-align decimal-sep)
                    (define (last-ch str idx ch)
                        (if (< idx 0)
                            idx
                            (if (char=? (string-ref str idx) ch)
                                idx
                                (last-ch str (- idx 1) ch))))
                    (if (and (integer? decimal-align) (> decimal-align 1))
                        (let ((idx (last-ch str (- (string-length str) 1) decimal-sep))
                                (decimal-align (- decimal-align 1)))
                            (if (or (< idx 0) (>= idx decimal-align))
                                str
                                (string-append (make-string (- decimal-align idx) #\space) str)))
                        str))
                (cond
                    ((or (nan? num) (infinite? num)) (number->string num))
                    ((not (real? num))
                        (each
                            (%numeric (real-part num) radix precision
                                    (if (boolean? sign-rule) sign-rule #f) comma-rule %comma-sep
                                    %decimal-sep #f)
                            (%numeric (imag-part num) radix precision #t comma-rule %comma-sep
                                    %decimal-sep #f)
                            "i"))
                    (else
                        (let* ((n (if (< num 0) (- num) num))
                                (str (numeric->string n radix precision comma-rule %comma-sep
                                             %decimal-sep)))
                            (align
                                (cond
                                    ((eqv? num -0.0)
                                        (if (pair? sign-rule)
                                            (string-append (car sign-rule) "0.0" (cdr sign-rule))
                                            "-0.0"))
                                    ((eq? sign-rule #f)
                                        (string-append (if (< num 0) "-" "") str))
                                    ((eq? sign-rule #t)
                                        (string-append (if (< num 0) "-" "+") str))
                                    (else
                                        (string-append (car sign-rule) str (cdr sign-rule))))
                                decimal-align %decimal-sep))))))

        (define nl (displayed "\n"))
        (define fl (fn (col) (if (= col 0) nothing "\n")))
        (define (space-to column)
            (fn (col)
              (if (> column col)
                  (make-string (- column col) #\space)
                  nothing)))
        (define tab-to
            (case-lambda
                (() (tab-to 8))
                ((tab)
                    (fn (col pad-char)
                        (let ((n (truncate-remainder col tab)))
                            (if (> n 0)
                                (make-string (- tab n) pad-char)
                                nothing))))))
        (define nothing (%formatter () ""))
        (define (each . fmts)
            (each-in-list fmts))
        (define (each-in-list fmts)
;            (let ((fmts (map (lambda (fmt) (displayed fmt)) fmts)))
;                (%formatter () (for-each (lambda (fmt) (fmt)) fmts))))
            (if (pair? fmts)
                (let ((fmt (car fmts)) (fmts (cdr fmts)))
                    (if (null? fmts)
                        (displayed fmt)
                        (fn () ((displayed fmt)) (each-in-list fmts))))
                nothing))

; From chibi scheme
; XXX
(define (joined/range elt-f start . o)
  (let ((end (and (pair? o) (car o)))
        (sep (if (and (pair? o) (pair? (cdr o))) (cadr o) "")))
    (let lp ((i start))
      (if (and end (>= i end))
          nothing
          (each (if (> i start) sep nothing)
                (elt-f i)
                (fn () (lp (+ i 1))))))))

        (define (with-output proc fmt)
            (fn ((original output))
                (with ((output (lambda (str) (proc original str))))
                    fmt)))
        (define (upcased . fmts)
            (with-output
                    (lambda (output str) (output (string-upcase str)))
                    (each-in-list fmts)))
        (define (downcased . fmts)
            (with-output
                    (lambda (output str) (output (string-downcase str)))
                    (each-in-list fmts)))

        (define (call-with-output formatter mapper)
            (let ((out (open-output-string)))
                ((with ((port out) (output output-default) (col 0) (row 0)) formatter))
                (fn () (mapper (get-output-string out)))))

        (define (output-default str)
            (write-string str (port))
            (let ((c (col)) (r (row)))
              (if (boolean? c)
                  (error "boolean col"))
              (if (boolean? r)
                  (error "boolean row"))
                (string-for-each
                    (lambda (ch)
                        (if (char=? ch #\newline)
                            (begin
                                (set! r (+ r 1))
                                (set! c 0))
                            (set! c (+ c 1))))
                    str)
                (col c)
                (row r)))

        (define port (make-parameter (current-output-port))) ; check for output-port
        (define row (make-parameter #f))
        (define col (make-parameter #f))

        (define output (make-parameter output-default)) ; check for procedure
        (define writer (make-parameter written-default)) ; check for procedure

        (define pad-char (make-parameter #\space)) ; check for character

        (define radix (make-parameter 10)) ; check for 2 to 36
        (define precision (make-parameter #f)) ; check for #f or integer
        (define decimal-sep (make-parameter #f)) ; check for character
        (define decimal-align (make-parameter #f)) ; check for integer
        (define sign-rule (make-parameter #f)) ; check for #f, #t, or pair of strings
        (define comma-rule (make-parameter #f)) ; check for #f, integer, or list of integers
        (define comma-sep (make-parameter #f)) ; check for character
        )
    )
