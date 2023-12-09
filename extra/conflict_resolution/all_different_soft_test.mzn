include "all_different.mzn";

predicate all_different_soft(array[int] of var int: x, var int: violation) =
  if lb(violation) == 0 /\ ub(violation) == 0 then all_different(x)
  else violation = sum(i, j in index_set(x) where i<j) (x[i] = x[j]) endif;

int: n;
int: v;

array[1..n] of var 1..v: x;

include "slacks.mzn";
int: adv :: slack_par(0, ceil((n * (n-1))/2)) = 0;
constraint all_different_soft(x, adv);
