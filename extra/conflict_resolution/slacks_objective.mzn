
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
