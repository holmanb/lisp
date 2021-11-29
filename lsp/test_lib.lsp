; Depends: lib.lsp
(assert (filter (\ {x} {> x 2}) { 5 2 11 -7 8 1 })  {5 11 8})
(assert (map - {5 6 77 -1}) {-5 -6 -77 1})
(assert (sum {1 2 4 8 16}) 31)
(assert (product {2 4}) 8)


