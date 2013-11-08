;;;
;;; R7RS
;;;
;;; These tests need eval. They will not work as a program; load or runtests.scm needs to be used.
;;;

;; define-library

(check-equal 100 (begin (import (lib a b c)) lib-a-b-c))
(check-equal 10 (begin (import (lib t1)) lib-t1-a))
(check-error (assertion-violation) lib-t1-b)
(check-equal 20 b-lib-t1)
(check-error (assertion-violation) lib-t1-c)

(check-equal 10 (begin (import (lib t2)) (lib-t2-a)))
(check-equal 20 (lib-t2-b))
(check-syntax (syntax-violation) (import (lib t3)))
(check-syntax (syntax-violation) (import (lib t4)))

(check-equal 1000 (begin (import (lib t5)) (lib-t5-b)))
(check-equal 1000 lib-t5-a)

(check-equal 1000 (begin (import (lib t6)) (lib-t6-b)))
(check-equal 1000 (lib-t6-a))

(check-equal 1000 (begin (import (lib t7)) (lib-t7-b)))
(check-equal 1000 lib-t7-a)

(check-equal 1 (begin (import (only (lib t8) lib-t8-a lib-t8-c)) lib-t8-a))
(check-error (assertion-violation) lib-t8-b)
(check-equal 3 lib-t8-c)
(check-error (assertion-violation) lib-t8-d)

(check-equal 1 (begin (import (except (lib t9) lib-t9-b lib-t9-d)) lib-t9-a))
(check-error (assertion-violation) lib-t9-b)
(check-equal 3 lib-t9-c)
(check-error (assertion-violation) lib-t9-d)

(check-equal 1 (begin (import (prefix (lib t10) x)) xlib-t10-a))
(check-error (assertion-violation) lib-t10-b)
(check-equal 3 xlib-t10-c)
(check-error (assertion-violation) lib-t10-d)

(check-equal 1 (begin (import (rename (lib t11) (lib-t11-b b-lib-t11) (lib-t11-d d-lib-t11)))
    lib-t11-a))
(check-error (assertion-violation) lib-t11-b)
(check-equal 2 b-lib-t11)
(check-equal 3 lib-t11-c)
(check-error (assertion-violation) lib-t11-d)
(check-equal 4 d-lib-t11)

(check-syntax (syntax-violation) (import bad "bad library" name))
(check-syntax (syntax-violation)
    (define-library (no ("good") "library") (import (scheme base)) (export +)))

(check-syntax (syntax-violation) (import (lib t12)))
(check-syntax (syntax-violation) (import (lib t13)))

(check-equal 10 (begin (import (lib t14)) (lib-t14-a 10 20)))
(check-equal 10 (lib-t14-b 10 20))

(check-equal 10 (begin (import (lib t15)) (lib-t15-a 10 20)))
(check-equal 10 (lib-t15-b 10 20))

;; include

(check-error (assertion-violation) (let () (include "include3.scm") include-c))

;; include-ci

(check-error (assertion-violation) (let () (include-ci "include4.scm") INCLUDE-E))
