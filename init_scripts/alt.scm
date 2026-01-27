(begin
  (display "alt init running")
  (newline)
  (define (countdown n)
    (if (< n 0)
        0
        (begin (display n) (newline) (countdown (- n 1)))))
  (countdown 3))
