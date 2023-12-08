include "globals.mzn";
include "slacks.mzn";
int: n_res;
set of int: Res = 1..n_res;
array [Res] of var int: rc_slack = slack_link(rc, rc_slack_down, rc_slack_up, 0, 100);
int: n_tasks;
set of int: Tasks = 1..n_tasks;
array [Tasks] of int: d;
array [Res, Tasks] of int: rr;
array [Tasks] of set of int: suc;
int: t_max = sum ( i in Tasks ) ( d[i] );
set of int: Times = 0..t_max-1;
array [Tasks] of var Times: s;
var 0..t_max: objective;
constraint forall ( i in Tasks, j in suc[i] ) ( s[i]+d[i]<=s[j] );
constraint forall ( i, j in Tasks where i < j ) ( if exists ( r in Res ) ( rr[r, i]+rr[r, j] > rc_slack[r] ) then s[i]+d[i]<=s[j] \/ s[j]+d[j]<=s[i] else true endif );
constraint forall ( r in Res ) ( 
    let {
        set of int: RTasks = { i | i in Tasks where rr[r, i] > 0 /\ d[i] > 0 }, 
        int: sum_rr = sum ( i in RTasks ) ( rr[r, i] ),
    } in (if RTasks!={} /\ sum_rr > rc_slack[r] then cumulative([ s[i] | i in RTasks ], [ d[i] | i in RTasks ], [ rr[r, i] | i in RTasks ], rc_slack[r]) else true endif) );
constraint forall ( i in Tasks where suc[i]=={} ) ( s[i]+d[i]<=objective );
solve :: int_search(s++[objective], smallest, indomain_min, complete) minimize slack_objective;
include "slacks_objective.mzn";
array [Res] of int: rc;
array [Res] of var 0..100: rc_slack_up;
array [Res] of var 0..0: rc_slack_down;
all_slacks = rc_slack_up++rc_slack_down;
constraint slack_configure_objective();
constraint old_objective==objective;
