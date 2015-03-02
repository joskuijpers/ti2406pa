grep ^XX log0 > log0.xx
grep ^XX log1 > log1.xx
sort -n -k2,3 log0.xx log1.xx > log2.xx
