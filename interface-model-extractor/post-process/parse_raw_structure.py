import os
import json
from collections import OrderedDict
from jsonutil import xml2json,get_formatted_json,add_qualified_name
import logging
from variable_type import load_simplified_typemap,get_true_complex_type
from ast_visitor import ASTVisitor



cfg = json.load(open("../../fans.cfg"))

fans_dir = cfg["fans_dir"]

rough_raw_structure_data_dir = os.path.join(
    cfg["rough_interface_related_data_dir"], "raw_structure")
interface_model_extractor_dir = os.path.join(
    fans_dir, cfg["interface_model_extractor_dir"])
precise_raw_structure_data_dir = os.path.join(interface_model_extractor_dir,"model","structure","raw")
precise_union_data_dir = os.path.join(interface_model_extractor_dir,"model","union")

interface_model_extractor_tmp_dir = os.path.join(interface_model_extractor_dir, "tmp")

logging.basicConfig(format='%(levelname)s - %(message)s')
logger = logging.getLogger(__name__)
logger.setLevel(logging.ERROR)

type_map = load_simplified_typemap()



def parse_one_raw_structure(filename):
    logger.info("Start dealing with " + filename)
    struct_name = filename[:-4]
    if os.path.exists(os.path.join(rough_raw_structure_data_dir, filename)):
        structure = xml2json(os.path.join(rough_raw_structure_data_dir, filename))
    else:
        logger.info("try again...")
        if struct_name in type_map:
            # for the following kind..
            # struct audio_config_base {
            #     uint32_t sample_rate;
            #     audio_channel_mask_t channel_mask;
            #     audio_format_t  format;
            # };
            # typedef struct audio_config_base audio_config_base_t;
            filename = type_map[struct_name] + ".xml"
            structure = xml2json(os.path.join(rough_raw_structure_data_dir, filename))
        else:
            logger.error(str(os.path.join(rough_raw_structure_data_dir, filename)) + " do not exists..")
            exit(0)
    tmp_raw_structure_file = os.path.join(
        interface_model_extractor_tmp_dir, "tmp_raw_structure_file.json")
    open(tmp_raw_structure_file, "w").write(get_formatted_json(structure))


    key = structure.keys()[0]

    ast_walker = ASTVisitor(type_map)
    parcel,raw_structure_set = ast_walker.walk_raw_structure(structure)
    # convert to the true complex type.
    struct_name = get_true_complex_type(type_map, struct_name)
    # TODO: do we need update name to qualified name?
    # add_qualified_name(parcel, struct_name)
    data = OrderedDict()
    data[struct_name] = parcel
    parcel = get_formatted_json(data)

    if key == "union":
        outfile = os.path.join(precise_union_data_dir,struct_name+".json")
    else:
        outfile = os.path.join(precise_raw_structure_data_dir,struct_name+".json")
    open(outfile, "w").write(parcel)
    logger.info("Finish parsing one raw structure: " + filename)
    return raw_structure_set

def load_raw_structure_list():
    # load raw structure from file
    raw_structure_list = []
    with open(os.path.join(interface_model_extractor_dir,"raw_structure_types.txt")) as f:
        for line in f.readlines():
            raw_structure_list.append(line.strip())
    raw_structure_list.append("struct android_smpte2086_metadata")
    raw_structure_list.append("struct android_cta861_3_metadata")
    return raw_structure_list

def store_raw_structure_list(raw_structure_list):
    # store raw structure list to file
    with open(os.path.join(interface_model_extractor_dir,"raw_structure_types.txt"),"w") as f:
        for item in raw_structure_list:
            f.write(str(item)+"\n")


def parse_raw_structures():
    # only extract what we need.
    raw_structure_list = load_raw_structure_list()
    raw_structure_num = len(raw_structure_list)
    visited = []
    i = 0
    logger.debug(str(raw_structure_list))

    while i < raw_structure_num:
        name = raw_structure_list[i]
        if name not in visited and name not in ["float"]:
            visited.append(name)
        elif name in ["float"]:
            visited.append(name)
            i += 1
            continue
        else:
            i += 1
            continue
        filename = name + ".xml"
        
        new_raw_structure_set = parse_one_raw_structure(filename)

        if len(new_raw_structure_set)!=0:
            i = 0
            raw_structure_list = raw_structure_list+list(new_raw_structure_set)
            raw_structure_num = len(raw_structure_list)
        else:
            i += 1
    store_raw_structure_list(raw_structure_list)

if __name__=="__main__":
    parse_raw_structures()