#!/usr/bin/python3
import os

with open('function_name.txt') as f:
    function_names = f.readlines()
    os.system('nm -a dmtcp1 > nm_output')
    with open('nm_output') as nm:
        symbol_names = nm.readlines()
        with open('function_symbols', 'w') as output_file:
            for names in function_names:
               for line in symbol_names:
                  parsed_line = line.split(' ')
                  if names[:-1] in parsed_line[-1]:
                     #print(parsed_line[-1], names[:-1])
                     output_file.write(parsed_line[-1] + '\n')
