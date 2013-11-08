;;;
;;; Tests from eccentric.txt by JRM.
;;;

(define-syntax nth-value
  (syntax-rules ()
    ((nth-value n values-producing-form)
     (call-with-values
       (lambda () values-producing-form)
       (lambda all-values
         (list-ref all-values n))))))

(check-equal 5 (nth-value 4 (values 1 2 3 4 5 6 7 8)))

(define-syntax please
  (syntax-rules ()
    ((please . forms) forms)))

(check-equal 20 (please + 9 11))

(define-syntax please
  (syntax-rules ()
    ((please function . arguments) (function . arguments))))

(check-equal 20 (please + 9 11))

(define-syntax prohibit-one-arg
  (syntax-rules ()
    ((prohibit-one-arg function argument)
     (syntax-error
      "Prohibit-one-arg cannot be used with one argument."
      function argument))
    ((prohibit-one-arg function . arguments)
     (function . arguments))))

(check-syntax (syntax-violation syntax-error) (prohibit-one-arg display 3))
(check-equal 5 (prohibit-one-arg + 2 3))

(define-syntax bind-variables
  (syntax-rules ()
    ((bind-variables () form . forms)
     (begin form . forms))

    ((bind-variables ((variable value0 value1 . more) . more-bindings) form . forms)
     (syntax-error "bind-variables illegal binding" (variable value0 value1 . more)))

    ((bind-variables ((variable value) . more-bindings) form . forms)
     (let ((variable value)) (bind-variables more-bindings form . forms)))

    ((bind-variables ((variable) . more-bindings) form . forms)
     (let ((variable #f)) (bind-variables more-bindings form . forms)))

    ((bind-variables (variable . more-bindings) form . forms)
     (let ((variable #f)) (bind-variables more-bindings form . forms)))

    ((bind-variables bindings form . forms)
     (syntax-error "Bindings must be a list." bindings))))

(check-equal (1 #f #f 4)
    (bind-variables ((a 1)        ;; a will be 1
                     (b)          ;; b will be #F
                     c            ;; so will c
                     (d (+ a 3))) ;; a is visible in this scope.
        (list a b c d)))

(define-syntax bind-variables1
  (syntax-rules ()
    ((bind-variables1 () form . forms)
     (begin form . forms))

    ((bind-variables1 ((variable value0 value1 . more) . more-bindings) form . forms)
     (syntax-error "bind-variables illegal binding" (variable value0 value1 . more)))

    ((bind-variables1 ((variable value) . more-bindings) form . forms)
     (bind-variables1 more-bindings (let ((variable value)) form . forms)))

    ((bind-variables1 ((variable) . more-bindings) form . forms)
     (bind-variables1 more-bindings (let ((variable #f)) form . forms)))

    ((bind-variables1 (variable . more-bindings) form . forms)
     (bind-variables1 more-bindings (let ((variable #f)) form . forms)))

    ((bind-variables1 bindings form . forms)
     (syntax-error "Bindings must be a list." bindings))))

(check-equal (1 #f #f 4)
    (bind-variables1 ((d (+ a 3)) ;; a is visible in this scope.
                      c            ;; c will be bound to #f
                      (b)          ;; so will b
                      (a 1))       ;; a will be 1
        (list a b c d)))

(define-syntax multiple-value-set!
  (syntax-rules ()
    ((multiple-value-set! variables values-form)

     (gen-temps
         variables ;; provided for GEN-TEMPS
         ()  ;; initial value of temps
         variables ;; provided for GEN-SETS
         values-form))))

(define-syntax emit-cwv-form
  (syntax-rules ()

    ((emit-cwv-form temps assignments values-form)
     (call-with-values (lambda () values-form)
       (lambda temps . assignments)))))

(define-syntax gen-temps
  (syntax-rules ()

    ((gen-temps () temps vars-for-gen-set values-form)
     (gen-sets temps
               temps ;; another copy for gen-sets
               vars-for-gen-set
               () ;; initial set of assignments
               values-form))

    ((gen-temps (variable . more) temps vars-for-gen-set values-form)
     (gen-temps
       more
       (temp . temps)
       vars-for-gen-set
       values-form))))

(define-syntax gen-sets
  (syntax-rules ()

    ((gen-sets temps () () assignments values-form)
     (emit-cwv-form temps assignments values-form))

    ((gen-sets temps (temp . more-temps) (variable . more-vars) assignments values-form)
     (gen-sets
       temps
       more-temps
       more-vars
       ((set! variable temp) . assignments)
       values-form))))

(check-equal (1 2 3)
    (let ((a 0) (b 0) (c 0)) (multiple-value-set! (a b c) (values 1 2 3)) (list a b c)))

(define-syntax multiple-value-set!
  (syntax-rules ()
    ((multiple-value-set! variables values-form)

     (gen-temps-and-sets
         variables
         ()  ;; initial value of temps
         ()  ;; initial value of assignments
         values-form))))

(define-syntax gen-temps-and-sets
  (syntax-rules ()

    ((gen-temps-and-sets () temps assignments values-form)
     (emit-cwv-form temps assignments values-form))

    ((gen-temps-and-sets (variable . more) (temps ...) (assignments ...) values-form)
     (gen-temps-and-sets
        more
       (temps ... temp)
       (assignments ... (set! variable temp))
       values-form))))

(define-syntax emit-cwv-form
  (syntax-rules ()

    ((emit-cwv-form temps assignments values-form)
     (call-with-values (lambda () values-form)
       (lambda temps . assignments)))))

(check-equal (1 2 3)
    (let ((a 0) (b 0) (c 0)) (multiple-value-set! (a b c) (values 1 2 3)) (list a b c)))

(define-syntax multiple-value-set!
  (syntax-rules ()
    ((multiple-value-set! variables values-form)
     (mvs-aux "entry" variables values-form))))

(define-syntax mvs-aux
  (syntax-rules ()
    ((mvs-aux "entry" variables values-form)
     (mvs-aux "gen-code" variables () () values-form))

    ((mvs-aux "gen-code" () temps sets values-form)
     (mvs-aux "emit" temps sets values-form))

    ((mvs-aux "gen-code" (var . vars) (temps ...) (sets ...) values-form)
     (mvs-aux "gen-code" vars
                         (temps ... temp)
                         (sets ... (set! var temp))
                         values-form))

    ((mvs-aux "emit" temps sets values-form)
     (call-with-values (lambda () values-form)
       (lambda temps . sets)))))

(check-equal (1 2 3)
    (let ((a 0) (b 0) (c 0)) (multiple-value-set! (a b c) (values 1 2 3)) (list a b c)))

(define d 0)
(define e 0)
(define f 0)
(check-equal (1 2 3) (begin (multiple-value-set! (d e f) (values 1 2 3)) (list d e f)))

(define-syntax sreverse
   (syntax-rules (halt)
     ((sreverse thing) (sreverse "top" thing ("done")))

     ((sreverse "top" () (tag . more))
      (sreverse tag () . more))

     ((sreverse "top" ((headcar . headcdr) . tail) kont)
      (sreverse "top" tail ("after-tail" (headcar . headcdr) kont)))

     ((sreverse "after-tail" new-tail head kont)
      (sreverse "top" head ("after-head" new-tail kont)))

     ((sreverse "after-head" () new-tail (tag . more))
      (sreverse tag new-tail . more))

     ((sreverse "after-head" new-head (new-tail ...) (tag . more))
      (sreverse tag (new-tail ... new-head) . more))

     ((sreverse "top" (halt . tail) kont)
      '(sreverse "top" (halt . tail) kont))

     ((sreverse "top" (head . tail) kont)
      (sreverse "top" tail ("after-tail2" head kont)))

     ((sreverse "after-tail2" () head (tag . more))
      (sreverse tag (head) . more))

     ((sreverse "after-tail2" (new-tail ...) head (tag . more))
      (sreverse tag (new-tail ... head) . more))

     ((sreverse "done" value)
      'value)))

(check-equal
  (sreverse "top" (halt)
    ("after-head" ()
      ("after-tail2" 4
        ("after-head" (7 6)
          ("after-tail" (2 3)
            ("after-tail2" 1
              ("done")))))))
    (sreverse (1 (2 3) (4 (halt)) 6 7)))
