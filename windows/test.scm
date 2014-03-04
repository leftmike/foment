(import (foment base))

(define (server)
    (define (loop s)
;;        (let ((bv (recv-socket s 128 0)))
;;            (if (> (bytevector-length bv) 0)
        (let ((bv (read-bytevector 128 s)))
            (if (not (eof-object? bv))
                (begin
                    (write (utf8->string bv))
                    (loop s)))))
    (let ((s (make-socket (address-family inet) (socket-domain stream) (ip-protocol tcp))))
        (bind-socket s "localhost" "12345" (address-family inet) (socket-domain stream)
                (ip-protocol tcp))
        (listen-socket s)
        (loop (accept-socket s))))

(define (client)
    (define (loop s)
;;        (socket-send s (string->utf8 (read-line)) 0)
        (write-bytevector (string->utf8 (read-line)) s)
        (loop s))
    (let ((s (make-socket (address-family inet) (socket-domain stream) (ip-protocol tcp))))
        (connect-socket s "localhost" "12345" (address-family inet) (socket-domain stream)
                0 (ip-protocol tcp))
        (loop s)))

(cond
    ((member "client" (command-line)) (client))
    ((member "server" (command-line)) (server))
    (else (display "error: expected client or server on the command line") (newline)))

