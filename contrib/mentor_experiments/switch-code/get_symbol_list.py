import os
import sys
if len(sys.argv) < 3:
	print("usage: python ./prog base-executable debug-library")
	sys.exit(1)

base_executable = sys.argv[1]
debug_library = sys.argv[2]
forbidden_list = ['B', 'b', 'D', 'd', 'R']
output_file = "symbol_to_redirect.txt"
os.system(' nm --defined-only --dynamic ' + base_executable +
          ' > symbol_information')

with open('symbol_information', 'r') as si:
	lines = si.readlines()
	with open(output_file, 'w') as symbol_list:
		for line in lines:
			parsed_line = line.split(' ')
			if parsed_line[1] not in forbidden_list:
				symbol_list.write(parsed_line[-1])
		symbol_list.close()
	si.close()
os.environ["SYMBOL_LIST"]= output_file
