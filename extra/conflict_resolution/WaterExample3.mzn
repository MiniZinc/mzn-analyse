% % Original model
% enum Res = {A, B, C};
% array[Res] of var int: water_level;
% array[Res] of int: min_level = [2, 2, 2];
% array[Res] of 0..100: max_level :: relax = [6, 3, 4];
% int: total_water = 30;
% constraint :: "All water must be in a reservoir" sum(water_level) = total_water;
% constraint :: "Reservoirs have capacities and min-levels" 
% forall(r in Res) ( (min_level[r] <= water_level[r]) :: "Lower \(r)"
%                 /\ (water_level[r] <= max_level[r]) :: "Upper \(r)" );
% solve minimize sum(water_level);


% % Modified model

enum Res = {A, B, C};

array[Res] of var int: water_level;
array[Res] of int: min_level = [2, 2, 2];

% array[Res] of 0..100: max_level :: relax;

% rename original parameter
array[Res] of int: max_level_o = [6, 3, 4];

% Introduce vars for slack up and down
array[Res] of var int: max_level_up;
array[Res] of var int: max_level_down;

% Replace parameter with variable and set up links
array[Res] of var int: max_level = 
  array1d(Res, slack_link(max_level_o, max_level_up, max_level_down, 0..100));

% Introduce array concatenating all slacks
array[int] of var int: slacks = max_level_up ++ max_level_down;
var int: delta = slack_delta_sum(slacks);
var int: maximum = slack_delta_max(slacks);
var int: counts = slack_count(slacks);

% Force all slacks to zero
predicate slack_disable(array[int] of var int: slacks) = (sum(slacks) = 0);

% Get distance each slack has moved
function var int: slack_delta_sum(array[int] of var int: slacks) =
  sum(slacks);
  
% Get largest slack distance
function var int: slack_delta_max(array[int] of var int: slacks) =
  max(slacks);
  
% Count active slacks
function var int: slack_count(array[int] of var int: slacks) =
  sum(i in index_set(slacks))(slacks[i] > 0);
  
% Construct new variable and connect slacks
function var int: slack_link(int: par_o, var int: par_up, var int: par_down,
                             set of int: range) = let {
  var int: new_var = par_o + par_up - par_down;
  constraint par_up >= 0 /\ par_up <= max(range)-par_o
          /\ par_down >= 0 /\ par_down <= par_o-min(range)
          /\ (par_down = 0 \/ par_up = 0);
} in new_var;
  
% Construct new array of variables and connect slacks
% function array[int] of var int: slack_link(array[int] of int: par_arr_o,
%                                            array[int] of var int: par_arr_up,
%                                            array[int] of var int: par_arr_down,
%                                            set of int: range) =
%   [slack_link(par_arr_o[i], par_arr_up[i], par_arr_down[i], range)
%    | i in index_set(par_arr_o)];
   
% Construct new array of variables and connect slacks
function array[int] of var int: slack_link(array[int] of int: par_arr_o,
                                           array[int] of var int: par_arr_up,
                                           array[int] of var int: par_arr_down,
                                           set of int: range) = let {
    array[int] of var int: par_arr = [par_arr_o[i] + par_arr_up[i] - par_arr_down[i] | i in index_set(par_arr_o)];
    constraint forall(i in index_set(par_arr_o)) (
         par_arr_up[i] >= 0 /\ par_arr_up[i] <= max(range)-par_arr_o[i]
      /\ par_arr_down[i] >= 0 /\ par_arr_down[i] <= par_arr_o[i]-min(range)
      /\ (par_arr_down[i] = 0 \/ par_arr_up[i] = 0)
    );
  } in par_arr;

int: total_water = 30;

constraint :: "All water must be in a reservoir"
  sum(water_level) = total_water;
  
constraint :: "Reservoirs have capacities and min-levels" 
forall(r in Res) (
     (min_level[r] <= water_level[r]) :: "Lower \(r)"
  /\ (water_level[r] <= max_level[r]) :: "Upper \(r)"
);

% Original Objective
constraint slack_disable(slacks);
solve minimize sum(water_level);
% UNSATISFIABLE

%% METHOD ONE

% % Minimize counts
% solve minimize counts;
% % = 1
% onstraint counts = 1;
% solve minimize delta;

% % Find all solutions with counts = 1 /\ delta = 17
% constraint counts = 1 /\ delta = 17;
% solve satisfy;

% % METHOD TWO
% % Minimize maximum
% solve minimize maximum;
% % = 6

% % Fix maximum, minimize delta
% constraint maximum = 6;
% solve minimize delta;
% % = 17

% % Find all solutions, balancing adjustments
% % Fix delta
% constraint maximum = 6 /\ delta = 17;
% solve satisfy;
% % = [6,6,5], [6,5,6], [5,6,6]

output [
  "water_level = \(water_level)\n",
  "max_level_up = \(max_level_up)\n",
  "delta = \(delta)\n",
  "maximum = \(maximum)\n",
  "counts = \(counts)\n"];