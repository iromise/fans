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

rough_function_data_dir = os.path.join(
    cfg["rough_interface_related_data_dir"], "function")
interface_model_extractor_dir = os.path.join(
    fans_dir, cfg["interface_model_extractor_dir"])
precise_function_data_dir = os.path.join(interface_model_extractor_dir,"model","function")
already_parsed_functions_file = os.path.join(interface_model_extractor_dir, "already_parsed_functions.txt")
already_parsed_functions = []
if os.path.exists(already_parsed_functions_file):
    already_parsed_functions = open(already_parsed_functions_file).read().split("\n")


interface_model_extractor_tmp_dir = os.path.join(interface_model_extractor_dir, "tmp")

blacklist_funcname = ["android::hardware::writeToParcel","android::hardware::readFromParcel"]

logging.basicConfig(format='%(levelname)s - %(message)s')
logger = logging.getLogger(__name__)
logger.setLevel(logging.ERROR)
type_map = load_simplified_typemap()

def check_funcname(funcname):
    if funcname in ["android::hardware::writeToParcel","android::hardware::readFromParcel","android::AMessage::setPointer"]:
        return False
    if "android::Bp" in funcname:
        return False
    if "MediaPlayer2Interface" in funcname or "MediaPlayerBase" in funcname:
        return False
    # if "::MediaPlayer::" in funcname or "::MediaPlayer2::" in funcname:
    #     return False
    # if "::MediaPlayerService::" in funcname:
    #     return False
    return True

def parse_one_parcel_function(filename):

    logger.info("Start dealing with " + filename)
    data = xml2json(os.path.join(rough_function_data_dir, filename))

    tmp_function_file = os.path.join(
        interface_model_extractor_tmp_dir, "tmp_function_file.json")
    open(tmp_function_file, "w").write(get_formatted_json(data))

    function = data["function"]
    func_name = function["funcName"]["$"]
    if func_name.startswith("class "):
        func_name = func_name.split("class ")[1]
    if func_name.startswith("struct "):
        func_name = func_name.split("struct ")[1]
    signature = function["signature"]["$"]
    if not check_funcname(func_name):
        return 
    if func_name + "+" + signature in already_parsed_functions:
        return
    if len(function) > 2:
        ast_walker = ASTVisitor(type_map)
        parcel = ast_walker.walk_parcel_function(function)
        # add_qualified_name(parcel, func_name)
        # add_qualified_name(parcel, struct_name)
        data = OrderedDict()
        data[func_name + "+" + signature] = parcel
        open(os.path.join(precise_function_data_dir,filename[:-4]+".json"),"w").write(get_formatted_json(data))
    logging.info("Finish parsing one parcel function: " + filename)
    open(already_parsed_functions_file, "a").write(func_name + "+" + signature + "\n")



def parse_parcel_functions():
    for filename in os.listdir(rough_function_data_dir):
        if not os.path.isdir(filename):
            tmp = filename
            if "-" in filename:
                tmp = tmp.split("-")[0]
            else:
                tmp = tmp[:-4]
            parse_one_parcel_function(str(filename))


if __name__=="__main__":
    parser = argparse.ArgumentParser(description="parse function")
    parser.add_argument("-f","--function",help="parse one function")
    args = parser.parse_args()
    if args.function:
        parse_one_parcel_function(args.function)
    else:
        parse_parcel_functions()