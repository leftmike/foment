(define-library (srfi 193)
    (import (foment base))
    (export
        command-line
        command-name
        command-args
        script-file
        script-directory)
    (begin
        (define base-directory (current-directory))
        (define path-char (cond-expand (windows #\\) (else #\/)))
        (define (string-last-char s ch)
            (define (last-char idx)
                (if (or (< idx 0) (eq? (string-ref s idx) ch))
                    idx
                    (last-char (- idx 1))))
            (last-char (- (string-length s) 1)))
        (define (strip-extension name)
            (let ((edx (string-last-char name #\.))
                    (pdx (string-last-char name path-char)))
                (if (> edx pdx)
                    (string-copy name 0 edx)
                    name)))
        (define (strip-directory name)
            (let ((pdx (string-last-char name path-char)))
                (if (>= pdx 0)
                    (string-copy name (+ pdx 1))
                    name)))
        (define (strip-filename name)
            (let ((pdx (string-last-char name path-char)))
                (if (>= pdx 0)
                    (string-copy name 0 (+ pdx 1))
                    "")))
        (define (command-name)
            (let ((name (car (command-line))))
                (if (equal? name "")
                    #f
                    (strip-directory (strip-extension name)))))
        (define (command-args) (cdr (command-line)))
        (define (script-file)
            (let ((name (car (command-line))))
                (if (equal? name "")
                    #f
                    (if (eq? (string-ref name 0) path-char)
                        name
                        (string-append base-directory (make-string 1 path-char) name)))))
        (define (script-directory)
          (let ((name (script-file)))
            (if name
                (strip-filename name)
                #f)))
    ))
