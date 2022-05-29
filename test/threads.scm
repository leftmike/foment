;;;
;;; Threads
;;;

(import (foment base))
(import (srfi 18))

(define (current-seconds) (time->seconds (current-time)))
(define s (current-seconds))

(thread-sleep! 0)

(check-equal #t (<= s (current-seconds)))

(set! s (current-seconds))

(thread-sleep! 2)

(check-equal #t (< s (current-seconds)))

(thread-sleep! (seconds->time (- (current-seconds) 10)))

(define t (current-time))
(set! s (time->seconds t))

(thread-sleep! (seconds->time (+ s 2)))

(check-equal #t (< s (current-seconds)))

(check-error (assertion-violation thread-sleep!) (thread-sleep!))
(check-error (assertion-violation thread-sleep!) (thread-sleep! #f))

(check-equal #t (thread? (current-thread)))
(check-equal #f (thread? 'thread))

(define result (cons #t #t))

(define thrd (make-thread (lambda () result) 'thread-name))

(check-equal thread-name (thread-name thrd))
(check-equal #t (thread? thrd))

(define specific (cons #f #f))

(thread-specific-set! thrd specific)
(check-equal #t (eq? (thread-specific thrd) specific))

(thread-start! thrd)

(check-equal thread-name (thread-name thrd))
(check-equal #t (thread? thrd))
(check-equal #t (eq? (thread-specific thrd) specific))

(thread-yield!)

(check-equal #t (eq? (thread-join! thrd) result))

(check-equal thread-name (thread-name thrd))
(check-equal #t (thread? thrd))
(check-equal #t (eq? (thread-specific thrd) specific))

(define thrd (thread-start! (make-thread (lambda () (raise specific)))))

(check-equal #t
    (eq? specific
        (guard (obj ((uncaught-exception? obj) (uncaught-exception-reason obj)))
            (thread-join! thrd))))

(define thrd (thread-start! (make-thread (lambda () (thread-sleep! 1) result))))

(check-equal #t (eq? result (thread-join! thrd)))

(define thrd (thread-start! (make-thread (lambda () (thread-sleep! 9999)))))

(thread-terminate! thrd)

(check-equal #t
    (guard (obj ((terminated-thread-exception? obj) #t))
        (thread-join! thrd)))

(define thrd (thread-start! (make-thread (lambda () result))))

(thread-sleep! 1)

(thread-terminate! thrd)

(check-equal #t (eq? result (thread-join! thrd)))

(define thrd (thread-start! (make-thread (lambda () (thread-sleep! 9999)))))

(check-equal #t
    (guard (obj ((join-timeout-exception? obj) #t))
        (thread-join! thrd 1)))

(check-equal #t
    (guard (obj ((join-timeout-exception? obj) #t))
        (thread-join! thrd (seconds->time (+ (time->seconds (current-time)) 1)))))

(check-equal timed-out
    (thread-join! thrd 1 'timed-out))

(check-equal timed-out
    (thread-join! thrd (seconds->time (+ (time->seconds (current-time)) 1)) 'timed-out))

(thread-terminate! thrd)

(check-equal #t
    (guard (obj ((terminated-thread-exception? obj) #t))
        (thread-join! thrd)))

(define mux (make-mutex 'mutex-name))

(check-equal #t (mutex? (make-mutex)))
(check-equal #t (mutex? mux))
(check-equal #f (mutex? 'mutex))

(check-equal mutex-name (mutex-name mux))

(check-equal #f (eq? (mutex-specific mux) specific))
(mutex-specific-set! mux specific)
(check-equal #t (eq? (mutex-specific mux) specific))

(check-equal #t (procedure? mutex-state))
(check-equal #t (procedure? abandoned-mutex-exception?))

(check-equal #t (try-exclusive mux))
(mutex-unlock! mux)

(run-thread (lambda () (mutex-lock! mux) (sleep 1000) (mutex-unlock! mux)))
(sleep 100)

(check-equal #f (try-exclusive mux))

(define cv (make-condition-variable 'condition-variable-name))

(check-equal #t (condition-variable? (make-condition-variable)))
(check-equal #t (condition-variable? cv))
(check-equal #f (condition-variable? 'condition-variable))

(check-equal condition-variable-name (condition-variable-name cv))

(check-equal #f (eq? (condition-variable-specific cv) specific))
(condition-variable-specific-set! cv specific)
(check-equal #t (eq? (condition-variable-specific cv) specific))

(define mux (make-mutex))
(define done #f)
(define result #f)

(define (wait-for-done cnt)
    (if done
        (begin
            (set! result cnt)
            (mutex-unlock! mux))
        (begin
            (mutex-unlock! mux cv)
            (wait-for-done (+ cnt 1)))))

(run-thread
    (lambda ()
        (mutex-lock! mux)
        (wait-for-done 0)))

(sleep 100)
(mutex-lock! mux)
(set! done #t)
(mutex-unlock! mux)
(condition-variable-signal! cv)

(define (wait-for-result)
    (mutex-lock! mux)
    (if (not result)
        (begin
            (mutex-unlock! mux)
            (sleep 100)
            (wait-for-result))
        (begin
            (mutex-unlock! mux)
            result)))

(check-equal #t (< 0 (wait-for-result)))

(check-equal #t (eq? (with-exception-handler list current-exception-handler) list))
