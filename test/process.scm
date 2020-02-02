;;;
;;; Processes
;;;

(import (foment base))

(check-equal #t #t)

(define (read-lines port)
    (let ((s (read-line port)))
        (if (eof-object? s)
            '()
            (cons s (read-lines port)))))

(define stdread
    (cond-expand
        (windows "..\\windows\\debug\\stdread.exe")
        (else "../unix/debug/stdread")))

(define stdwrite
    (cond-expand
        (windows "..\\windows\\debug\\stdwrite.exe")
        (else "../unix/debug/stdwrite")))

(define exitcode
    (cond-expand
        (windows "..\\windows\\debug\\exitcode.exe")
        (else "../unix/debug/exitcode")))

(define hang
    (cond-expand
        (windows "..\\windows\\debug\\hang.exe")
        (else "../unix/debug/hang")))

;; subprocess

(check-equal 0
    (call-with-values
        (lambda ()
            (subprocess (current-output-port) #f (current-error-port)
                    stdread "ABC" "DEF GHI" "JKLMNOPQR"))
        (lambda (sub in out err)
            (for-each
                (lambda (line)
                    (display line out)
                    (newline out))
                '("ABC" "DEF GHI" "JKLMNOPQR"))
            (close-port out)
            (subprocess-wait sub)
            (subprocess-status sub))))

(check-equal ("abc" "def\\ghi" "jklmn\"opqr")
    (call-with-values
        (lambda ()
            (subprocess #f (current-input-port) (current-error-port)
                    stdwrite "--stdout" "abc" "def\\ghi" "jklmn\"opqr"))
        (lambda (sub in out err)
            (let ((lines (read-lines in)))
                (close-port in)
                (subprocess-wait sub)
                (if (zero? (subprocess-status sub))
                    lines
                    #f)))))

(check-equal ("abc" "defghi" "jklmnopqr")
    (call-with-values
        (lambda ()
            (subprocess #f (current-input-port) 'stdout
                    stdwrite "--stderr" "abc" "defghi" "jklmnopqr"))
        (lambda (sub in out err)
            (let ((lines (read-lines in)))
                (close-port in)
                (subprocess-wait sub)
                (if (zero? (subprocess-status sub))
                    lines
                    #f)))))

(check-equal ("abc" "defghi" "jklmnopqr")
    (call-with-values
        (lambda ()
            (subprocess (current-output-port) (current-input-port) #f
                    stdwrite "--stderr" "abc" "defghi" "jklmnopqr"))
        (lambda (sub in out err)
            (let ((lines (read-lines err)))
                (close-port err)
                (subprocess-wait sub)
                (if (zero? (subprocess-status sub))
                    lines
                    #f)))))

(check-equal 234
    (call-with-values
        (lambda ()
            (subprocess (current-output-port) (current-input-port) (current-error-port)
                    exitcode "234"))
        (lambda (sub in out err)
            (subprocess-wait sub)
            (subprocess-status sub))))

;; subprocess-wait
;; subprocess-status
;; subprocess-kill
;; subprocess-pid
;; subprocess?

(define-values (sub1 in1 out1 err1) (subprocess #f #f 'stdout hang))
(check-equal #t (subprocess? sub1))
(check-equal #f (subprocess? in1))
(check-equal #f err1)
(check-equal #t (input-port-open? in1))
(check-equal #t (output-port-open? out1))

(check-equal running (subprocess-status sub1))
(check-equal #t (and (integer? (subprocess-pid sub1)) (> (subprocess-pid sub1) 0)))

(subprocess-kill sub1 #t)
(subprocess-wait sub1)
(check-equal #t (integer? (subprocess-status sub1)))

(close-port in1)
(close-port out1)

;; process/ports
;; process*/ports

(check-equal 0
    (let* ((lst (process*/ports (current-output-port) #f (current-error-port)
                    stdread "ABC" "DEFGHI" "JKLMNOPQR"))
            (out (cadr lst))
            (ctrl (cadddr (cdr lst))))
        (for-each
            (lambda (line)
                (display line out)
                (newline out))
            '("ABC" "DEFGHI" "JKLMNOPQR"))
        (close-port out)
        (ctrl 'wait)
        (ctrl 'exit-code)))

(check-equal ("abc" "defghi" "jklmnopqr")
    (let* ((lst (process*/ports #f (current-input-port) (current-error-port)
                        stdwrite "--stdout" "abc" "defghi" "jklmnopqr"))
            (in (car lst))
            (ctrl (cadddr (cdr lst))))
        (let ((lines (read-lines in)))
            (close-port in)
            (ctrl 'wait)
            (if (zero? (ctrl 'exit-code))
                lines
                #f))))

(check-equal ("abc" "defghi" "jklmnopqr")
    (let* ((lst (process/ports #f (current-input-port) 'stdout
                        "stdwrite --stderr abc defghi jklmnopqr"))
            (in (car lst))
            (ctrl (cadddr (cdr lst))))
        (let ((lines (read-lines in)))
            (close-port in)
            (ctrl 'wait)
            (if (zero? (ctrl 'exit-code))
                lines
                #f))))

(check-equal ("abc" "defghi" "jklmnopqr")
    (let* ((lst (process*/ports (current-output-port) (current-input-port) #f
                        stdwrite "--stderr" "abc" "defghi" "jklmnopqr"))
            (err (cadddr lst))
            (ctrl (cadddr (cdr lst))))
        (let ((lines (read-lines err)))
            (close-port err)
            (ctrl 'wait)
            (if (zero? (ctrl 'exit-code))
                lines
                #f))))

(check-equal 123
    (let* ((lst (process*/ports (current-output-port) (current-input-port) (current-error-port)
                        exitcode "123"))
            (ctrl (cadddr (cdr lst))))
            (ctrl 'wait)
            (ctrl 'exit-code)))

;; (<ctrl> 'status)
;; (<ctrl> 'exit-code)
;; (<ctrl> 'wait)
;; (<ctrl> 'kill)

(define p2 (process*/ports #f #f 'stdout hang))
(define in2 (car p2))
(define out2 (cadr p2))
(check-equal #t
    (let ((pid (caddr p2)))
        (and (integer? pid) (> pid 0))))
(define err2 (cadddr p2))
(define ctrl2 (cadddr (cdr p2)))

(check-equal #f err2)
(check-equal #t (input-port-open? in2))
(check-equal #t (output-port-open? out2))

(check-equal running (ctrl2 'status))
(check-equal #f (ctrl2 'exit-code))

(ctrl2 'kill)
(ctrl2 'wait)
(check-equal #t
    (eq? (cond-expand (windows 'done-ok) (else 'done-error))
        (ctrl2 'status)))

(check-equal #t (integer? (ctrl2 'exit-code)))

;; system
;; system*

(define (with-output-to-string proc)
    (let ((port (open-output-string)))
        (parameterize ((current-output-port port)) (proc))
        (get-output-string port)))

(check-equal #t
    (string=? (substring "abcdefghijklmnopqr" 0 18)
        (substring
            (with-output-to-string
                (lambda ()
                    (system "stdwrite --stdout abcdefghijklmnopqr")))
            0 18)))

(define (with-input-from-string s proc)
    (let ((port (open-input-string s)))
        (parameterize ((current-input-port port)) (proc))))

(check-equal #t
    (with-input-from-string "abcdef\n"
        (lambda ()
            (system* stdread "abcdef"))))

(check-equal #f
    (with-input-from-string "abcdef"
        (lambda ()
            (system* stdread "abc" "def"))))

;; system/exit-code
;; system*/exit-code

(check-equal 88
    (system/exit-code "exitcode 88"))

(check-equal 88
    (system*/exit-code exitcode "88"))

;; chicken process module

(define (call-with-input-pipe cmdline proc)
    (let* ((lst (process/ports #f (current-input-port) (current-error-port) cmdline))
            (in (car lst))
            (ctrl (cadddr (cdr lst))))
        (let-values ((results (proc in)))
            (close-port in)
            (ctrl 'wait)
            (if (= (ctrl 'exit-code) 0)
                (apply values results)
                #f))))

(check-equal ("aaa" "bbb" "ccc" "ddd")
    (call-with-input-pipe "stdwrite --stdout aaa bbb ccc ddd"
        (lambda (in) (read-lines in))))

(define (call-with-output-pipe cmdline proc)
    (let* ((lst (process/ports (current-output-port) #f (current-error-port) cmdline))
            (out (cadr lst))
            (ctrl (cadddr (cdr lst))))
        (let-values ((results (proc out)))
            (close-port out)
            (ctrl 'wait)
            (if (= (ctrl 'exit-code) 0)
                (apply values results)
                #f))))

(check-equal #t
    (call-with-output-pipe "stdread 1 22 333 4444 55555"
        (lambda (out)
            (for-each
                (lambda (line)
                    (display line out)
                    (newline out))
                '("1" "22" "333" "4444" "55555"))
            #t)))

(define (with-input-from-pipe cmdline proc)
    (call-with-input-pipe cmdline
        (lambda (port)
            (parameterize ((current-input-port port)) (proc)))))

(check-equal ("aaa" "bbb" "ccc" "ddd")
    (with-input-from-pipe "stdwrite --stdout aaa bbb ccc ddd"
        (lambda () (read-lines (current-input-port)))))

(define (with-output-to-pipe cmdline proc)
    (call-with-output-pipe cmdline
        (lambda (port)
            (parameterize ((current-output-port port)) (proc)))))

(check-equal #t
    (with-output-to-pipe "stdread 1 22 333 4444 55555"
        (lambda ()
            (for-each
                (lambda (line)
                    (display line)
                    (newline))
                '("1" "22" "333" "4444" "55555"))
            #t)))
