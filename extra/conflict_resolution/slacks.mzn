annotation slack_par(int: down, int: up);

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
