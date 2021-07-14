# MznTool

## Usage
```
mzn_tool input.mzn [passes]
```

where `[passes]` is the list of passes and arguments for those passes (`cmd:arg1,arg2,...` with no spaces unless quoted)

An implicit `out:-` command (or `out_fzn:-` if reading FlatZinc) is appended at the end of the pipeline if no `out` commands occur in the sequence.
An explicit final `out` must be added to the end if you wish to output models throughout the pipeline.

## Help text:

```
 usage:
   mzn_tool in.mzn [passes...]

 passes:
   in:in.mzn
     Read input file (no support for stdout)
   out:out.mzn
     Write model to out.mzn (- for stdout)
   out_fzn:out.fzn
     Write model to out.fzn (- for stdout)
   no_out
     Disable automatic output insertion
   json_out:out.json
     Write collected json output to out.json (- for stdout)
   json_clear
     Clear collected json output
   no_json
     Disable automatic json output
   inline-includes
     Inline non-library includes
   inline-all-includes
     Inline all includes
   remove-anns:name1,[name2,...]
     Remove Id and Call annotations matching names
   remove-includes:name1,[name2,...]
     Remove includes matching names
   output-all
     Add 'add_to_output' annotation to all VarDecls
   remove-stdlib
     Remove stdlib includes
   get-items:idx1,[idx2,...]
     Narrow to items indexed by idx1,...
   filter-items:iid1,[iid2,...]
     Only keep items matching iids
   remove-items:iid1,[iid2,...]
     Remove items matching iids
   filter-typeinst:{var|par}
     Just show var/par parts of model
   replace-with-newvar:location1,location2
     Replace expressions with 'let' expressions

   annotate-data-deps
     Annotate expressions with their data dependencies
   get-term-types:out.terms
     Write .terms file with types of objective terms
   get-data-deps:out.cons (FlatZinc only)
     Write .cons file with data dependenceis of
     FlatZinc constraints
   get-exprs:location1,location2
     Extract list of expressions occurring inside location
     location = path.mzn|sl|sc|el|ec
   get-ast:location1,location2
     Build JSON representation of AST for whole model or just for
     expression matching location1 or location2, place in json_store
```

## Examples


1. Remove the solve and output items from the model and write the model to solveless.mzn. It then inlines the local includes and outputs to stdout as "FlatZinc" (no linebreaks while printing an item).
```
mzn_tool in.mzn remove-items:solve,output out:solveless.mzn inline-includes out_fzn
```

2. Remove all items except constraint items, picks out the 50th constraint, remove any annotations, then output to stdout.
The implicit output will default to `out_fzn` since the input was fzn.
```
mzn_tool in.fzn filter-items:constraint get-items:50 remove-anns
```

3. The following requests the data-deps information for constraints 60, 61, and 62 from a FlatZinc file.
The `no_out` command disables the automatic insertion of `out_fzn`.

```
$ ./mzn_tool.exe rcpsp-wet-r0.annotated.fzn filter-items:constraint get-items:60,61,62 get-data-deps no_out
{"constraint_info": [
  [
    ["in", "i", "Tasks"],
    ["in", "j", "suc[i]"],
    ["assign", "j", "24"],
    ["assign", "i", "2"],
    ["eq", "suc[i]", "23..24"],
    ["eq", "Tasks", "1..32"]],
  [
    ["in", "i", "Tasks"],
    ["in", "j", "suc[i]"],
    ["assign", "j", "5"],
    ["assign", "i", "3"],
    ["eq", "suc[i]", "{5,6,17}"],
    ["eq", "Tasks", "1..32"]],
  [
    ["in", "i", "Tasks"],
    ["in", "j", "suc[i]"],
    ["assign", "j", "6"],
    ["assign", "i", "3"],
    ["eq", "suc[i]", "{5,6,17}"],
    ["eq", "Tasks", "1..32"]]]}
```

