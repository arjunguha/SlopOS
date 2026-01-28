(begin
  ; boot.scm runs as the first Scheme program.
  ; It provides filesystem helpers and then loads init.scm.
  (define (u8 off) (disk-read-byte off))
  (define (u32 off)
    (+ (u8 off)
       (* 256 (u8 (+ off 1)))
       (* 65536 (u8 (+ off 2)))
       (* 16777216 (u8 (+ off 3)))))
  (define (cadr x) (car (cdr x)))
  (define (list a b) (cons a (cons b '())))

  ; Ramdisk layout (created by scripts/mkfs.py):
  ; 0x0: boot.scm length (u32 LE)
  ; 0x4: filesystem offset (u32 LE)
  ; 0x8: boot.scm bytes
  (define fs-offset (u32 4))
  (define sb fs-offset)
  ; Superblock fields are relative to fs-offset.
  (define dir-off (+ fs-offset (u32 (+ sb 12))))
  (define dir-len (u32 (+ sb 16)))
  (define dir-limit (+ dir-off dir-len))

  ; Find a file by name in the flat directory table.
  (define (find-file-loop name off)
    (if (< off dir-limit)
        (if (string=? (disk-read-cstring off 64) name)
            (list (+ fs-offset (u32 (+ off 64))) (u32 (+ off 68)))
            (find-file-loop name (+ off 76)))
        #f))

  (define (find-file name) (find-file-loop name dir-off))

  ; Load an entire file as a string; return #f if missing.
  (define (read-text-file name)
    (begin
      (define info (find-file name))
      (if info
          (disk-read-bytes (car info) (cadr info))
          #f)))

  ; Reverse a list (used by read-string).
  (define (reverse-list xs)
    (define (rev xs acc)
      (if (null? xs)
          acc
          (rev (cdr xs) (cons (car xs) acc))))
    (rev xs '()))

  ; Read a line from serial input and return a string.
  (define (read-string)
    (define (loop acc)
      (define ch (read-char))
      (if (char=? ch #\newline)
          (list->string (reverse-list acc))
          (if (char=? ch #\return)
              (list->string (reverse-list acc))
              (loop (cons ch acc)))))
    (loop '()))

  (define (bind name value rest) (cons (cons name value) rest))

  ; Allowed bindings for init scripts; eval-scoped restricts the environment.
  (define allowed
    (bind 'display display
      (bind 'newline newline
        (bind '+ +
          (bind '- -
            (bind '* *
              (bind '< <
                (bind '= =
                  (bind 'cons cons
                    (bind 'car car
                      (bind 'cdr cdr
                        (bind 'null? null?
                          (bind 'pair? pair?
                            (bind 'eq? eq?
                              (bind 'string-length string-length
                                (bind 'string-ref string-ref
                                  (bind 'string=? string=?
                                    (bind 'char=? char=?
                                      (bind 'list-alloc list-alloc
                                        (bind 'list->string list->string
                                          (bind 'read-char read-char
                                            (bind 'read-string read-string
                                              (bind 'eval-string eval-string
                                                (bind 'read-text-file read-text-file
                                                  '()))))))))))))))))))))))))

  ; Eval a Scheme file by name with the restricted environment.
  (define (load name)
    (begin
      (define info (find-file name))
      (if info
          (eval-scoped allowed (disk-read-bytes (car info) (cadr info)))
          (begin (display "missing file: ") (display name) (newline)))))

  ; Hand off to init.scm.
  (load "init.scm"))
