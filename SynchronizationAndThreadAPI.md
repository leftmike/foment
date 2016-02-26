# Synchronization And Thread API #

`(import (foment base))` to use these procedures.

## Threads ##

procedure: `(current-thread)`

Returns the current thread.

procedure: `(thread?` _obj_`)`

Returns `#t` if _obj_ is a thread, otherwise returns `#f`.

procedure: `(run-thread` _thunk_`)`

A new thread will be returned. The new thread will execute _thunk_ which is a procedure taking
no arguments.

procedure: `(exit-thread` _obj_`)`

Runs all outstanding `dynamic-wind` _after_ procedures for the current thread.
If the current thread is the only thread, then the running program is terminated. Otherwise, only
the current thread is terminated.

procedure: `(emergency-exit-thread` _obj_`)`

Terminate the current thread.

procedure: `(sleep` _k_`)`

Sleep for _k_ milliseconds.

## Exclusives ##

procedure: `(exclusive?` _obj_`)`

Returns `#t` if _obj_ is an exclusive, otherwise returns `#f`.

procedure: `(make-exclusive)`

Returns a new exclusive.

procedure: `(enter-exclusive` _exclusive_`)`

Wait, if necessary, until the _exclusive_ is available, and then enter it. Only a single thread
at a time may enter the _exclusive_. The wait for the _exclusive_ could be potentially
indefinate.

Exclusives may be entered recursively. For the _exclusive_ to become available, `leave-exclusive`
must be called once for ever call to `enter-exclusive`.

procedure: `(leave-exclusive` _exclusive_`)`

Leave the _exclusive_ and make it available. Another thread may enter it.

procedure: `(try-exclusive` _exclusive_`)`

Attempt to enter the _exclusive_ without waiting. If the _exclusive_ is entered return `#t`,
otherwise return `#f`.

syntax: `(with-exclusive` _exclusive_ _expr1_ _expr2_ _..._`)`

Enter the _exclusive_, evaluate the expressions _expr1_ _expr2_ _..._, and leave the _exclusive_.
The _exclusive_ will be entered when the execution of the expressions begin and when a
capture continuation is invoked.
The _exclusive_ will be left whether the expressions return normally or by an exception being
raised or by a capture continuation being invoked.

## Conditions ##

procedure: `(condition?` _obj_`)`

Returns `#t` if _obj_ is an condition, otherwise returns `#f`.

procedure: `(make-condition)`

Returns a new condition.

procedure: `(condition-wait` _condition_ _exclusive_`)`

Leave _exclusive_ and wait on _condition_. This is done atomically. When `condition-wait` returns
the _exclusive_ will have been entered. Conditions are subject to wakeups not associated with an
explicit wake. You must recheck a predicate when `condition-wait` returns.

procedure: `(condition-wake` _condition_`)`

Wake one thread waiting on _condition_.

procedure: `(condition-wake-all` _condition_ `)`

Wake all threads waiting on _condition_.

## Example ##

Conditions and exclusives are used to implement a mailbox. One or more producer threads put
items into the mailbox and one or more consumer threads get items out of the mailbox.

```
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

(run-thread producer)
(run-thread producer)
(run-thread consumer)
(run-thread consumer)
(run-thread consumer)
(run-thread consumer)

(sleep 4000)
(display "
")
```