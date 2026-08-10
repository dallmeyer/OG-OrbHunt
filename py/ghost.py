from os import listdir
from os.path import isdir, isfile, join
import copy
import re

root = "C:/Users/matt/Documents/Projects/OG-OrbHunt/custom_assets/jak2/levels"

for d in listdir(root):
    path = join(root, d)
    if isdir(path):
        # in a level folder
        for f in listdir(path):
            if ".jsonc" in f:
                # found a .jsonc file
                print(f,"\n\n\n\n")
                with open(join(path, f), 'r') as file:
                    data = file.read()
                    data2 = re.sub("\"name\"\s*:\s*\"(.*)-skill-(.*)\"", "\"name\":\"\g<1>-skill-ghost-\g<2>\",\n        \"alt-actor\":\"\g<1>-skill-\g<2>\"", data)
                    data3 = re.sub("\"etype\"\s*:\s*\"skill\"", "\"etype\":\"skill-ghost\"", data2)
                    print(data3)


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

                print("\n\n", f)
                print("\n\n\n\n")

                # wait for user input
                input("Press Enter to continue...")
                