
int: n = 5;
int: v = 4;

array[1..n] of var 1..v: x;
array[1..n, 1..v] of var 0..1: x_b;
constraint forall(i in 1..n) (sum(j in 1..v) (x_b[i,j]) = 1);
constraint forall(i in 1..n) (sum(j in 1..v) (j * x_b[i,j]) = x[i]);

include "slacks.mzn";
array[1..v] of int: constraint_violation :: slack_par(0, 10) = [0 | i in 1..v];
constraint forall(j in 1..v) (sum(i in 1..n) (x_b[i,j]) - constraint_violation[j] = 1);

