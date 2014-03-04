(import (foment base))

(define (server)
    (define (loop s)
;;        (let ((bv (socket-recv s 128 0)))
;;            (if (> (bytevector-length bv) 0)
        (let ((bv (read-bytevector 128 s)))
            (if (not (eof-object? bv))
                (begin
                    (write (utf8->string bv))
                    (loop s)))))
    (let ((s (make-socket (address-family inet) (socket-domain stream) (ip-protocol tcp))))
        (socket-bind s "localhost" "12345" (address-family inet) (socket-domain stream) (ip-protocol tcp))
        (socket-listen s)
        (loop (socket-accept s))))

(define (client)
    (define (loop s)
;;        (socket-send s (string->utf8 (read-line)) 0)
        (write-bytevector (string->utf8 (read-line)) s)
        (loop s))
    (let ((s (make-socket (address-family inet) (socket-domain stream) (ip-protocol tcp))))
        (socket-connect s "localhost" "12345" (address-family inet) (socket-domain stream)
                (ip-protocol tcp))
        (loop s)))

(cond
    ((member "client" (command-line)) (client))
    ((member "server" (command-line)) (server))
    (else (display "error: expected client or server on the command line") (newline)))

