annotation slack_par(set of int: range);
annotation slack_par_ratio(set of float: range);


enum SlackModes = {SLACK_SUM, SLACK_MAX, SLACK_NONZERO, SLACK_DISABLE};
opt SlackModes: SlackMode;
SlackModes: selectedSlackMode = if occurs(SlackMode) then deopt(SlackMode) else SLACK_SUM endif;

function var int: slack_opt_mode(array[int] of var int: slacks) =
  if selectedSlackMode = SLACK_SUM then sum(slacks)
  elseif selectedSlackMode = SLACK_MAX then max(slacks)
  elseif selectedSlackMode = SLACK_NONZERO then sum(i in index_set(slacks)) (slacks[i] > 0)
  else let {var 0..10: o; constraint forall(i in index_set(slacks)) (slacks[i] = 0)} in o
  endif;

% Construct new variable and connect slacks
function var int: slack_link(int: par_o, var int: par_up, var int: par_down, set of int: range) = let {
  var int: new_var = par_o + par_up - par_down;
  constraint par_up >= 0 /\ par_up <= max(range)
          /\ par_down >= 0 /\ par_down <= min(range)
          /\ (par_down = 0 \/ par_up = 0);
} in new_var;
   
% Construct new array of variables and connect slacks
function array[$$E] of var int: slack_link(array[$$E] of int: par_arr_o,
                                           array[$$E] of var int: par_arr_up,
                                           array[$$E] of var int: par_arr_down,
                                           set of int: range) = 
    array1d(index_set(par_arr_o),
            [slack_link(par_arr_o[i], par_arr_up[i], par_arr_down[i], range)
             | i in index_set(par_arr_o)]);

% Construct new array of variables and connect slacks
function array[$$E, $$E] of var int: slack_link(array[$$E, $$E] of int: par_arr_o,
                                                array[$$E, $$E] of var int: par_arr_up,
                                                array[$$E, $$E] of var int: par_arr_down,
                                                set of int: range) = 
    array2d(index_set_1of2(par_arr_o), index_set_2of2(par_arr_o),
            [slack_link(par_arr_o[i,j], par_arr_up[i,j], par_arr_down[i,j], range) 
             | i in index_set_1of2(par_arr_o), j in index_set_2of2(par_arr_o)]);
