(define (eof? ch) (char=? ch (int->char 4)))
(define space (int->char 32))
(define newline-ch (int->char 10))

(define (append a b)
  (if (null? a)
      b
      (cons (car a) (append (cdr a) b))))

(define (string->revlist s)
  (define (loop i acc)
    (if (< i (string-length s))
        (loop (+ i 1) (cons (string-ref s i) acc))
        acc))
  (loop 0 '()))

(define (string-trim-left s)
  (define (loop i)
    (if (< i (string-length s))
        (if (char=? (string-ref s i) space)
            (loop (+ i 1))
            i)
        i))
  (loop 0))

(define (string-trim-right s)
  (define (loop i)
    (if (< i 0)
        -1
        (if (char=? (string-ref s i) space)
            (loop (- i 1))
            i)))
  (loop (- (string-length s) 1)))

(define (string-trim s)
  (define start (string-trim-left s))
  (define end (string-trim-right s))
  (if (< end start)
      ""
      (begin
        (define (slice i acc)
          (if (> i end)
              (list->string (reverse-list acc))
              (slice (+ i 1) (cons (string-ref s i) acc))))
        (slice start '()))))

(define (split-first s)
  (define (loop i)
    (if (< i (string-length s))
        (if (char=? (string-ref s i) space)
            i
            (loop (+ i 1)))
        i))
  (define sep (loop 0))
  (if (= sep (string-length s))
      (cons s (cons "" '()))
      (begin
        (define (take i acc)
          (if (= i sep)
              (list->string (reverse-list acc))
              (take (+ i 1) (cons (string-ref s i) acc))))
        (define (drop i acc)
          (if (= i (string-length s))
              (list->string (reverse-list acc))
              (drop (+ i 1) (cons (string-ref s i) acc))))
        (define head (take 0 '()))
        (define tail (string-trim (drop (+ sep 1) '())))
        (cons head (cons tail '())))))

(define (print-lines xs)
  (if (null? xs)
      0
      (begin (display (car xs)) (newline) (print-lines (cdr xs)))))

(define (cmd-ls)
  (print-lines (list-files)))

(define (cmd-cat name)
  (define contents (read-text-file name))
  (if contents
      (begin (display contents) (newline))
      (begin (display "missing file") (newline))))

(define (cmd-exec name)
  (define contents (read-text-file name))
  (if contents
      (eval-string contents)
      (begin (display "missing file") (newline))))

(define (create-loop name acc)
  (define line (readline))
  (if (not line)
      (begin
        (create-file name (list->string (reverse-list acc)))
        (display "ok")
        (newline))
      (if (string=? (string-trim line) "EOF")
          (begin
            (create-file name (list->string (reverse-list acc)))
            (display "ok")
            (newline))
          (create-loop name (append (cons newline-ch (string->revlist line)) acc)))))

(define (cmd-create name)
  (create-loop name '()))

(define (cmd-help)
  (display "commands:") (newline)
  (display "  ls") (newline)
  (display "  cat <file>") (newline)
  (display "  exec <file>") (newline)
  (display "  create <file>  (end input with EOF on its own line)") (newline)
  (display "  help") (newline)
  (display "  exit") (newline))

(define (readline) (read-string))

(define (dispatch cmd arg)
  (if (string=? cmd "ls")
      (begin (cmd-ls) #t)
      (if (string=? cmd "cat")
          (begin (cmd-cat arg) #t)
          (if (string=? cmd "exec")
              (begin (cmd-exec arg) #t)
              (if (string=? cmd "create")
                  (begin (cmd-create arg) #t)
                  (if (string=? cmd "help")
                      (begin (cmd-help) #t)
                      (if (string=? cmd "exit")
                          #f
                          (begin (display "unknown command") (newline) #t))))))))

(define (repl)
  (display "> ")
  (define line (readline))
  (if (not line)
      0
      (begin
        (define trimmed (string-trim line))
        (if (string=? trimmed "")
            (repl)
            (begin
              (define parts (split-first trimmed))
              (if (dispatch (car parts) (car (cdr parts)))
                  (repl)
                  0))))))

(repl)
