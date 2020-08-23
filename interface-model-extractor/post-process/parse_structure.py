import os
from jsonutil import xml2json,get_formatted_json,add_qualified_name
import json
from collections import OrderedDict
import logging
from variable_type import load_simplified_typemap
from ast_visitor import ASTVisitor
import argparse

cfg = json.load(open("../../fans.cfg"))

fans_dir = cfg["fans_dir"]

rough_structure_data_dir = os.path.join(
    cfg["rough_interface_related_data_dir"], "structure")
interface_model_extractor_dir = os.path.join(
    fans_dir, cfg["interface_model_extractor_dir"])
precise_parcelable_structure_data_dir = os.path.join(interface_model_extractor_dir,"model","structure","parcelable")

already_parsed_structures_file = os.path.join(interface_model_extractor_dir, "already_parsed_structures.txt")
already_parsed_structures = []
if os.path.exists(already_parsed_structures_file):
    already_parsed_structures = open(already_parsed_structures_file).read().split("\n")

already_parsed_functions_file = os.path.join(interface_model_extractor_dir, "already_parsed_functions.txt")

interface_model_extractor_tmp_dir = os.path.join(interface_model_extractor_dir, "tmp")

special_parcelable_function_storage_location = os.path.join(fans_dir, cfg[
    "special_parcelable_function_storage_location"])
special_parcelable_function = open(
    special_parcelable_function_storage_location).read().strip().split(
        "\n")
special_parcelable_function_splited = []
for item in special_parcelable_function:
    special_parcelable_function_splited.append(item.split("+"))

logger = logging.getLogger(__name__)
logger.setLevel(logging.ERROR)

type_map = load_simplified_typemap()



def parse_one_structure(filename):
    logger.info("Start dealing with " + filename)
    structure = xml2json(os.path.join(rough_structure_data_dir, filename))

    tmp_structure_file = os.path.join(
        interface_model_extractor_tmp_dir, "tmp_structure_file.json")
    open(tmp_structure_file, "w").write(get_formatted_json(structure))

    qualified_function_name = filename[:-4]
    if "-" in qualified_function_name:
        qualified_function_name = qualified_function_name.split("-")[0]
    tmp = qualified_function_name.split("::")
    non_qualified_function_name = tmp[-1]
    struct_name = "::".join(tmp[:-1])
    

    if non_qualified_function_name in ["readFromParcel", "writeToParcel"]:
        struct_name = "::".join(tmp[:-1])
        structure_read_write = non_qualified_function_name
    else:
        flag = False
        for item in special_parcelable_function_splited:
            # even there is no "::", it does not matter
            suffix = "::".join(item[0].split("::")[-2:])
            if item[0] == qualified_function_name or qualified_function_name.endswith(suffix):
                structure_read_write = item[-1]
                struct_name = item[-2]
                flag = True
                break
        if flag is False:
            return
        else:
            open(already_parsed_functions_file, "a").write(item[0] + "+" + item[1] + "\n")


    ast_walker = ASTVisitor(type_map)
    ast_walker.set_structure_read_write(structure_read_write)
    parcel = ast_walker.walk_structure(structure)
    # TODO: do we need update name to qualified name?
    # if updated, consider argv in function variable.
    # add_qualified_name(parcel, struct_name)
    data = OrderedDict()
    data[struct_name] = parcel
    info = get_formatted_json(data)
    if structure_read_write == "readFromParcel":
        outfile = os.path.join(precise_parcelable_structure_data_dir,"data",struct_name+".json")
        open(outfile, "w").write(info)
    elif structure_read_write == "writeToParcel":
        outfile = os.path.join(precise_parcelable_structure_data_dir,"reply",struct_name+".json")
        open(outfile, "w").write(info)
        # very special
        if struct_name=="class android::MetaDataBase":
            struct_name = "class android::MetaData"
            data[struct_name] = parcel
            info = get_formatted_json(data)
            outfile = os.path.join(precise_parcelable_structure_data_dir,"reply",struct_name+".json")
            open(outfile, "w").write(info)
    else:
        logger.error(filename)
        logger.error("Unexpected thing meeted when parsing structure.")
        exit(0)
    logger.info("Finish parsing one structure: " + filename)
    open(already_parsed_structures_file, "a").write(filename + "\n")

def parse_structures():
    for filename in os.listdir(rough_structure_data_dir):
        if not os.path.isdir(filename):
            if filename not in already_parsed_structures:
                filename = str(filename)
                if "android::Parcel::writeNullableParcelable" in filename:
                    continue
                if "android::Parcel::readParcelable" in filename:
                    continue
                if "android::hardware::readFromParcel" in filename:
                    continue
                if "android::hardware::writeToParcel" in filename:
                    continue
                parse_one_structure(filename)

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="parse structure")
    parser.add_argument("-s","--structure",help="parse one structure")
    args = parser.parse_args()
    if args.structure:
        parse_one_structure(args.structure)
    else:
        parse_structures()