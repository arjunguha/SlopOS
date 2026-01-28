(define files (list-files))
(define (print-lines xs)
  (if (null? xs)
      0
      (begin (display (car xs)) (newline) (print-lines (cdr xs)))))
(print-lines files)
