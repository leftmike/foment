(define-library (srfi 106)
    (import (foment base))
    (export
        make-client-socket
        make-server-socket
        socket?
        (rename accept-socket socket-accept)
        socket-send
        socket-recv
        (rename shutdown-socket socket-shutdown)
        socket-input-port
        socket-output-port
        call-with-socket
        address-family
        address-info
        socket-domain
        ip-protocol
        message-type
        shutdown-method
        socket-merge-flags
        socket-purge-flags
        *af-unspec*
        *af-inet*
        *af-inet6*
        *sock-stream*
        *sock-dgram*
        *ai-canonname*
        *ai-numerichost*
        *ai-v4mapped*
        *ai-all*
        *ai-addrconfig*
        *ipproto-ip*
        *ipproto-tcp*
        *ipproto-udp*
        *msg-peek*
        *msg-oob*
        *msg-waitall*
        *shut-rd*
        *shut-wr*
        *shut-rdwr*)
    (begin
        (define make-client-socket
           (case-lambda
               ((node svc)
                   (make-client-socket node svc *af-inet* *sock-stream*
                           (socket-merge-flags *ai-v4mapped* *ai-addrconfig*) *ipproto-ip*))
               ((node svc fam)
                   (make-client-socket node svc fam *sock-stream*
                           (socket-merge-flags *ai-v4mapped* *ai-addrconfig*) *ipproto-ip*))
               ((node svc fam type)
                   (make-client-socket node svc fam type
                           (socket-merge-flags *ai-v4mapped* *ai-addrconfig*) *ipproto-ip*))
               ((node svc fam type flags)
                   (make-client-socket node svc fam type flags *ipproto-ip*))
               ((node svc fam type flags prot)
                   (let ((s (make-socket fam type prot)))
                       (connect-socket s node svc fam type flags prot)
                       s))))

        (define make-server-socket
            (case-lambda
                ((svc) (make-server-socket svc *af-inet* *sock-stream* *ipproto-ip*))
                ((svc fam) (make-server-socket svc fam *sock-stream* *ipproto-ip*))
                ((svc fam type) (make-server-socket svc fam type *ipproto-ip*))
                ((svc fam type prot)
                    (let ((s (make-socket fam type prot)))
                        (bind-socket s "" svc fam type prot)
                        (listen-socket s)
                        s))))

        (define socket-send
            (case-lambda
                ((socket bv) (send-socket socket bv 0))
                ((socket bv flags) (send-socket socket bv flags))))

        (define socket-recv
            (case-lambda
                ((socket size) (recv-socket socket size 0))
                ((socket size flags) (recv-socket socket size flags))))

        (define (socket-close socket) (close-port socket))

        (define (socket-input-port socket) socket)

        (define (socket-output-port socket) socket)

        (define (call-with-socket socket proc)
            (let-values ((results (proc socket)))
                (close-port socket)
                (apply values results)))
    ))
