import json
import os

cfg = json.load(open("../../fans.cfg"))
fans_dir = cfg["fans_dir"]

special_type = json.load(open("data/special_type.json"))
basic_types = open("data/basic_type.txt").read().split("\n")
rough_typemap_file = os.path.join(cfg["rough_interface_related_data_dir"],"typemap.txt")
precise_typemap_file = os.path.join(fans_dir,cfg["interface_model_extractor_dir"],"model","typemap.txt")
simplified_typemap_file  = os.path.join(fans_dir,cfg["interface_model_extractor_dir"],"simplified_typemap_file.txt")

def get_true_complex_type(type_map,var_type):
    if var_type in type_map and type_map[var_type] not in basic_types:
        return type_map[var_type]
    else:
        return var_type


def load_simplified_typemap():
    type_map = dict()
    with open(simplified_typemap_file) as f:
        for line in f.readlines():
            key,value = line.strip().split("+")
            type_map[key]=value
    return type_map


def save_type_map(type_map, used_types):
    with open(precise_typemap_file, "w") as f:
        for key, value in type_map.items():
            if key in used_types and value in basic_types:
                f.write(key + "+" + value + "\n")
            elif key in used_types:
                print(key,value)
                raw_input("complex type?")