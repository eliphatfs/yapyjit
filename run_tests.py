# -*- coding: utf-8 -*-
"""
Created on Fri Mar 26 16:20:34 2021
@author: eliphat
"""
import sys
import unittest


# input("Press Enter to continue...")
import yapyjit
# yapyjit.set_recompile_debug_enabled(True)
# yapyjit.set_profiling_enabled(True)
print("Python", sys.version)
__unittest = True
try:
    unittest.main(module=None, argv=['unittest', 'discover', '-v'])
finally:
    pass
    # print(yapyjit.print_profiling_data())
