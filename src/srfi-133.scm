(define-library (scheme vector)
    (aka (srfi 133))
    (import (foment base))
    (export
        make-vector
        vector
        vector-unfold
        vector-unfold-right
        vector-copy
        vector-reverse-copy
        vector-append
        vector-concatenate
        vector-append-subvectors
        vector?
        vector-empty?
        vector=
        vector-ref
        vector-length
        vector-fold
        vector-fold-right
        vector-map
        vector-map!
        vector-for-each
        vector-count
        vector-cumulate
        vector-index
        vector-index-right
        vector-skip
        vector-skip-right
        vector-binary-search
        vector-any
        vector-every
        vector-partition
        vector-set!
        vector-swap!
        vector-fill!
        vector-reverse!
        vector-copy!
        vector-reverse-copy!
        vector-unfold!
        vector-unfold-right!
        vector->list
        reverse-vector->list
        list->vector
        reverse-list->vector
        vector->string
        string->vector
        )
    (begin
        (define (vector-concatenate vector-list)
            (apply vector-append vector-list))
        (define (vector-empty? vec)
            (= (vector-length vec) 0))
        (define (vector= elt=? . vectors)
            (define (length= len vectors)
                (or (null? vectors)
                    (and (= len (vector-length (car vectors)))
                        (length= len (cdr vectors)))))
            (define (vector-2= idx vec1 vec2)
                (or (= idx (vector-length vec1))
                    (and (elt=? (vector-ref vec1 idx) (vector-ref vec2 idx))
                        (vector-2= (+ idx 1) vec1 vec2))))
            (define (vector-list= vec vectors)
                (or (null? vectors)
                    (and (vector-2= 0 vec (car vectors))
                        (vector-list= (car vectors) (cdr vectors)))))
            (or (null? vectors) (null? (cdr vectors))
                (and (length= (vector-length (car vectors)) (cdr vectors))
                     (vector-list= (car vectors) (cdr vectors)))))
        (define (fold step state idx next done vectors)
            (if (done idx)
                state
                (let ((state (apply step state (map (lambda (vec) (vector-ref vec idx)) vectors))))
                  (fold step state (next idx 1) next done vectors))))
        (define (fold-left step start vectors)
            (let ((len (apply min (map vector-length vectors))))
                (fold step start 0 + (lambda (idx) (= idx len)) vectors)))
        (define (vector-fold step start . vectors)
            (if (null? vectors)
                start
                (fold-left step start vectors)))
        (define (vector-fold-right step start . vectors)
            (if (null? vectors)
                start
                (let ((len (apply min (map vector-length vectors))))
                    (fold step start (- len 1) - (lambda (idx) (< idx 0)) vectors))))
        (define (vector-map! proc vec . vectors)
          (let* ((vectors (cons vec vectors))
                 (len (apply min (map vector-length vectors))))
            (define (map-vectors idx)
              (if (< idx len)
                  (let ((val (apply proc (map (lambda (v) (vector-ref v idx)) vectors))))
                    (vector-set! vec idx val)
                    (map-vectors (+ idx 1)))))
            (map-vectors 0)))
        (define (vector-count pred? vec . vectors)
            (fold-left (lambda (count . elts) (if (apply pred? elts) (+ count 1) count)) 0
                    (cons vec vectors)))
        (define (vector-cumulate f start vec)
            (let* ((len (vector-length vec))
                   (ret (make-vector len)))
                (define (cumulate idx prev)
                    (if (= idx len)
                        ret
                        (let ((val (f prev (vector-ref vec idx))))
                            (vector-set! ret idx val)
                            (cumulate (+ idx 1) val))))
                (cumulate 0 start)))
        (define (index pred? idx next done vectors)
            (if (done idx)
                #f
                (if (apply pred? (map (lambda (vec) (vector-ref vec idx)) vectors))
                    idx
                    (index pred? (next idx 1) next done vectors))))
        (define (vector-index pred? vec . vectors)
            (let* ((vectors (cons vec vectors))
                   (len (apply min (map vector-length vectors))))
                (index pred? 0 + (lambda (idx) (= idx len)) vectors)))
        (define (vector-index-right pred? vec . vectors)
            (let* ((vectors (cons vec vectors))
                   (len (apply min (map vector-length vectors))))
                (index pred? (- len 1) - (lambda (idx) (< idx 0)) vectors)))
        (define (vector-skip pred? vec . vectors)
            (let* ((vectors (cons vec vectors))
                   (len (apply min (map vector-length vectors))))
                (index (lambda elts (not (apply pred? elts))) 0 + (lambda (idx) (= idx len))
                        vectors)))
        (define (vector-skip-right pred? vec . vectors)
            (let* ((vectors (cons vec vectors))
                   (len (apply min (map vector-length vectors))))
                (index (lambda elts (not (apply pred? elts))) (- len 1) -
                        (lambda (idx) (< idx 0)) vectors)))
        (define (vector-binary-search vec val cmp)
            (define (binary-search start end)
                (if (> 2 (- end start))
                    #f
                    (let* ((idx (+ start (truncate-quotient (- end start) 2)))
                           (ret (cmp (vector-ref vec idx) val)))
                        (cond
                            ((< ret 0) (binary-search idx end))
                            ((= ret 0) idx)
                            ((> ret 0) (binary-search start idx))))))
            (binary-search -1 (vector-length vec)))
        (define (vector-any pred? vec . vectors)
            (let* ((vectors (cons vec vectors))
                   (len (apply min (map vector-length vectors))))
                (define (any idx)
                    (if (= idx len)
                        #f
                        (let ((ret (apply pred? (map (lambda (vec) (vector-ref vec idx)) vectors))))
                            (if ret
                            ret
                            (any (+ idx 1))))))
            (any 0)))
        (define (vector-every pred? vec . vectors)
            (let* ((vectors (cons vec vectors))
                   (len (apply min (map vector-length vectors))))
                (define (every idx ret)
                    (if (= idx len)
                        ret
                        (let ((ret (apply pred? (map (lambda (vec) (vector-ref vec idx)) vectors))))
                            (if ret
                                (every (+ idx 1) ret)
                                #f))))
            (every 0 #f)))
        (define (vector-partition pred? vec)
            (let* ((len (vector-length vec))
                   (cnt (vector-count pred? vec))
                   (ret (make-vector len)))
                (define (partition idx yes no)
                    (if (= idx len)
                        (values ret cnt)
                        (let ((elt (vector-ref vec idx)))
                            (cond
                                ((pred? elt)
                                    (vector-set! ret yes elt)
                                    (partition (+ idx 1) (+ yes 1) no))
                                (else
                                    (vector-set! ret no elt)
                                    (partition (+ idx 1) yes (+ no 1)))))))
            (partition 0 0 cnt)))
        (define (vector-unfold f len . seeds)
            (let ((vec (make-vector len)))
                (apply vector-unfold! f vec 0 len seeds)
                vec))
        (define (vector-unfold! f vec start end . seeds)
            (define (unfold idx seeds)
                (if (< idx end)
                    (let-values (((val . seeds) (apply f idx seeds)))
                        (vector-set! vec idx val)
                        (unfold (+ idx 1) seeds))))
            (unfold start seeds))
        (define (vector-unfold-right f len . seeds)
            (let ((vec (make-vector len)))
                (apply vector-unfold-right! f vec 0 len seeds)
                vec))
        (define (vector-unfold-right! f vec start end . seeds)
            (define (unfold idx seeds)
                (if (>= idx start)
                    (let-values (((val . seeds) (apply f idx seeds)))
                        (vector-set! vec idx val)
                        (unfold (- idx 1) seeds))))
            (unfold (- end 1) seeds))
        (define (reverse-vector->list vec . args)
              (reverse (apply vector->list vec args)))
        (define (reverse-list->vector lst)
            (let ((vec (list->vector lst)))
                (vector-reverse! vec)
                vec))
    ))
