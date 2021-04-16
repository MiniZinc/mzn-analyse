#!python
import sys
import json

def main(argv):
    cons_file = argv[1]
    cons = map(int, argv[2:])
    with open(cons_file, 'r') as f:
        data = json.load(f)
        con_info = data['constraint_info']
        for con_id in cons:
            print(f"Constraint: {con_id}")
            print(f"Details: {con_info[con_id]}")

if __name__ == '__main__':
    import sys
    main(sys.argv)
