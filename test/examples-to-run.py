#! /usr/bin/env python3

# A list of C++ examples to run in order to ensure that they remain
# buildable and runnable over time.  Each tuple in the list contains
#
#     (example_name, do_run, do_valgrind_run).
#
# See test.py for more information.
cpp_examples = [
    ("telep-lin-example", "True", "False"),
    ("telep-lin-adapt-example --nNodes=4", "True", "False"),
    ("ent-swap-example", "True", "False"),
    ("ent-swap-adapt-example --nNodes=4", "True", "False"),
    ("ent-swap-adapt-local-example --nNodes=16", "True", "False"),
    ("distill-nested-example", "True", "False"),
    ("distill-nested-adapt-example --nPairs=4", "True", "False"),
]

# A list of Python examples to run in order to ensure that they remain
# runnable over time.  Each tuple in the list contains
#
#     (example_name, do_run).
#
# See test.py for more information.
python_examples = []
