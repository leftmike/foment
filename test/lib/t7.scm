(define-library (lib t7)
    (cond-expand
        (windows (include-library-declarations "..\\test\\lib\\t7-ild.scm"))
        (else (include-library-declarations "../test/lib/t7-ild.scm"))))
