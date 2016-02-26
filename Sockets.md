# Sockets #

The socket support in Foment is based on
[SRFI 106: Basic socket interface](http://srfi.schemers.org/srfi-106/srfi-106.html) with one
important difference: sockets are ports.

## Procedures ##

`(import (foment base))` to use these procedures.

procedure: `(socket?` _obj_`)`

Return `#t` if _obj_ is a socket and `#f` otherwise.

procedure: `(make-socket` _address-family_ _socket-domain_ _ip-protocol_`)`

Return a new socket. To use this socket to listen for incoming connection requests, `bind-socket`,
`listen-socket`, and `accept-socket` must be used. To connect this socket with a server,
`connect-socket` must be used.
_address-family_, _socket-domain_, and _ip-protocol_ are specified below.

procedure: `(bind-socket` _socket_ _node_ _service_ _address-family_ _socket-domain_
_ip-protocol_`)`

Bind _socket_ to _node_ and _service_. _node_ must be a string. If _node_ is the empty string,
then the _socket_ will be bound to all available IP addresses. If _node_ is `"localhost"`, then
the _socket_ will be bound to all available loopback addresses. _service_ must be a string. It
may be a numeric port number, for example, `"80"`. Or it may be the name of a service, for
example, `"http"`.
_address-family_, _socket-domain_, and _ip-protocol_ are specified below.

procedure: `(listen-socket` _socket_`)`
<br>procedure: <code>(listen-socket</code> <i>socket</i> <i>backlog</i><code>)</code>

Start <i>socket</i> listening for incoming connections. <i>socket</i> must be bound using <code>bind-socket</code>
before calling <code>listen-socket</code>.<br>
<br>
procedure: <code>(accept-socket</code> <i>socket</i><code>)</code>

Wait for a client to connect to the <i>socket</i>. A new socket will be returned which can be used to<br>
communicate with the connected client socket. <i>socket</i> may continue to be used with <code>accept-socket</code>
to wait for more clients to connect. <i>socket</i> must be put into a listening state using<br>
<code>listen-socket</code> before calling <code>accept-socket</code>.<br>
<br>
procedure: <code>(connect-socket</code> <i>socket</i> <i>node</i> <i>service</i> <i>address-family</i> <i>socket-domain</i>
<i>address-info</i> <i>ip-protocol</i><code>)</code>

Connect <i>socket</i> to a server. <i>node</i> and <i>service</i> must be strings.<br>
<i>address-family</i>, <i>socket-domain</i>, <i>address-info</i>, and <i>ip-protocol</i> are specified below.<br>
<br>
procedure: <code>(shutdown-socket</code> <i>socket</i> <i>how</i><code>)</code>

Shutdown the <i>socket</i>. <i>how</i> must be one of <code>*shut-rd*</code>, <code>*shut-wr*</code>, or <code>*shut-rdwr*</code>.<br>
<br>
procedure: <code>(send-socket</code> <i>socket</i> <i>bytevector</i> <i>flags</i><code>)</code>

Send the contents of <i>bytevector</i> on <i>socket</i>. <i>flags</i> must be <code>0</code> or <code>*msg-oob*</code>.<br>
<br>
procedure: <code>(recv-socket</code> <i>socket</i> <i>size</i> <i>flags</i><code>)</code>

Wait for and receive a block of data of <i>size</i> bytes from <i>socket</i>. The data will be returned<br>
as a bytevector. A zero length bytevector indicates that the peer connection is closed.<br>
<i>flags</i> must be <code>0</code>, <code>*msg-peek*</code>, <code>*msg-oob*</code>, or <code>*msg-waitall*</code>.<br>
<br>
procedure: <code>(get-ip-addresses</code> <i>address-family</i><code>)</code>

Return a list of the local IP addresses in the specified <i>address-family</i>.<br>
<br>
<h2>Flag Operations and Constants</h2>

These are the same as<br>
<a href='http://srfi.schemers.org/srfi-106/srfi-106.html'>SRFI 106: Basic socket interface</a>.<br>
<br>
<h2>Example</h2>

This is a simple example of using sockets. The client reads a line of text, converts it<br>
to UTF-8, and sends it to the server. The server receives UTF-8, converts it to a string, and<br>
writes it.<br>
<br>
<pre><code>(import (foment base))<br>
<br>
(define (server)<br>
    (define (loop s)<br>
;;        (let ((bv (recv-socket s 128 0)))<br>
;;            (if (&gt; (bytevector-length bv) 0)<br>
        (let ((bv (read-bytevector 128 s)))<br>
            (if (not (eof-object? bv))<br>
                (begin<br>
                    (display (utf8-&gt;string bv))<br>
                    (newline)<br>
                    (loop s)))))<br>
    (let ((s (make-socket (address-family inet) (socket-domain stream) (ip-protocol tcp))))<br>
        (bind-socket s "localhost" "12345" (address-family inet) (socket-domain stream)<br>
                (ip-protocol tcp))<br>
        (listen-socket s)<br>
        (loop (accept-socket s))))<br>
<br>
(define (client)<br>
    (define (loop s)<br>
;;        (socket-send s (string-&gt;utf8 (read-line)) 0)<br>
        (write-bytevector (string-&gt;utf8 (read-line)) s)<br>
        (loop s))<br>
    (let ((s (make-socket (address-family inet) (socket-domain stream) (ip-protocol tcp))))<br>
        (connect-socket s "localhost" "12345" (address-family inet) (socket-domain stream)<br>
                0 (ip-protocol tcp))<br>
        (loop s)))<br>
<br>
(cond<br>
    ((member "client" (command-line)) (client))<br>
    ((member "server" (command-line)) (server))<br>
    (else (display "error: expected client or server on the command line") (newline)))<br>
</code></pre>
