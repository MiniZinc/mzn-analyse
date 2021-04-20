# MznTool

## Usage
```
mzn_tool sequence input.mzn [passes]
```

where `[passes]` is the list of passes and arguments for those passes (`cmd:arg1,arg2,...` with no spaces unless quoted)

An implicit `out:-` command (or `out_fzn:-` if reading FlatZinc) is appended at the end of the pipeline if no `out` commands occur in the sequence.
An explicit final `out` must be added to the end if you wish to output models throughout the pipeline.

## Hardcoded Pipelines

There are three hardcoded pipelines that have their own argument parsing: `annotate`, `get_data`, `get_terms`.

Example:
```
mzn_tool get_terms in.mzn out.mzn out.terms
```
Is translated to:

```
mzn_tool sequence in.mzn get-term-types:out.terms remove-anns:data remove-items:solve,output out:out.mzn
```

## Examples


1. Remove the solve and output items from the model and write the model to solveless.mzn. It then inlines the local includes and outputs to stdout as "FlatZinc" (no linebreaks while printing an item).
```
mzn_tool sequence in.mzn remove-items:solve,output out:solveless.mzn inline-includes out_fzn
```

2. Remove all items except constraint items, picks out the 50th constraint, remove any annotations, then output to stdout.
The implicit output will default to `out_fzn` since the input was fzn.
```
mzn_tool sequence in.fzn filter-items:constraint get-items:50 remove-anns
```

3. The following requests the data-deps information for constraints 60, 61, and 62 from a FlatZinc file.
The `no_out` command disables the automatic insertion of `out_fzn`.

```
$ ./mzn_tool.exe sequence rcpsp-wet-r0.annotated.fzn filter-items:constraint get-items:60,61,62 get-data-deps no_out
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
