int: N ;

array [1..2*N] of var 0..1: X ;

constraint forall(i in 1..N)(X[2*i-1] + X[2*i] >= 1) ;

solve minimize sum(X) ;


N = 8;
