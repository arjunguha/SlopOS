(begin
  ; Filesystem helpers and restricted init loader.
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
  (define data-off (u32 (+ sb 20)))
  (define dir-limit (+ dir-off dir-len))

  ; Find a file by name in the flat directory table.
  (define (find-file-loop name off)
    (if (< off dir-limit)
        (if (string=? (disk-read-cstring off 64) name)
            (list (+ fs-offset (u32 (+ off 64))) (u32 (+ off 68)))
            (find-file-loop name (+ off 76)))
        #f))

  (define (find-file name) (find-file-loop name dir-off))

  ; List all filenames in the directory table.
  (define (list-files)
    (define (loop off acc)
      (if (< off dir-limit)
          (begin
            (define name (disk-read-cstring off 64))
            (if (string=? name "")
                (loop (+ off 76) acc)
                (loop (+ off 76) (cons name acc))))
          acc))
    (reverse-list (loop dir-off '())))

  ; Load an entire file as a string; return #f if missing.
  (define (read-text-file name)
    (begin
      (define info (find-file name))
      (if info
          (disk-read-bytes (car info) (cadr info))
          #f)))

  (define (string->list s)
    (define (loop i acc)
      (if (< i 0)
          acc
          (loop (- i 1) (cons (string-ref s i) acc))))
    (loop (- (string-length s) 1) '()))

  (define (append a b)
    (if (null? a)
        b
        (cons (car a) (append (cdr a) b))))

  (define (make-filled-string n ch)
    (define (loop i acc)
      (if (< i 1)
          (list->string acc)
          (loop (- i 1) (cons ch acc))))
    (loop n '()))

  (define (write-bytes off s)
    (disk-write-bytes off s))

  (define (write-u32 off v)
    (define (b n) (int->char (modulo n 256)))
    (write-bytes off (list->string (list (b v)
                                         (b (quotient v 256))
                                         (b (quotient v 65536))
                                         (b (quotient v 16777216))))))

  (define (write-dir-entry off name data-off len)
    (define name-list (string->list name))
    (define pad-len (- 64 (string-length name)))
    (define padded (append name-list (string->list (make-filled-string pad-len (int->char 0)))))
    (write-bytes off (list->string padded))
    (write-u32 (+ off 64) data-off)
    (write-u32 (+ off 68) len)
    (write-u32 (+ off 72) 0))

  (define (clear-dir-entry off)
    (write-bytes off (make-filled-string 76 (int->char 0))))

  (define (find-empty-entry off)
    (if (< off dir-limit)
        (if (string=? (disk-read-cstring off 1) "")
            off
            (find-empty-entry (+ off 76)))
        #f))

  (define (data-end off current)
    (if (< off dir-limit)
        (begin
          (define name (disk-read-cstring off 64))
          (if (string=? name "")
              (data-end (+ off 76) current)
              (begin
                (define data-off (u32 (+ off 64)))
                (define len (u32 (+ off 68)))
                (define end (+ data-off len))
                (data-end (+ off 76) (if (< current end) end current)))))
        current))

  (define (create-file name contents)
    (begin
      (define existing (find-file name))
      (if existing
          (delete-file name)
          0)
      (define entry (find-empty-entry dir-off))
      (if (not entry)
          (begin (display "no free dir slots") (newline) #f)
          (begin
            (define end (data-end dir-off data-off))
            (define len (string-length contents))
            (define abs-off (+ fs-offset end))
            (if (> (+ abs-off len) (disk-size))
                (begin (display "disk full") (newline) #f)
                (begin
                  (write-bytes abs-off contents)
                  (write-dir-entry entry name end len)
                  #t))))))

  (define (delete-file name)
    (begin
      (define info (find-file name))
      (if info
          (begin
            (define off (car info))
            (clear-dir-entry (- off 64))
            #t)
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
      (if (char=? ch (int->char 4))
          (if (null? acc) #f (list->string (reverse-list acc)))
          (if (char=? ch #\newline)
              (list->string (reverse-list acc))
              (if (char=? ch #\return)
                  (list->string (reverse-list acc))
                  (loop (cons ch acc))))))
    (loop '()))

  (define (bind name value rest) (cons (cons name value) rest))

  ; Allowed bindings for init scripts; eval-scoped restricts the environment.
  (define (not x) (eq? x #f))
  (define (> a b) (< b a))

  (define allowed '())
  (set! allowed (bind 'read-text-file read-text-file allowed))
  (set! allowed (bind 'eval-string eval-string allowed))
  (set! allowed (bind 'list-files list-files allowed))
  (set! allowed (bind 'delete-file delete-file allowed))
  (set! allowed (bind 'create-file create-file allowed))
  (set! allowed (bind 'disk-size disk-size allowed))
  (set! allowed (bind 'disk-write-bytes disk-write-bytes allowed))
  (set! allowed (bind 'yield yield allowed))
  (set! allowed (bind 'spawn-thread spawn-thread allowed))
  (set! allowed (bind 'read-string read-string allowed))
  (set! allowed (bind 'read-char read-char allowed))
  (set! allowed (bind 'number->string number->string allowed))
  (set! allowed (bind 'list->string list->string allowed))
  (set! allowed (bind 'list-alloc list-alloc allowed))
  (set! allowed (bind 'int->char int->char allowed))
  (set! allowed (bind 'char->int char->int allowed))
  (set! allowed (bind 'char=? char=? allowed))
  (set! allowed (bind 'string=? string=? allowed))
  (set! allowed (bind 'string-ref string-ref allowed))
  (set! allowed (bind 'string-length string-length allowed))
  (set! allowed (bind 'reverse-list reverse-list allowed))
  (set! allowed (bind 'not not allowed))
  (set! allowed (bind 'eq? eq? allowed))
  (set! allowed (bind 'pair? pair? allowed))
  (set! allowed (bind 'null? null? allowed))
  (set! allowed (bind 'cdr cdr allowed))
  (set! allowed (bind 'car car allowed))
  (set! allowed (bind 'cons cons allowed))
  (set! allowed (bind 'modulo modulo allowed))
  (set! allowed (bind 'quotient quotient allowed))
  (set! allowed (bind '= = allowed))
  (set! allowed (bind '> > allowed))
  (set! allowed (bind '< < allowed))
  (set! allowed (bind '* * allowed))
  (set! allowed (bind '- - allowed))
  (set! allowed (bind '+ + allowed))
  (set! allowed (bind 'newline newline allowed))
  (set! allowed (bind 'display display allowed))

  ; Eval a Scheme file by name with the restricted environment.
  (define (load name)
    (begin
      (define info (find-file name))
      (if info
          (eval-scoped allowed (disk-read-bytes (car info) (cadr info)))
          (begin (display "missing file: ") (display name) (newline)))))
)
