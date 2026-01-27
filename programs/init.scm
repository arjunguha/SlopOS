(begin
  (foreign-call 'spawn 0)
  (foreign-call 'spawn 1)
  (define (fact n) (if (< n 2) 1 (* n (fact (- n 1)))))
  (display (fact 5))
  (newline))
