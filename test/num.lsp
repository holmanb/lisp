; Equality
(assert 1 1)
(assert () ())
(assert {} {})
(assert "test" "test")

; Equality Ops
(assert (== 1000 1000) 1)
(assert (!= 1000 1001) 1)

; Order
(assert (> 1 0) 1)
(assert (> 0 -1) 1)
(assert (>= 57 56) 1)
(assert (>= 57 57) 1)
(assert (<= 57 57) 1)
(assert (<= 56 57) 1)

; Math
(assert (+ 1 1) 2)
(assert (+ 11 22 33) 66)
(assert (- 1 3) -2)
(assert (* 3 4) 12)
(assert (* 2 3 4) 24)
(assert (% 55 33) 22)
(assert (- 1) -1)
(assert (/ 33 3) 11)

; Logical
(assert (&& 1 1) 1)
(assert (&& 0 1) 0)
(assert (&& 1 0) 0)
(assert (&& 0 0) 0)
(assert (&& 1 1 1) 1)
(assert (&& 1 0 1) 0)
(assert (|| 0 0) 0)
(assert (|| 1 0) 1)
(assert (|| 0 1) 1)
(assert (|| 1 1) 1)
(assert (|| 1 1 1) 1)
(assert (|| 1 0 0) 1)
(assert (|| 0 0 1) 1)
(assert (|| 0 0 0) 0)
(assert (! 0) 1)
(assert (! 1) 0)

; Bitwise
(assert (& 15 9) 9)
(assert (| 5 10) 15)
(assert (>> 8 1) 4)
(assert (<< 512 3) 4096)

(assert (& 1 1) 1)
(assert (& 0 1) 0)
(assert (& 1 0) 0)
(assert (& 0 0) 0)

(assert (| 1 1) 1)
(assert (| 0 1) 1)
(assert (| 1 0) 1)
(assert (| 0 0) 0)

(assert (^ 1 1) 0)
(assert (^ 0 1) 1)
(assert (^ 1 0) 1)
(assert (^ 0 0) 0)

(assert (~ 1) -2)
(assert (~ 0) -1)
