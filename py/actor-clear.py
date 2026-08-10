import os
import json
from jsmin import jsmin

for root, dirs, files in os.walk("C:/Users/matt/Documents/Projects/OG-OrbHunt/custom_assets/jak3/levels"):
    for name in files:
        if name.endswith((".jsonc")):
            full_path = os.path.join(root, name)
            print(f"Updating {full_path}")
            with open(full_path, "r") as f:
                m = jsmin(f.read())
                j = json.loads(m)
            j["actors"] = []
            with open(full_path, "w") as f:
                f.write(json.dumps(j, indent=4))