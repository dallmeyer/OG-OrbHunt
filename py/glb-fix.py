from os import listdir
from os.path import isdir, isfile, join, getsize
import copy
import re
import json
import shutil
from jsmin import jsmin

root = "C:/Users/matt/Documents/Projects/OG-OrbHunt/custom_assets/jak3/levels"
fixed = "C:/Users/matt/Documents/Projects/OG-OrbHunt/custom_assets/jak3/levels/orbs-sewh/orbs-sewh.glb"

for d in listdir(root):
    path = join(root, d)
    if isdir(path):
        # in a level folder
        for f in listdir(path):
            full = join(path, f)
            if ".glb" in f and getsize(full) == 1936 and "sewh" not in f:
                # found an empty .glb file
                shutil.copy(fixed, full)

                print(f"overwrote {full}")

                # wait for user input
                input("Press Enter to continue...")
                