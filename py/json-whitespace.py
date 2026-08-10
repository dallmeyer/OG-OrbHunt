from os import listdir
from os.path import isdir, isfile, join
import copy
import re

root = "C:/Users/matt/Documents/Projects/OG-OrbHunt/custom_assets/jak3/levels"

for d in listdir(root):
    path = join(root, d)
    if isdir(path):
        # in a level folder
        for f in listdir(path):
            if ".jsonc" in f:
                # found a .jsonc file
                data3 = ""
                with open(join(path, f), 'r') as file:
                    data = file.read()
                    data2 = re.sub("(\\S)  +", "\\1 ", data) # remove double spaces following any non-whitespace
                    data3 = re.sub(" +\\n", "\\n", data2) # remove trailing whitespace

                with open(join(path, f), 'w') as file:
                    file.write(data3)

                # data = JsoncParser.parse_file(join(path, f))
                # actors = data["actors"]
                # ghosts = []
                # for a in actors:
                #     if a["etype"] == "skill":
                #         b = copy.deepcopy(a)
                #         b["etype"] = "skill-ghost"
                #         lump = b["lump"]
                #         lump["alt-actor"] = lump["name"]
                #         lump["name"] = lump["name"] + "-ghost"
                #         ghosts.append(b)
                # print(ghosts)

                print(f)

                # wait for user input
                # input("Press Enter to continue...")
                