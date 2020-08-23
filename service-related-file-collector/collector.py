import os
import json
import ast

cfg = json.load(open("../fans.cfg"))

aosp_dir = cfg["aosp_dir"]
fans_dir = cfg["fans_dir"]

service_related_file_collector_workdir = os.path.join(
    fans_dir, cfg["service_related_file_collector_workdir"])

service_related_filepath_storage_location = os.path.join(fans_dir, cfg[
    "service_related_filepath_storage_location"])

service_related_files = []
if os.path.exists(service_related_filepath_storage_location):
    service_related_files = open(service_related_filepath_storage_location).read().strip("\n").split("\n")

interface2file_storage_location = os.path.join(
    service_related_file_collector_workdir, "interface2file.json")

interface2file = dict()
if os.path.exists(interface2file_storage_location):
    interface2file = ast.literal_eval(open(interface2file_storage_location).read())


misc_parcel_related_function_storage_location = os.path.join(fans_dir, cfg[
    "misc_parcel_related_function_storage_location"])
misc_parcel_related_function = []
with open(misc_parcel_related_function_storage_location, "r") as f:
    for line in f.readlines():
        line = line.strip()
        if line == "":
            continue
        else:
            name, signature = line.split("+")
        misc_parcel_related_function.append([name, signature])

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
        special_parcelable_function.append(
            [name, signature, return_type, isinput])



def is_interface_line(line):
    if "=" in line:
        return False
    if "status_t onTransact" in line:
        return False
    if "<<" in line:
        return False
    if "status_t" in line and "onTransact" in line:
        return True

def try_get_interfaces(relative_filepath):
    absolute_filepath = os.path.join(aosp_dir, relative_filepath)
    data = open(absolute_filepath).read()
    interfaces = []
    # count how many interfaces located in one
    cnt = 0
    for line in data.splitlines():
        # indicating this is the onTransact function
        # ::android::status_t BnHwWifi::onTransact(
        # exclude "status_t onTransact(" in line for the declaration in c++ file, e.g.,
        # onTransact in system/extras/perfprofd/binder_interface/perfprofd_binder.cc
        if is_interface_line(line):
            cnt += 1
            # record the interface2file map
            # example: status_t BnAAudioService::onTransact(uint32_t code, const Parcel& data,
            interface_name = line.split("::onTransact")[
                0].split("status_t ")[1]
            interfaces.append(interface_name)
    return interfaces


def is_interface_related(relative_filepath):
    absolute_filepath = os.path.join(aosp_dir, relative_filepath)
    data = open(absolute_filepath).read()
    if "readFromParcel(" in data or "writeToParcel(" in data or "flatten(" in data or "unflatten(" in data:
        return True
    else:
        for item in misc_parcel_related_function:
            name = item[0].split("::")[-1]
            if name + "(" in data:
                # print(name)
                return True

        flag = False
        for item in special_parcelable_function:
            namelist = item[0].split("::")
            if namelist[-1] + "(" in data:
                if namelist[-1] == "read":
                    if namelist[-2] in data:
                        # print(namelist)
                        flag = True
                else:
                    flag = True
        return flag


def main():
    aosp_compilation_cmd_file = cfg["aosp_compilation_cmd_file"]
    with open(aosp_compilation_cmd_file, 'r') as f:
        for line in f:
            if "prebuilts/clang" not in line:
                continue
            if "mkdir -p" in line or "rm -f" in line:
                continue
            if "out/soong/host/linux-x86/bin/aidl-cpp" in line:
                continue
            if "tests/" in line or "test_" in line:
                continue
            relative_filepath = line.split(" ")[-1].strip().strip('"').strip(
                "'")
            if relative_filepath.startswith("external/"):
                continue
            if '/jni/' in relative_filepath:
                continue
            if  relative_filepath.startswith("out/soong/") and  "android_arm_armv8" in relative_filepath:
                # Currently, we exclude the file generated for 32-bit programs.
                continue
            if relative_filepath in service_related_files:
                continue
            if relative_filepath.endswith(
                    ".cpp") or relative_filepath.endswith(".cc"):
                interfaces = try_get_interfaces(relative_filepath)
                # exclude Hardware related interface, e.g.,
                # ::android::status_t BnHwNxpEse::onTransact(
                if len(interfaces)==0:
                    if is_interface_related(relative_filepath):
                        # print("interface_related: ", relative_filepath)
                        # print(line, filepath)
                        print("Find new file indirectly related with interface: %s" % relative_filepath)
                        service_related_files.append(relative_filepath)
                else:
                    for item in interfaces:
                        if item.startswith("BnHw"):
                            continue
                        else:
                            # print("expected_interface: ", relative_filepath)
                            interface2file[item] = relative_filepath
                            # print(line, filepath)
                            if relative_filepath not in service_related_files:
                                print("Find new file directly related with interface: %s" % relative_filepath)
                                service_related_files.append(relative_filepath)
    with open(service_related_filepath_storage_location, "w") as f:
        for item in service_related_files:
            if item!="":
                f.write(item+"\n")
    data = json.dumps(interface2file, indent=4, separators=(",", ":"))
    open(interface2file_storage_location, "w").write(data)


if __name__ == "__main__":
    main()
