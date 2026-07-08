from os import listdir
from os.path import isdir, isfile, join
import copy
import re
import json
from jsmin import jsmin

root = "C:/Users/matt/Documents/Projects/OG-OrbHunt/custom_assets/jak3/levels"

orb_names = {}
orb_pos = {}
for d in listdir(root):
    path = join(root, d)
    if isdir(path):
        # in a level folder
        for f in listdir(path):
            if ".jsonc" in f:
                # found a .jsonc file
                counts = {"barg": 0}
                with open(join(path, f), 'r') as file:
                    m = jsmin(file.read())
                    j = json.loads(m)
                    for a in j["actors"]:
                        # info about counts/authors
                        if a["etype"] == "skill" or a["etype"] == "skill-crate":
                            if "author" in a["lump"]:
                                aa = a["lump"]["author"]
                                if aa not in counts:
                                    counts[aa] = 0
                                counts[aa] += 1
                            else:
                                counts["barg"] += 1
                            
                            # check for reused names or locations
                            name = a["lump"]["name"]
                            if name in orb_names:
                                print(f"DUPLICATE ORB NAME: {name}")
                            else:
                                orb_names[name] = 0
                            orb_names[name] += 1

                            pos = str(a["trans"])
                            if pos in orb_pos:
                                print(f"DUPLICATE ORB POSITION: {pos}")
                            else:
                                orb_pos[pos] = 0
                            orb_pos[pos] += 1
                
                # hacky reminder about onintent
                print(f"{f[5:len(f)-6]}    {counts}")
                if f == "orbs-ctysluma.jsonc":
                    print("   some of those^^^ are onintent")

                # wait for user input
                # input("Press Enter to continue...")
                