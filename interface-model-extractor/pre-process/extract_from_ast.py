import sys
import ast
import os
import subprocess
import shutil

import argparse

import json
cfg = json.load(open("../../fans.cfg"))

fans_dir = cfg["fans_dir"]
aosp_dir = cfg["aosp_dir"]
interface_model_dir = cfg["interface_model_extractor_dir"]
aosp_compilation_cmd_file = cfg["aosp_compilation_cmd_file"]
aosp_compilation_cc1_cmd_file = os.path.join(
    fans_dir, cfg["aosp_compilation_cc1_cmd_file"])
lunch_command = cfg["lunch_command"]
aosp_clang_location = cfg["aosp_clang_location"]
manually_build_clang_location = cfg["manually_build_clang_location"]
clang_plugin_option = cfg["clang_plugin_option"]
service_related_filepath_storage_location = os.path.join(fans_dir, cfg[
    "service_related_filepath_storage_location"])

cc1_cmd = dict()
if os.path.exists(aosp_compilation_cc1_cmd_file):
    cc1_cmd = json.load(open(aosp_compilation_cc1_cmd_file))


already_preprocessed_files_storage_location = os.path.join(fans_dir, cfg[
    "already_preprocessed_files_storage_location"])

interface_model_extractor_dir = os.path.join(
    fans_dir, cfg["interface_model_extractor_dir"])

already_parsed_functions_file = os.path.join(interface_model_extractor_dir, "already_parsed_functions.txt")
already_parsed_functions = []
if os.path.exists(already_parsed_functions_file):
    already_parsed_functions = open(already_parsed_functions_file).read().split("\n")


misc_parcel_related_function_storage_location = os.path.join(fans_dir, cfg[
    "misc_parcel_related_function_storage_location"])
misc_parcel_related_function = []
with open(misc_parcel_related_function_storage_location, "r") as f:
    for line in f.readlines():
        line = line.strip()
        if line == "":
            continue
        misc_parcel_related_function.append(line)

special_parcelable_function_storage_location = os.path.join(fans_dir, cfg[
    "special_parcelable_function_storage_location"])
special_parcelable_function = []
with open(special_parcelable_function_storage_location) as f:
    for line in f.readlines():
        line = line.strip()
        if line == "":
            continue
        else:
            name, signature, return_type, isinput = line.split("+")
        special_parcelable_function.append("+".join([name,signature]))


def execute_one(cmd):
    if cmd.endswith(" -fwhole-program-vtables"):
        filepath = cmd.split(" ")[-2]
    else:
        filepath = cmd.split(" ")[-1]
    if "-D __alignx(x)=__attribute__((__aligned__(x)))" in cmd:
        cmd = cmd.replace("-D __alignx(x)=__attribute__((__aligned__(x)))", "")

    if "tests/" in filepath or "test_" in filepath or "/Value.cpp" in filepath:
        return
    cmd = cmd.replace(aosp_clang_location, manually_build_clang_location)
    cmd = cmd + " " + clang_plugin_option + " 2>&1"
    cmdlist = ["cd %s" % aosp_dir,
               "source build/envsetup.sh",
               lunch_command,
               cmd
               ]
    cmd = '\n'.join(cmdlist)
    try:
        out_bytes = subprocess.check_output(
            cmd, stderr=subprocess.STDOUT, shell=True,executable="/bin/bash").decode("utf-8")
        # print(cmdlist[-1])
        # print(out_bytes)
        if "is not completed!" in out_bytes:
            print(out_bytes)
            exit(0)
    except subprocess.CalledProcessError as e:
        out_bytes = e.output  # Output generated before error
        # code = e.returncode  # Return code
        print(out_bytes)
        print("exception meeted.")
        exit(0)


def rm_mk_dir(dir):
    if os.path.exists(dir):
        shutil.rmtree(dir)
    os.makedirs(dir)


def prepare():
    data = os.path.join(cfg["aosp_dir"], "data")
    enumeration_xml_dir = os.path.join(data, "enumeration")
    rm_mk_dir(enumeration_xml_dir)

    function_xml_dir = os.path.join(data, "function")
    rm_mk_dir(function_xml_dir)

    raw_structure_xml_dir = os.path.join(data, "raw_structure")
    rm_mk_dir(raw_structure_xml_dir)

    structure_xml_dir = os.path.join(data, "structure")
    rm_mk_dir(structure_xml_dir)

    interface_xml_dir = os.path.join(data, "interface")
    rm_mk_dir(interface_xml_dir)

    open(already_preprocessed_files_storage_location, "w").write('')


def is_contain_to_process_functions(filepath,to_processed_functions):
    content = open(os.path.join(filepath)).read().split("\n")
    possible_function = []
    for item in to_processed_functions:
        i = 0
        tmp = item.split("::")
        if "readFromParcel" in item or "writeToParcel" in item:
            name = "::".join(tmp[-2:])
        else:
            name = tmp[-1]
        while i<len(content):
            line = content[i]
            flag = 0
            if name+"(" in line:
                if "android::I" in item or "::MediaRecorderBase::" in item or "::MediaPlayerBase::" in item or "::MediaPlayer2Interface::" in item:
                    if  " "+name+"(" in line:
                        flag = 0
                    else:
                        flag = 1
                else:
                    flag = 1
            # check if the line contains the function name pattern
            if flag:
                # check if the function contains the parcel pattern
                j = i
                while j<len(content):
                    # the function ends or the statement ends 
                    if content[j].endswith(";"):
                        break
                    if "Parcel" in content[j]:
                        print("File %s contains %s" % (filepath.split("/")[-1],name))
                        return True
                    if content[j].endswith("{"):
                        break
                    j+=1
            i+=1
    return False

def get_to_processed_functions(collected_functions, already_parsed_functions, to_processed_functions):
    for item in collected_functions:
        func_name, signature = item.split("+")
        non_qualified_function_name  =func_name.split("::")[-1]
        tmp = "+".join([non_qualified_function_name,signature])
        flag = True
        for item_parsed in already_parsed_functions:
            if tmp in item_parsed:
                flag = False
        if flag:
            to_processed_functions.append(item.split("+")[0])

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Extract interface information from ast")
    parser.add_argument("-f","--file",help="process the target file")
    args = parser.parse_args()
    if args.file:
        filepath = args.file
        if filepath in cc1_cmd:
            execute_one(cc1_cmd[filepath])
        else:
            print("%s is not in cc1 cmd" % filepath)
    else:
        option = raw_input("Do you want to remove all of the files and extract again?y/n")
        if option=="y": 
            prepare()
        elif option=="n":
            pass
        else:
            print("The option can only be 'y' or 'n'.")
            exit(0)

        already_parsed_functions = list(set(already_parsed_functions))

        to_processed_cmds = dict()
        to_processed_functions = []

        get_to_processed_functions(misc_parcel_related_function,already_parsed_functions,to_processed_functions)
        get_to_processed_functions(special_parcelable_function,already_parsed_functions,to_processed_functions)

        already_preprocessed_files = open(
            already_preprocessed_files_storage_location).read().strip().split("\n")

        for filepath, cmd in cc1_cmd.items():
            if filepath not in already_preprocessed_files:
                # this means this file hasn't been processed
                to_processed_cmds[filepath]=cmd
            else:
                if is_contain_to_process_functions(os.path.join(aosp_dir,filepath),to_processed_functions):
                    # although this file has been processed
                    # it contains some function which is not processed
                    to_processed_cmds[filepath]=cmd

        for filepath, cmd in to_processed_cmds.items():
            print("Processing file %s" % filepath)
            execute_one(cmd)
            if filepath not in already_preprocessed_files:    
                open(already_preprocessed_files_storage_location,
                    "a").write(filepath + "\n")