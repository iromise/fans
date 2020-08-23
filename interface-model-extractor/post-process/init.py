import json
import os

cfg = json.load(open("../../fans.cfg"))
fans_dir = cfg["fans_dir"]

rough_typemap_file = os.path.join(cfg["rough_interface_related_data_dir"],"typemap.txt")
simplified_typemap_file  = os.path.join(fans_dir,cfg["interface_model_extractor_dir"],"simplified_typemap_file.txt")


def simpilify_type_map():
    type_map = dict()
    with open(rough_typemap_file) as f:
        for line in f.readlines():
            line = line.strip()
            if line == "" or "type-parameter" in line or "decltype" in line or "typename" in line or line.startswith(
                    "Eigen::"):
                continue
            name, under = line.split("+")
            if name in type_map:
                continue
            # e.g. ::std::android
            name = name.strip("::")
            # map to the true type
            while under in type_map:
                under = type_map[under]
            type_map[name] = under

    with open(simplified_typemap_file, "w") as f:
        for key, value in type_map.items():
            f.write(key + "+" + value + "\n")


if __name__ == "__main__":
    simpilify_type_map()