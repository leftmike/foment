(define-library (srfi 166)
    (import (foment base))
    (export
        show
        displayed
        written

        escaped
        maybe-escaped
        numeric

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
        output-default
        output
        writer

        radix
        precision
        decimal-sep

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
                        ((port show-port))
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
            (cond
                ((number? obj) (%write-number obj))
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
                            (decimal-sep)))))
        (define (%numeric num radix precision sign-rule comma-rule comma-sep decimal-sep)
            (fn ()
                (numeric->string num radix precision sign-rule comma-rule comma-sep
                        (if (char? decimal-sep)
                            decimal-sep
                            (if (char=? comma-sep #\.) #\, #\.)))))

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
                ((with ((port out) (output output-default)) formatter))
                (fn () (mapper (get-output-string out)))))

        (define port (make-parameter (current-output-port))) ; check for output-port
        (define (output-default str)
            (write-string str (port)))
        (define output (make-parameter output-default)) ; check for procedure
        (define writer (make-parameter written-default)) ; check for procedure

        (define radix (make-parameter 10)) ; converter to check for 2 to 36
        (define precision (make-parameter #f)) ; check for #f or integer
        (define decimal-sep (make-parameter #f)) ; check for character

        (define sign-rule (make-parameter #f)) ; check for #f, #t, or pair of strings
        (define comma-rule (make-parameter #f)) ; check for #f, integer, or list of integers
        (define comma-sep (make-parameter #\,)) ; check for character
        )
    )
