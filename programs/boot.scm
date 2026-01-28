(begin
  ; boot.scm runs as the first Scheme program.
  ; It loads fs.scm in a privileged environment, then runs init.scm.
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

  (define (find-file-loop name off)
    (if (< off dir-limit)
        (if (string=? (disk-read-cstring off 64) name)
            (list (+ fs-offset (u32 (+ off 64))) (u32 (+ off 68)))
            (find-file-loop name (+ off 76)))
        #f))

  (define (find-file name) (find-file-loop name dir-off))

  (define (read-file-bytes name)
    (begin
      (define info (find-file name))
      (if info
          (disk-read-bytes (car info) (cadr info))
          #f)))

  ; Load fs.scm with full privileges so it can define filesystem helpers.
  (define fs-code (read-file-bytes "fs.scm"))
  (if fs-code
      (eval-string fs-code)
      (begin (display "missing file: fs.scm") (newline)))

  ; Hand off to init.scm via the restricted loader from fs.scm.
  (load "init.scm"))
