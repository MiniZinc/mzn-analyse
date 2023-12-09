include "slacks.mzn";

int: n;
int: l;
int: u;

array[1..n] of int: al :: slack_par(10, 0) = [l | i in 1..n];
array[1..n] of int: au :: slack_par(0, 10) = [u | i in 1..n];

array[1..n] of var int: x;
constraint forall(i in 1..n) (x[i] >= al[i] /\ x[i] <= au[i]);

include "all_different.mzn";
constraint all_different(x);
