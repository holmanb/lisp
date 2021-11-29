; Atoms
(def {nil} {})
(def {true} 1)
(def {false} 0)


; Function definitions
; makes for cleaner syntax
(def {fun} (\ {f b} {
	def (head f) (\ (tail f) b)
	 }))

; unpack
(fun {unpack f l} {
     eval (join (list f) l)
     })

; pack
(fun {pack f & xs} { f xs })

; curry / uncurry
(def {curry} unpack)
(def {uncurry} pack)

; do things in sequence
(fun {do & l} {
     if (== l nil)
	     {nil}
	     {last l}
})

; open new scope
(fun {let b} {
     (( \ {_} b ()))
})

; flip arg order for partially evaluated functions
(fun {flip f a b} {f b a})

; List Length
(fun {len l} {
     if (== l nil)
     	{0}
     	{+ 1 (len (tail l))}
})

; Nth item in List
(fun {nth n l} {
     if (== n 0)
	{eval (head l)}
	{nth (- n 1) tail l}
})

; Last item in list
(fun {last l} {nth (- (len l) 1) l})

; First item in list
(fun {first l} {nth 0 l})

; Take first N items
(fun {take n l} {
     if (== n 0)
	     {nil}
	     {join (head l) (take (- n 1) (tail l))}
})

; Drop first N items
(fun {drop n l} {
     if (== n 0)
	     {l}
	     {drop (- n 1) (tail l)}
})

; Split at N
(fun {split n l} {
     list (take n l) (drop n l)
})

; Apply function to list
(fun {map f l} {
     if (== l nil)
     	{nil}
	{join (list (f (first l))) (map f (tail l))}
})

; Apply Filter to list
(fun {filter f l} {
     if (== l nil)
     	{nil}
	{join (if (f (first l)) {head l} {nil}) (filter f (tail l))}
})

; Fold left
(fun {foldl f z l} {
     if (== l nil)
     	{z}
	{foldl f (f z (first l)) (tail l)}
})

; Sum
(fun {sum l} {foldl + 0 l})
(fun {product l} {foldl * 1 l})

; Select
(fun {select & cs} {
     if (== cs nil)
     	{error "No selection found!"}
	{if (first (first cs)) {nth 2 (first cs)} {unpack select (tail cs)}}
})

; Default Case
(def {otherwise} true)
