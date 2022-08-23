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
        bytestring-pad
        bytestring-pad-right
        bytestring-trim
        bytestring-trim-right
        bytestring-trim-both

        bytestring-index
        bytestring-index-right

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
        (define (bytestring-pad bv len pad)
            (let ((padding (- len (bytevector-length bv))))
                (if (< padding 0)
                    (bytevector-copy bv)
                    (let ((result (make-bytevector len (if (char? pad) (char->integer pad) pad))))
                        (bytevector-copy! result padding bv)
                        result))))
        (define (bytestring-pad-right bv len pad)
            (if (< (- len (bytevector-length bv)) 0)
                (bytevector-copy bv)
                (let ((result (make-bytevector len (if (char? pad) (char->integer pad) pad))))
                    (bytevector-copy! result 0 bv)
                    result)))
        (define (bytestring-trim bv pred)
            (let ((start (bytestring-index bv (lambda (n) (not (pred n))))))
                (if start
                    (bytevector-copy bv start)
                    #u8())))
        (define (bytestring-trim-right bv pred)
            (let ((end (bytestring-index-right bv (lambda (n) (not (pred n))))))
                (if end
                    (bytevector-copy bv 0 (+ end 1))
                    #u8())))
        (define (bytestring-trim-both bv pred)
            (let ((start (bytestring-index bv (lambda (n) (not (pred n))))))
                (if start
                    (bytevector-copy bv start
                            (+ (bytestring-index-right bv (lambda (n) (not (pred n)))) 1))
                    #u8())))

        (define (%bytestring-index bv pred start end)
            (if (>= start end)
                #f
                (if (pred (bytevector-u8-ref bv start))
                    start
                    (%bytestring-index bv pred (+ start 1) end))))
        (define bytestring-index
            (case-lambda
                ((bv pred) (%bytestring-index bv pred 0 (bytevector-length bv)))
                ((bv pred start) (%bytestring-index bv pred start (bytevector-length bv)))
                ((bv pred start end) (%bytestring-index bv pred start end))))
        (define (%bytestring-index-right bv pred start end)
            (let ((end (- end 1)))
                (if (>= start end)
                    #f
                    (if (pred (bytevector-u8-ref bv end))
                        end
                        (%bytestring-index-right bv pred start end)))))
        (define bytestring-index-right
            (case-lambda
                ((bv pred) (%bytestring-index-right bv pred 0 (bytevector-length bv)))
                ((bv pred start) (%bytestring-index-right bv pred start (bytevector-length bv)))
                ((bv pred start end) (%bytestring-index-right bv pred start end))))

        (define (bytestring-error? obj)
            (and (error-object? obj) (eq? (error-object-kind obj) 'bytestring-error)))
        )
    )
