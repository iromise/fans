from variable_type import save_type_map,load_simplified_typemap
import os
import json

cfg = json.load(open("../../fans.cfg"))

fans_dir = cfg["fans_dir"]

interface_model_extractor_dir = os.path.join(
    fans_dir, cfg["interface_model_extractor_dir"])

type_map = load_simplified_typemap()

used_types_file = os.path.join(interface_model_extractor_dir,"used_types.txt")
used_types = open(used_types_file).read().split("\n")

save_type_map(type_map,used_types)