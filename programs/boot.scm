(begin
  (define (u8 off) (disk-read-byte off))
  (define (u32 off)
    (+ (u8 off)
       (* 256 (u8 (+ off 1)))
       (* 65536 (u8 (+ off 2)))
       (* 16777216 (u8 (+ off 3)))))
  (define (cadr x) (car (cdr x)))
  (define (list a b) (cons a (cons b '())))

  (define fs-offset (u32 4))
  (define sb fs-offset)
  (define dir-off (+ fs-offset (u32 (+ sb 12))))
  (define dir-len (u32 (+ sb 16)))
  (define dir-limit (+ dir-off dir-len))

  (define allowed
    (cons (cons 'display display)
      (cons (cons 'newline newline)
        (cons (cons '+ +)
          (cons (cons '- -)
            (cons (cons '* *)
              (cons (cons '< <)
                (cons (cons '= =)
                  (cons (cons 'cons cons)
                    (cons (cons 'car car)
                      (cons (cons 'cdr cdr)
                        (cons (cons 'null? null?)
                          (cons (cons 'pair? pair?)
                            (cons (cons 'eq? eq?)
                              (cons (cons 'string-length string-length)
                                (cons (cons 'string-ref string-ref)
                                  (cons (cons 'string=? string=?)
                                    (cons (cons 'eval-string eval-string)
                                      '()))))))))))))))))))

  (define (find-file-loop name off)
    (if (< off dir-limit)
        (if (string=? (disk-read-cstring off 64) name)
            (list (+ fs-offset (u32 (+ off 64))) (u32 (+ off 68)))
            (find-file-loop name (+ off 76)))
        #f))

  (define (find-file name) (find-file-loop name dir-off))

  (define (load name)
    (begin
      (define info (find-file name))
      (if info
          (eval-scoped allowed (disk-read-bytes (car info) (cadr info)))
          (begin (display "missing file: ") (display name) (newline)))))

  (load "init.scm"))
