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
        bytestring-replace
        bytevector=?
        bytevector<?
        bytevector>?
        bytevector<=?
        bytevector>=?
        (rename bytevector=? bytestring=?)
        (rename bytevector<? bytestring<?)
        (rename bytevector>? bytestring>?)
        (rename bytevector<=? bytestring<=?)
        (rename bytevector>=? bytestring>=?)
        bytestring-index
        bytestring-index-right
        bytestring-break
        bytestring-span
        bytestring-join
        bytestring-split
        read-textual-bytestring
        write-textual-bytestring
        write-binary-bytestring
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
        (define bytestring-replace
            (case-lambda
                ((bv1 bv2 start1 end1)
                    (%bytestring-replace bv1 bv2 start1 end1 0 (bytevector-length bv2)))
                ((bv1 bv2 start1 end1 start2 end2)
                    (%bytestring-replace bv1 bv2 start1 end1 start2 end2))))
        (define (%bytestring-replace bv1 bv2 start1 end1 start2 end2)
            (let* ((len (+ (- (bytevector-length bv1) (- end1 start1)) (- end2 start2)))
                    (bv (make-bytevector len)))
                (bytevector-copy! bv 0 bv1 0 start1)
                (bytevector-copy! bv start1 bv2 start2 end2)
                (bytevector-copy! bv (+ start1 (- end2 start2)) bv1 end1 (bytevector-length bv1))
                bv))
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
        (define (bytestring-break bv pred)
            (let ((idx (bytestring-index bv pred)))
                (if (not idx)
                    (values (bytevector-copy bv) (bytevector))
                    (values (bytevector-copy bv 0 idx) (bytevector-copy bv idx)))))
        (define (bytestring-span bv pred)
            (bytestring-break bv (lambda (b) (not (pred b)))))
        (define bytestring-join
            (case-lambda
                ((lst delim) (%bytestring-join lst (bytestring delim) 'infix))
                ((lst delim grammar) (%bytestring-join lst (bytestring delim) grammar))))
        (define (%bytestring-join lst delim grammar)
            (if (null? lst)
                (if (eq? grammar 'strict-infix)
                    (full-error 'assertion-violation 'bytestring-join 'bytestring-error
                            "bytestring-join: list must not be empty with strict-inflix")
                    (bytevector))
                (let ((port (open-output-bytevector)))
                    (if (eq? grammar 'prefix)
                        (write-bytevector delim port))
                    (write-bytevector (car lst) port)
                    (for-each
                        (lambda (bv)
                            (write-bytevector delim port)
                            (write-bytevector bv port))
                        (cdr lst))
                    (if (eq? grammar 'suffix)
                        (write-bytevector delim port))
                    (get-output-bytevector port))))
        (define bytestring-split
            (case-lambda
                ((bv delim)
                    (%bytestring-split bv (if (char? delim) (char->integer delim) delim) 'infix))
                ((bv delim grammar)
                    (%bytestring-split bv (if (char? delim) (char->integer delim) delim)
                            grammar))))
        (define (%bytestring-split bv delim grammar)
            (let* ((len (bytevector-length bv))
                    (len (if (and (eq? grammar 'suffix)
                                    (= (bytevector-u8-ref bv (- len 1)) delim))
                             (- len 1)
                             len)))
                (define (split sdx idx)
                    (if (= idx len)
                        (list (bytevector-copy bv sdx idx))
                        (if (= (bytevector-u8-ref bv idx) delim)
                            (cons (bytevector-copy bv sdx idx) (split (+ idx 1) (+ idx 1)))
                            (split sdx (+ idx 1)))))
                (if (= len 0)
                    '()
                    (if (and (eq? grammar 'prefix) (= (bytevector-u8-ref bv 0) delim))
                        (split 1 1)
                        (split 0 0)))))
        (define (write-binary-bytestring port . args)
            (write-bytevector (apply bytestring args) port))
        (define (bytestring-error? obj)
            (and (error-object? obj) (eq? (error-object-kind obj) 'bytestring-error)))
        )
    )
