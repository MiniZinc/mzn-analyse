% Original model
include "slacks.mzn";

set of int: Res;
array[Res] of int: min_level;
array[Res] of 0..100: max_level :: slack_par(10, 10);
int: total_water :: slack_par(20, 0);

array[Res] of var 0..infinity: water_level;
constraint :: "All water must be in a reservoir"
  sum(water_level) = total_water;
constraint :: "Reservoirs have capacities and min-levels" 
  forall(r in Res) ( (min_level[r] <= water_level[r]) :: "Lower \(r)"
                /\ (water_level[r] <= max_level[r]) :: "Upper \(r)" );
solve minimize sum(water_level);

output ["water_level = \(water_level)\n"];
