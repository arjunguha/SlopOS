(define (read-number)
  (define s (read-string))
  (define (digit? ch)
    (define v (char->int ch))
    (if (< v (char->int #\0))
        #f
        (if (< (char->int #\9) v)
            #f
            #t)))
  (define (digit->int ch) (- (char->int ch) (char->int #\0)))
  (define (loop i acc)
    (if (< i (string-length s))
        (begin
          (define ch (string-ref s i))
          (if (digit? ch)
              (loop (+ i 1) (+ (* acc 10) (digit->int ch)))
              acc))
        acc))
  (if (not s)
      0
      (loop 0 0)))

(define (factorial n)
  (if (< n 2)
      1
      (* n (factorial (- n 1)))))

(display "enter n: ") (newline)
(define n (read-number))
(display "factorial: ")
(display (number->string (factorial n)))
(newline)
