import sys
import re

# args.tmp should contain untouched CLI arguments
args = None
with open('args.tmp', 'r') as f:
    args = f.read()

if args:
    with open('args.tmp', 'w') as f:
        # Filter and keep anything that starts with a C:\ path, discard any optional quotes and write back to file
        match = re.search(r'\"?(c\:\\.*?)\"?$', args, re.IGNORECASE)
        if match and match[1]:
            f.write(match[1])
            exit(0)

# Nothing to be done
exit(1)

