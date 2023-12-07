% Original model
include "slacks.mzn";

set of int: Res;
array[Res] of int: min_level;
array[Res] of 0..100: max_level;

array[Res] of var 0..100: max_level_slack_up;
array[Res] of var 0..100: max_level_slack_down;
array[Res] of var int: max_level_slack = slack_link(max_level, max_level_slack_up, max_level_slack_down, 0..100);

int: total_water;

array[Res] of var 0..: water_level;
constraint :: "All water must be in a reservoir" sum(water_level) = total_water;
constraint :: "Reservoirs have capacities and min-levels" 
forall(r in Res) ( (min_level[r] <= water_level[r]) :: "Lower \(r)"
                /\ (water_level[r] <= max_level_slack[r]) :: "Upper \(r)" );

array[int] of var int: slacks = max_level_slack_down ++ max_level_slack_up;
solve minimize slack_opt_mode(slacks);