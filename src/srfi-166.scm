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
        numeric/si
        numeric/fitted
        nl
        fl
        space-to
        tab-to
        nothing
        each
        each-in-list
        joined
        joined/prefix
        joined/suffix
        joined/last
        joined/dot
        joined/range
        padded
        padded/right
        padded/both
        trimmed
        trimmed/right
        trimmed/both
        trimmed/lazy
        fitted
        fitted/right
        fitted/both

        columnar
        tabular

        wrapped/char

        line-numbers

        upcased
        downcased
        fn
        with
        with!
        forked
        call-with-output
        make-state-variable
        port
        row
        col
        width
        output
        output-default
        writer
        string-width
        substring/width
        substring/preserve
        pad-char
        ellipsis
        radix
        precision
        decimal-sep
        decimal-align
        sign-rule
        comma-rule
        comma-sep

        make-runner
        string-split
        make-infinite-runner)
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
        (define numeric/si
            (case-lambda
                ((num) (%numeric/si num 1000 ""))
                ((num base) (%numeric/si num base ""))
                ((num base sep) (%numeric/si num base sep))))
        (define (%numeric/si num base sep)
            (define (si-precision d)
                (if (>= d 100)
                    0
                    (if (= 0 (modulo (round (* d 10)) 10))
                        0
                        1)))
            (define (si10 n k prefixes)
                (if (null? (cdr prefixes))
                    (each (numeric (/ n k) 10 0) sep (car prefixes))
                    (let ((d (/ n k)))
                        (if (< d base)
                            (each (numeric d 10 (si-precision d)) sep (car prefixes))
                            (si10 n (* k base) (cdr prefixes))))))
            (define (si-10 n k prefixes)
                (if (null? (cdr prefixes))
                    (each (numeric (* n k) 10 0) sep (car prefixes))
                    (let ((d (* n k)))
                        (if (>= d 1)
                            (each (numeric d 10 (si-precision d)) sep (car prefixes))
                            (si-10 n (* k base) (cdr prefixes))))))
            (if (not (or (= base 1000) (= base 1024)))
                (full-error 'assertion-violation 'numeric/si #f
                        "numeric/si: expected base of 1000 or 1024" base))
            (if (not (string? sep))
                (full-error 'assertion-violation 'numeric/si #f
                            "numeric/si: expected string for separator" sep))
            (if (= num 0)
                "0"
                (let* ((n (if (< num 0) (- num) num))
                        (str
                            (if (< n 1)
                                (si-10 n 1
                                        (if (= base 1000)
                                            '("" "m" "µ" "n" "p" "f" "a" "z" "y")
                                            '("" "mi" "µi" "ni" "pi" "fi" "ai" "zi" "yi")))
                                (si10 n 1
                                        (if (= base 1000)
                                            '("" "k" "M" "G" "T" "E" "P" "Z" "Y")
                                            '("" "Ki" "Mi" "Gi" "Ti" "Ei" "Pi" "Zi" "Yi"))))))
                     (each (if (< num 0) "-" "") str))))
        (define numeric/fitted
            (case-lambda
                ((width num)
                    (%numeric/fitted width num (radix) (precision) (sign-rule) (comma-rule)
                            (comma-sep) (decimal-sep)))
                ((width num radix)
                    (%numeric/fitted width num radix (precision) (sign-rule) (comma-rule)
                            (comma-sep) (decimal-sep)))
                ((width num radix precision)
                    (%numeric/fitted width num radix precision (sign-rule) (comma-rule) (comma-sep)
                            (decimal-sep)))
                ((width num radix precision sign-rule)
                    (%numeric/fitted width num radix precision sign-rule (comma-rule) (comma-sep)
                            (decimal-sep)))
                ((width num radix precision sign-rule comma-rule)
                    (%numeric/fitted width num radix precision sign-rule comma-rule (comma-sep)
                            (decimal-sep)))
                ((width num radix precision sign-rule comma-rule comma-sep)
                    (%numeric/fitted width num radix precision sign-rule comma-rule comma-sep
                            (decimal-sep)))
                ((width num radix precision sign-rule comma-rule comma-sep decimal-sep)
                    (%numeric/fitted width num radix precision sign-rule comma-rule comma-sep
                            decimal-sep))))
        (define (%numeric/fitted width num radix precision sign-rule comma-rule comma-sep
                decimal-sep)
            (call-with-output (%numeric num radix precision sign-rule comma-rule comma-sep
                            decimal-sep (decimal-align))
                    (lambda (str)
                        (if (> (string-length str) width)
                            (if (and precision (> precision 0) (< precision width))
                                (string-append
                                        (make-string (- width precision 1) #\#)
                                        (if (char? decimal-sep)
                                            (string decimal-sep)
                                            (if (eq? comma-sep #\.) "," "."))
                                        (make-string precision #\#))
                                    (make-string width #\#))
                            str))))
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
            (if (pair? fmts)
                (let ((fmt (car fmts)) (fmts (cdr fmts)))
                    (if (null? fmts)
                        (displayed fmt)
                        (fn () ((displayed fmt)) (each-in-list fmts))))
                nothing))
        (define joined
            (case-lambda
                ((mapper lst) (%joined mapper lst ""))
                ((mapper lst sep) (%joined mapper lst sep))))
        (define (%joined mapper lst sep)
            (if (null? lst)
                nothing
                (if (null? (cdr lst))
                    (mapper (car lst))
                    (each (mapper (car lst)) sep (fn () (%joined mapper (cdr lst) sep))))))
        (define joined/prefix
            (case-lambda
                ((mapper lst) (%joined/prefix mapper lst ""))
                ((mapper lst sep) (%joined/prefix mapper lst sep))))
        (define (%joined/prefix mapper lst sep)
            (if (null? lst)
                nothing
                (each sep (mapper (car lst)) (fn () (%joined/prefix mapper (cdr lst) sep)))))
        (define joined/suffix
            (case-lambda
                ((mapper lst) (%joined/suffix mapper lst ""))
                ((mapper lst sep) (%joined/suffix mapper lst sep))))
        (define (%joined/suffix mapper lst sep)
            (if (null? lst)
                nothing
                (each (mapper (car lst)) sep (fn () (%joined/suffix mapper (cdr lst) sep)))))
        (define joined/last
            (case-lambda
                ((mapper last lst) (%joined/last mapper last lst ""))
                ((mapper last lst sep) (%joined/last mapper last lst sep))))
        (define (%joined/last mapper last lst sep)
            (if (null? lst)
                nothing
                (if (null? (cdr lst))
                    (last (car lst))
                    (each (mapper (car lst)) sep
                            (fn () (%joined/last mapper last (cdr lst) sep))))))
        (define joined/dot
            (case-lambda
                ((mapper dot lst) (%joined/dot mapper dot lst ""))
                ((mapper dot lst sep) (%joined/dot mapper dot lst sep))))
        (define (%joined/dot mapper dot lst sep)
            (if (null? lst)
                nothing
                (if (pair? lst)
                    (if (null? (cdr lst))
                        (mapper (car lst))
                        (each (mapper (car lst)) sep
                                (fn () (%joined/dot mapper dot (cdr lst) sep))))
                    (dot lst))))
        (define joined/range
            (case-lambda
                ((mapper start) (%joined/range mapper start #f ""))
                ((mapper start end) (%joined/range mapper start end ""))
                ((mapper start end sep) (%joined/range mapper start end sep))))
        (define (%joined/range mapper start end sep)
            (if (or (not end) (< (+ start 1) end))
                (each (mapper start) sep (fn () (%joined/range mapper (+ start 1) end sep)))
                (if (< start end)
                    (mapper start)
                    nothing)))
        (define (padded width . fmts)
            (call-with-output (each-in-list fmts)
                    (lambda (str)
                        (fn (string-width pad-char)
                            (let ((pad (- width (string-width str))))
                                (if (> pad 0)
                                    (each (make-string pad pad-char) str)
                                    str))))))
        (define (padded/right width . fmts)
            (fn ((start col))
                (each
                    (each-in-list fmts)
                    (fn ((end col) pad-char)
                        (if (> width (- end start))
                            (make-string (- width (- end start)) pad-char)
                            nothing)))))
        (define (padded/both width . fmts)
            (call-with-output (each-in-list fmts)
                    (lambda (str)
                        (fn (string-width pad-char)
                            (let* ((pad (- width (string-width str)))
                                    (left (truncate-quotient pad 2)))
                                (if (> pad 0)
                                    (each
                                            (make-string left pad-char)
                                            str
                                            (make-string (- pad left) pad-char))
                                    str))))))
        (define (trimmed width . fmts)
            (call-with-output (each-in-list fmts)
                    (lambda (str)
                        (fn (string-width ellipsis substring/preserve substring/width)
                            (let* ((extra (- (string-width str) width))
                                    (ellipsis (if (< (string-width ellipsis) width) ellipsis ""))
                                    (trim (+ extra (string-width ellipsis))))
                                (if (> extra 0)
                                    (each
                                        (if substring/preserve
                                            (substring/preserve (substring/width str 0 trim))
                                            nothing)
                                        ellipsis
                                        (substring/width str trim (string-width str)))
                                    str))))))
        (define (trimmed/right width . fmts)
            (call-with-output (each-in-list fmts)
                    (lambda (str)
                        (fn (string-width ellipsis substring/preserve substring/width)
                            (let* ((extra (- (string-width str) width))
                                    (ellipsis (if (< (string-width ellipsis) width) ellipsis ""))
                                    (trim (+ extra (string-width ellipsis))))
                                (if (> extra 0)
                                    (each
                                        (substring/width str 0 (- (string-width str) trim))
                                        ellipsis
                                        (if substring/preserve
                                            (substring/preserve (substring/width str
                                                    (- (string-width str) trim)
                                                    (string-width str)))
                                            nothing))
                                    str))))))
        (define (trimmed/both width . fmts)
            (call-with-output (each-in-list fmts)
                    (lambda (str)
                        (fn (string-width ellipsis substring/preserve substring/width)
                            (let* ((extra (- (string-width str) width))
                                    (ellipsis
                                        (if (< (* 2 (string-width ellipsis)) width) ellipsis ""))
                                    (trim (+ extra (* 2 (string-width ellipsis))))
                                    (left (truncate-quotient trim 2))
                                    (right (- trim left)))
                                (if (> extra 0)
                                    (each
                                        (if substring/preserve
                                            (substring/preserve (substring/width str 0 left))
                                            nothing)
                                        ellipsis
                                        (substring/width str left (- (string-width str) right))
                                        ellipsis
                                        (if substring/preserve
                                            (substring/preserve (substring/width str right
                                                    (string-width str)))
                                            nothing))
                                    str))))))
        (define (trimmed/lazy width . fmts)
            (fn (string-width substring/width)
                (call/cc
                    (lambda (done)
                        (let ((count 0))
                            (define (output/lazy original str)
                                (if (< count width)
                                    (let ((len (string-width str)))
                                        (if (< (+ count len) width)
                                            (original str)
                                            (original (substring/width str 0 (- width count))))
                                        (set! count (+ count len)))
                                    (done nothing)))
                            (with-output output/lazy (each-in-list fmts)))))))
        (define (fitted width . fmts)
            (padded width (trimmed width (each-in-list fmts))))
        (define (fitted/right width . fmts)
            (padded/right width (trimmed/right width (each-in-list fmts))))
        (define (fitted/both width . fmts)
            (padded/both width (trimmed/both width (each-in-list fmts))))

        (define (string-split str ch)
            (define (split idx)
                (if (= idx (string-length str))
                    (values #f str "")
                    (if (char=? (string-ref str idx) ch)
                        (values #t (string-copy str 0 idx) (string-copy str (+ idx 1)))
                        (split (+ idx 1)))))
            (split 0))
        (define (make-infinite-runner wid fmt)
            (let ((state 'init) (buf "") (return #f) (continue #f) (newlines 0))
                (define (output-infinite str)
                    (set! continue #f)
                    (set! buf (string-append buf str))
                    (set! newlines (update-row-col str))
                    (if (> newlines 0)
                        (call/cc
                            (lambda (cont)
                                (set! continue cont)
                                (return #t)))))
                (lambda ()
                    (set! return #f)
                    (if (eq? state 'init)
                        (begin
                            (set! state 'generate)
                            (call/cc
                                (lambda (cont)
                                    (set! return cont)
                                    (call-with-parameterize
                                        state-variables
                                        (map
                                            (lambda (var)
                                                (cond
                                                    ((eq? var output) output-infinite)
                                                    ((eq? var col) 0)
                                                    ((eq? var row) 0)
                                                    ((eq? var width) wid)
                                                    (else (var))))
                                            state-variables)
                                        (lambda ()
                                            (fmt)
                                            (set! state 'done)
                                            (return #f)))))))
                    (if (and (eq? state 'generate) (= newlines 0))
                        (call/cc
                            (lambda (cont)
                                (set! return cont)
                                (continue #f))))
                    (if (> newlines 0)
                        (let-values (((found line rest) (string-split buf #\newline)))
                            (set! newlines (- newlines 1))
                            (set! buf rest)
                            line)
                        (let ((ret (if (equal? buf "") #f buf)))
                            (set! buf #f)
                            ret)))))
        (define (make-runner wid fmt)
            (let ((out (open-output-string)))
                (call-with-parameterize
                    state-variables
                    (map
                        (lambda (var)
                            (cond
                                ((eq? var port) out)
                                ((eq? var output) output-default)
                                ((eq? var col) 0)
                                ((eq? var row) 0)
                                ((eq? var width) wid)
                                (else (var))))
                        state-variables)
                    (lambda () (fmt)))
                (let* ((str (get-output-string out))
                        (len (string-length str))
                        (strt 0))
                    (define (line idx)
                        (cond
                            ((= idx len)
                                (let ((ret (string-copy str strt idx)))
                                    (set! strt idx)
                                    ret))
                            ((char=? (string-ref str idx) #\newline)
                                (let ((ret (string-copy str strt idx)))
                                    (set! strt (+ idx 1))
                                    ret))
                            (else (line (+ idx 1)))))
                    (lambda ()
                        (if (= strt len)
                            #f
                            (line strt))))))
        (define (make-list-runner wid col)
            (define (runner->list runner tail)
                (let ((val (runner)))
                    (if (string? val)
                        (let ((w ((string-width) val)))
                            (if (> w wid)
                                (set! wid w))
                            (set-cdr! tail (list val))
                            (runner->list runner (cdr tail))))))
          (let ((runner (make-runner wid col))
                  (tail (list #f)))
              (runner->list runner tail)
              (let ((lst (cdr tail)))
                  (values
                          (lambda ()
                              (if (null? lst)
                                  #f
                                  (let ((val (car lst)))
                                      (set! lst (cdr lst))
                                      val)))
                          wid))))
        (define (columnar-args args)
            (define (justify-left width str last)
                (let ((wid ((string-width) str)))
                    (cond
                        ((and (< wid width) (not last))
                            (string-append str (make-string (- width wid) (pad-char))))
                        (else str))))
            (define (justify-right width str last)
                (let ((wid ((string-width) str)))
                    (cond
                        ((< wid width)
                            (string-append (make-string (- width wid) (pad-char)) str))
                        (else str))))
            (define (justify-center width str last)
                (let ((wid ((string-width) str)))
                    (if (< wid width)
                        (let* ((pad (- width wid))
                                (left (truncate-quotient pad 2)))
                            (string-append (make-string left (pad-char)) str
                                (if (not last)
                                    (make-string (- pad left) (pad-char))
                                    "")))
                        str)))
            (define (handle-args args justify justification wid widths inf infinite cols)
                (if (null? args)
                    (values (reverse justification) (reverse widths) (reverse infinite)
                            (reverse cols))
                    (let ((arg (car args)))
                        (cond
                            ((string? arg)
                                (handle-args (cdr args)
                                        justify-left (cons justify-left justification)
                                        #f (cons ((string-width) arg) widths) #f (cons #f infinite)
                                        (cons arg cols)))
                            ((%formatter? arg)
                                (handle-args (cdr args) justify-left (cons justify justification)
                                        #f (cons wid widths) #f (cons inf infinite)
                                        (cons arg cols)))
                            ((eq? arg 'left)
                                (handle-args (cdr args) justify-left justification wid widths
                                        inf infinite cols))
                            ((eq? arg 'right)
                                (handle-args (cdr args) justify-right justification wid widths
                                        inf infinite cols))
                            ((eq? arg 'center)
                                (handle-args (cdr args) justify-center justification wid widths
                                        inf infinite cols))
                            ((eq? arg 'infinite)
                                (handle-args (cdr args) justify justification wid widths #t
                                        infinite cols))
                            ((and (number? arg)
                                (or (and (exact-integer? arg) (> arg 0))
                                    (and (> arg 0) (< arg 1))))
                                (handle-args (cdr args) justify justification arg widths inf
                                        infinite cols))
                            (else
                                (full-error 'assertion-violation 'columnar #f
                                        "columnar: unexpected column modifier" arg))))))
            (handle-args args justify-left '() #f '() #f '() '()))
        (define (columnar-widths width widths zero)
            (define (total widths)
                (if (null? widths)
                    0
                    (let ((arg (car widths)))
                        (if (and (number? arg) (exact-integer? arg))
                            (+ arg (total (cdr widths)))
                            (total (cdr widths))))))
            (define (fractional extra widths)
              (if (not (null? widths))
                  (let ((arg (car widths)))
                      (if (and (number? arg) (> arg 0) (< arg 1))
                          (set-car! widths (truncate (* arg extra))))
                      (fractional extra (cdr widths)))))
            (define (unspecified extra widths)
                (define (count cnt widths)
                    (if (null? widths)
                        cnt
                        (let ((arg (car widths)))
                            (count (if (and (number? arg) (exact-integer? arg)) cnt (+ cnt 1))
                                   (cdr widths)))))
                (define (specify div mod widths)
                  (if (not (null? widths))
                      (begin
                          (if (not (number? (car widths)))
                              (begin
                                  (set-car! widths (+ div (if (> mod 0) 1 0)))
                                  (set! mod (- mod 1))))
                          (specify div mod (cdr widths)))))
                (let ((cnt (count 0 widths)))
                    (if (> cnt 0)
                        (let* ((div (truncate-quotient extra cnt))
                                (mod (- extra (* cnt div))))
                            (specify div mod widths)))))
            (define (zero-unspecified widths)
              (if (not (null? widths))
                  (begin
                      (if (not (number? (car widths)))
                          (set-car! widths 0))
                      (zero-unspecified (cdr widths)))))
            (let ((extra (- width (total widths))))
                (fractional (if (> extra 0) extra 0) widths)
                (let ((extra (- width (total widths))))
                    (if zero
                        (zero-unspecified widths)
                        (unspecified (if (> extra 0) extra 0) widths))
                    widths)))
        (define (show-columns first justification widths infinite cols)
            (define (show-row justification widths strs)
                (if (not (null? strs))
                    (let ((str (car strs)))
                        ((output)
                                ((car justification) (car widths) (if str str "")
                                        (null? (cdr strs))))
                        (show-row (cdr justification) (cdr widths) (cdr strs)))))
            (let* ((show #f)
                    (strs (map
                            (lambda (inf col)
                                (if (string? col)
                                    col
                                    (let ((str (col)))
                                        (if (and (string? str) (not inf))
                                            (set! show #t))
                                        str)))
                            infinite cols)))
                (if show
                    (begin
                        (if (not first)
                            ((output) "\n"))
                        (show-row justification widths strs)
                        (show-columns #f justification widths infinite cols)))))
        (define (columnar . args)
            (fn (width)
                (let-values (((justification widths infinite cols) (columnar-args args)))
                    (let* ((widths (columnar-widths width widths #f))
                            (cols (map
                                    (lambda (wid inf col)
                                        (if (string? col)
                                            col
                                            (if inf
                                                (make-infinite-runner wid col)
                                                (make-runner wid col))))
                                    widths infinite cols)))
                        (show-columns #t justification widths infinite cols)
                        ((output) "\n")))
                nothing))
        (define (tabular . args)
            (define (run-columns widths infinite cols)
                (if (null? cols)
                    '()
                    (let ((wid (car widths)) (inf (car infinite)) (col (car cols)))
                        (cons
                                (if (string? col)
                                    col
                                    (if inf
                                        (make-infinite-runner wid col)
                                        (let-values (((runner wid) (make-list-runner wid col)))
                                            (set-car! widths wid)
                                            runner)))
                                (run-columns (cdr widths) (cdr infinite) (cdr cols))))))
            (fn (width)
                (let*-values (((justification widths infinite cols) (columnar-args args)))
                    (let ((cols (run-columns (columnar-widths width widths #t) infinite cols)))
                        (show-columns #t justification widths infinite cols)
                        ((output) "\n")))
                nothing))

        (define (wrapped/char . fmts)
            (let ((last-ch #\space))
                (each
                    (with-output
                            (lambda (output str)
                                (string-for-each
                                    (lambda (ch)
                                        (if (and (>= (col) (width)) (not (char=? ch #\newline)))
                                            (output "\n"))
                                        (output (string ch)))
                                    str)
                                (if (> (string-length str) 0)
                                    (set! last-ch (string-ref str (- (string-length str) 1)))))
                            (each-in-list fmts))
                    (fn () (if (not (char=? last-ch #\newline)) "\n" "")))))

        (define line-numbers
            (case-lambda
                (() (joined/range displayed 1 #f "\n"))
                ((start) (joined/range displayed start #f "\n"))))

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
        (define-syntax with!
            (syntax-rules ()
                ((with! (var val) ...)
                    (begin (var val) ...))))
        (define (forked fmt1 fmt2)
            (each
                (call-with-parameterize
                        state-variables
                        (map (lambda (var) (var)) state-variables)
                        (fn () fmt1))
                fmt2))
        (define (call-with-output formatter mapper)
            (let ((out (open-output-string)))
                (forked (with ((port out) (output output-default) (col 0) (row 0)) formatter)
                (fn () (mapper (get-output-string out))))))
        (define state-variables '())
        (define make-state-variable
            (case-lambda
                ((name default) (%make-state-variable default))
                ((name default immutable) (%make-state-variable default))))
        (define (%make-state-variable default)
            (let ((param (make-parameter default)))
                (set! state-variables (cons param state-variables))
                param))
        (define (update-row-col str)
            (let ((c (col)) (r (row)) (newlines 0))
                (if (or (boolean? c) (boolean? r))
                    (full-error 'assertion-violation 'output-default #f
                            "output-default: called out of show"))
                (string-for-each
                    (lambda (ch)
                        (if (char=? ch #\newline)
                            (begin
                                (set! r (+ r 1))
                                (set! c 0)
                                (set! newlines (+ newlines 1)))
                            (set! c (+ c 1))))
                    str)
                (col c)
                (row r)
                newlines))
        (define (output-default str)
            (update-row-col str)
            (write-string str (port)))
        (define string-width-default
            (case-lambda
                ((str) (string-length str))
                ((str start) (- (string-length str) start))
                ((str start end) (- end start))))
        (define port (%make-state-variable (current-output-port))) ; check for output-port
        (define row (%make-state-variable #f))
        (define col (%make-state-variable #f))
        ; XXX: calculate based on the terminal width if possible; otherwise default to 80
        (define width (%make-state-variable 100)) ; check for integer
        (define output (%make-state-variable output-default)) ; check for procedure
        (define writer (%make-state-variable written-default)) ; check for procedure
        (define string-width (%make-state-variable string-width-default)) ; check for procedure
        (define substring/width (%make-state-variable substring)) ; check for procedure
        (define substring/preserve (%make-state-variable #f)) ; check for #f or procedure
        (define pad-char (%make-state-variable #\space)) ; check for character
        (define ellipsis (%make-state-variable "")) ; check for string
        (define radix (%make-state-variable 10)) ; check for 2 to 36
        (define precision (%make-state-variable #f)) ; check for #f or integer
        (define decimal-sep (%make-state-variable #f)) ; check for character
        (define decimal-align (%make-state-variable #f)) ; check for integer
        (define sign-rule (%make-state-variable #f)) ; check for #f, #t, or pair of strings
        (define comma-rule (%make-state-variable #f)) ; check for #f, integer, or list of integers
        (define comma-sep (%make-state-variable #f)) ; check for character
        )
    )
