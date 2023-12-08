enum SlackModes = {SLACK_SUM, SLACK_MAX, SLACK_NONZERO, SLACK_DISABLE};
opt SlackModes: SlackMode;
SlackModes: selectedSlackMode = if occurs(SlackMode) then deopt(SlackMode) else SLACK_SUM endif;

% Construct new variable and connect slacks
function var int: slack_link(int: par_o, var int: par_down, var int: par_up, int: down, int: up) = let {
  var par_o-down..par_o+up: new_var = par_o + par_up - par_down;
  constraint par_up >= 0 /\ par_up <= up
          /\ par_down >= 0 /\ par_down <= down
          /\ (par_down = 0 \/ par_up = 0);
} in new_var;
   
% Construct new array of variables and connect slacks
function array[$$E] of var int: slack_link(array[$$E] of int: par_arr_o,
                                           array[$$E] of var int: par_arr_down,
                                           array[$$E] of var int: par_arr_up,
                                           int: down, int: up) = 
    array1d(index_set(par_arr_o),
            [slack_link(par_arr_o[i], par_arr_down[i], par_arr_up[i], down, up)
             | i in index_set(par_arr_o)]);

% Construct new array of variables and connect slacks
function array[$$E, $$E] of var int: slack_link(array[$$E, $$E] of int: par_arr_o,
                                                array[$$E, $$E] of var int: par_arr_down,
                                                array[$$E, $$E] of var int: par_arr_up,
                                                int: down, int: up) = 
    array2d(index_set_1of2(par_arr_o), index_set_2of2(par_arr_o),
            [slack_link(par_arr_o[i,j], par_arr_down[i,j], par_arr_up[i,j], down, up) 
             | i in index_set_1of2(par_arr_o), j in index_set_2of2(par_arr_o)]);

predicate slack_domain(var int: x, var set of int: d) = x in d;
predicate slack_domain(array[$$E] of var int: xs, var set of int: ds) =
  forall(i in index_set(xs)) (xs[i] in ds);

opt int: old_objective_ub;
opt int: old_objective_lb;
opt int: old_objective_eq;
var int: old_objective;
constraint if occurs(old_objective_ub) then old_objective <= old_objective_ub endif;
constraint if occurs(old_objective_lb) then old_objective >= old_objective_lb endif;
constraint if occurs(old_objective_eq) then old_objective = old_objective_eq endif;

opt int: slack_objective_ub;
opt int: slack_objective_lb;
opt int: slack_objective_eq;
var int: slack_objective;
constraint if occurs(slack_objective_ub) then slack_objective <= slack_objective_ub endif;
constraint if occurs(slack_objective_lb) then slack_objective >= slack_objective_lb endif;
constraint if occurs(slack_objective_eq) then slack_objective = slack_objective_eq endif;

opt int: slack_nonzero_count_eq;
var int: slack_nonzero_count;
constraint if occurs(slack_nonzero_count_eq) then 
     slack_nonzero_count = sum(i in index_set(all_slacks)) (all_slacks[i] > 0)
  /\ slack_nonzero_count = slack_nonzero_count_eq
  else slack_nonzero_count = 0
endif;

array[int] of var int: all_slacks;
predicate slack_configure_objective() =
  if selectedSlackMode = SLACK_SUM 
    then slack_objective = sum(all_slacks)
  elseif selectedSlackMode = SLACK_MAX
    then slack_objective = sum(s in all_slacks) (ub(s)) * max(all_slacks) + sum(all_slacks)
  elseif selectedSlackMode = SLACK_NONZERO
    then slack_objective = sum(i in index_set(all_slacks)) (all_slacks[i] > 0)
  else slack_objective = 0 /\ forall(i in index_set(all_slacks)) (all_slacks[i] = 0)
  endif;
