;;;
;;; Sleep
;;;

(import (foment base))

(define (current-seconds) (time->seconds (current-time)))
(define s (current-seconds))

(thread-sleep! 0)

(check-equal #t (<= s (current-seconds)))

(set! s (current-seconds))

(thread-sleep! 2)

(check-equal #t (< s (current-seconds)))

(define t (current-time))
(set! s (time->seconds t))

(thread-sleep! (seconds->time (+ s 2)))

(check-equal #t (< s (current-seconds)))

(check-error (assertion-violation thread-sleep!) (thread-sleep!))
(check-error (assertion-violation thread-sleep!) (thread-sleep! #f))
