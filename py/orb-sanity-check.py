from os import listdir
from os.path import isdir, isfile, join
import copy
import re
import json
from jsmin import jsmin

root = "C:/Users/matt/Documents/Projects/OG-OrbHunt/custom_assets/jak3/levels"

expected = {
    'orbs-rubblea': {'barg': 41}
    'o-rubblea2': {'barg': 47}
    'orbs-rubbleb': {'barg': 161}
    'orbs-rubblec': {'barg': 128}
    'orbs-rublcst': {'barg': 61}
    'orbs-precurd': {'barg': 21}
    'orbs-precurc': {'barg': 16}
    'orbs-precurb': {'barg': 16}
    'orbs-precura': {'barg': 33}
    'orbs-minec': {'barg': 310}
    'orbs-mineb': {'barg': 106}
    'orbs-minea': {'barg': 21}
    'orbs-mined': {'barg': 24}
    'orbs-freehq': {'barg': 8}
    'orbs-lfacrm1': {'barg': 6}
    'orbs-wasdoors': {'barg': 7}
    'orbs-ctyfarma': {'barg': 23}
    'orbs-ctyfarmb': {'barg': 24}
    'orbs-gungame': {'barg': 11}
    'orbs-halfpipe': {'barg': 28}
    'orbs-foresta': {'barg': 76}
    'orbs-forestb': {'barg': 79}
    'orbs-ctyindb': {'barg': 28}
    'orbs-ctyinda': {'barg': 18}
    'orbs-ctygenb': {'barg': 12}
    'orbs-mhcityb': {'barg': 3}
    'orbs-mhcitya': {'barg': 15}
    'orbs-towerb': {'barg': 20}
    'orbs-ltowera': {'barg': 9}
    'orbs-ltowerb': {'barg': 4}
    'orbs-nsta': {'barg': 11}
    'orbs-nstb': {'barg': 2}
    'orbs-templeb': {'barg': 16}
    'orbs-templea': {'barg': 8}
    'orbs-templex': {'barg': 42}
    'orbs-templec': {'barg': 23}
    'orbs-templed': {'barg': 48}
    'orbs-hiphog': {'barg': 4}
    'orbs-ctyslumc': {'barg': 6}
    'orbs-ctyslumb': {'barg': 24}
    'orbs-powergd': {'barg': 3}
    'orbs-ctyport': {'barg': 4}
    'orbs-vinroom': {'barg': 2}
    'orbs-sewl': {'barg': 14}
    'orbs-sewm': {'barg': 7}
    'orbs-sewn': {'barg': 5, 'blahpy': 6}
    'orbs-sewo': {'barg': 2}
    'orbs-sewk': {'barg': 5}
    'orbs-sewg': {'barg': 9}
    'orbs-sewh': {'barg': 8}
    'orbs-sewi': {'barg': 25}
    'orbs-sewj': {'barg': 11}
    'orbs-sewb': {'barg': 2}
    'orbs-sewc': {'barg': 12}
    'orbs-sewd': {'barg': 6}
    'orbs-sewe': {'barg': 8}
    'orbs-sewf': {'barg': 7}
    'orbs-ctysluma': {'barg': 6}
    # some of those^^^ are onintent
    'orbs-wascitya': {'barg': 8, 'TheGuy': 1}
    'orbs-wascityb': {'barg': 24}
    'orbs-wasstada': {'barg': 13}
    'orbs-waspala': {'barg': 4}
    'orbs-stadium': {'barg': 45}
    'orbs-stadiumb': {'barg': 96}
    'orbs-comba': {'barg': 2}
    'orbs-combn': {'barg': 2}
    'orbs-combb': {'barg': 0}
    'orbs-combc': {'barg': 0}
    'orbs-combd': {'barg': 4}
    'orbs-combe': {'barg': 0}
    'orbs-railx': {'barg': 2}
    'orbs-railb': {'barg': 1}
    'orbs-railb2': {'barg': 0}
    'orbs-railc': {'barg': 0}
    'orbs-raild': {'barg': 0}
    'orbs-raile': {'barg': 0}
    'orbs-railf': {'barg': 0}
    'orbs-deswalk': {'barg': 9}
    'o-wstadb': {'barg': 10}
    'orbs-volcanoa': {'barg': 344}
    'orbs-volcanox': {'barg': 28}
    'orbs-lfacrm2': {'barg': 2}
    'orbs-factoryd': {'barg': 11}
    'orbs-factoryb': {'barg': 97}
    'orbs-factoryc': {'barg': 69}
    'orbs-desert': {'barg': 1}
    'orbs-desertd': {'barg': 7, 'TheGuy': 1}
    'orbs-desertb': {'barg': 41, 'TheGuy': 2}
    'orbs-deserta': {'barg': 6, 'blahpy': 6}
    'orbs-desertf': {'barg': 8}
    'orbs-deserth': {'barg': 38}
    'orbs-desertg': {'barg': 91}
    'orbs-deserte': {'barg': 53}
    'orbs-desertc': {'barg': 106}
    'orbs-intpalrf': {'barg': 26}
    'test-zone': {'barg': 0}
}

orb_names = {}
orb_pos = {}
total = 0
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
                            total += 1    
                            
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
                filename = f[:len(f)-6]
                print(f"{filename}    {counts}   /   expected: {expected[filename]}")
                if f == "orbs-ctysluma.jsonc":
                    print("   some of those^^^ are onintent")

                # wait for user input
                # input("Press Enter to continue...")
print(f"Total added orbs: {total}")
print(f"Total (+vanilla) orbs: {total+600}")