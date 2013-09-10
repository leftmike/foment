;;;
;;; Stress Tests
;;;

(import (foment bedrock))

(define p (make-parameter 0))

; An implementation of a mailbox used by producer(s) and consumer(s).

(define not-empty (make-condition))
(define not-full (make-condition))
(define lock (make-exclusive))
(define mailbox #f)
(define mailbox-full #f)
(define next-item 1)
(define last-item 100)

(define (producer)
    (sleep (random 100))
    (enter-exclusive lock)
    (let ((item next-item))
        (define (put item)
            (if mailbox-full
                (begin
                    (condition-wait not-full lock)
                    (put item))
                (begin
                    (set! mailbox item)
                    (set! mailbox-full #t)
                    (leave-exclusive lock)
                    (condition-wake not-empty))))
        (set! next-item (+ next-item 1))
        (if (> next-item last-item)
            (leave-exclusive lock) ; All done.
            (begin
                (put item)
                (producer)))))

(define (consumer)
    (define (get)
        (if mailbox-full
            (begin
                (set! mailbox-full #f)
                (let ((item mailbox))
                    (condition-wake not-full)
                    (leave-exclusive lock)
                    item))
            (begin
                (condition-wait not-empty lock)
                (get))))
    (enter-exclusive lock)
    (let ((item (get)))
        (write item)
        (display " ")
        (consumer)))

(p 1)
(run-thread producer)
(parameterize ((p 2)) (run-thread producer))
(parameterize ((p 3)) (run-thread consumer)
    (parameterize ((p 4)) (run-thread consumer)))
(run-thread consumer)
(run-thread consumer)

(sleep 3000)
(display "
")

(define e1 (make-exclusive))
(define c1 (make-condition))

(define (stress1)
    (enter-exclusive e1)
    (condition-wait c1 e1)
    (leave-exclusive e1)
    (stress1))

(define (stress2)
    (sleep (random 1000))
    (wake-condition c1)
    (stress2))

(run-thread stress1)
(run-thread stress1)
(run-thread stress2)
(run-thread stress2)

(define e2 (make-exclusive))
(enter-exclusive e2)
(run-thread (lambda () (enter-exclusive e2)))

(run-thread (lambda () (define (recur n) (recur (+ n 1))) (recur 0)))
(run-thread (lambda () (define (recur n) (recur (+ n 1))) (recur 0)))
(run-thread (lambda () (define (recur n) (recur (+ n 1))) (recur 0)))



