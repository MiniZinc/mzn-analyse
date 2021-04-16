
import json

with open("rcpsp-wet_orig.mzn.solveless.mzn",'rt') as f:
    model_txt = f.read()

term_types = json.load(open("rcpsp-wet_orig.mzn.terms.json"))['term_types']

def construct_term(term_types, tt, assigns):
    gens = ','.join(term_types[tt]['generators'])
    conds = '/\\'.join(assigns)
    coef = '*'.join(term_types[tt]['coefficients'])
    var = term_types[tt]['variable']
    return f"\n(sum({gens} where {conds}) ({coef}*{var}))"


literals = [(0, ["i=5"]),
            (0, ["i=2"]),
            (1, ["i=15"])]

obj_sum = '+'.join([construct_term(term_types, lit[0], lit[1])
                    for lit in literals])

print(model_txt);
print(f"solve minimize {obj_sum};")
