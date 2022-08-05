(define-library (srfi 207)
    (import (foment base))
    (export
        bytestring
        make-bytestring
        make-bytestring!
        bytevector->hex-string
        hex-string->bytevector
        bytevector->base64
        base64->bytevector
        bytestring->list
        make-bytestring-generator

        bytestring-error?)
    (begin
        (define (bytestring . args)
            (make-bytestring args))
        (define (make-bytestring! bv at lst)
            (bytevector-copy! bv at (make-bytestring lst)))
        (define (make-bytestring-generator . args)
            (let ((bdx 0) (bv (make-bytestring args)))
                (lambda ()
                    (if (= bdx (bytevector-length bv))
                        (eof-object)
                        (let ((val (bytevector-u8-ref bv bdx)))
                            (set! bdx (+ bdx 1))
                            val)))))

        (define (bytestring-error? obj)
            (and (error-object? obj) (eq? (error-object-kind obj) 'bytestring-error)))
        )
    )
