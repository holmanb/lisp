; Q-Expressions
(assert (list 1 2 3 4) {1 2 3 4})
(assert (eval {head (list 1 2 3 4)}) {1})
(assert (tail {tail tail tail}) ({tail tail}))
(assert (head {head head head}) {head})
(assert (eval (head {(+ 1 2) (+ 10 20)})) 3)
; Charbufs
(assert (join "header " "body " "tail") "header body tail")
; Function
(assert ((\ {x y} {+ x y}) 1 10) 11)
(put {x} 1)
; Error
(print "")
(assert_err (\ 1) "Function '\\' passed too few")
(assert_err (error "test") "test")
(assert_err (\ 1 1 1) "Function '\\' passed too many")
(assert_err (\ 1 1) "Function '\\' passed incorrect type")
(assert_err (== 1) "Function '==' passed too few")
(assert_err (join "" {}) "Function 'join' passed multiple")
(assert_err (assert_err (error "test") "testing") "assert failed \"test\" does not contain \"testing\"")
(assert_err (assert 1 2) "assert failed [1] != [2]")
; Type
(assert (type (\ {x y} {+ x y})) "Function")
(assert (type {head (list 1 2 3 4)}) "Q-expression")
(assert (type ()) "S-expression")
(assert (type "") "Charbuf")
(assert (type 1) "Number")
(assert (type error) "Builtin")
(assert (type (error "mayday")) "Error")
(assert (type (type "mayday")) "Charbuf")
;(assert_type "Charbuf" "sing" "song")
(map type {"sing" "song"})
; Map
(map == {
	(== (type ()) "S-expression")
	(== (type "") "Charbuf")
	(== (type 1) "Number")
	(== (type error) "Builtin")
	(== (type (error "mayday")) "Error")
	(== (type (type "mayday")) "Charbuf")
	}
)

; Curry, yum
assert (curry == (map type {"a" "b" "c"}))
assert (curry == (map type {"a" "b" 1}))
