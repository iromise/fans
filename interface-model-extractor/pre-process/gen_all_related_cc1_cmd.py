import os
import json
import ast
cfg = json.load(open("../../fans.cfg"))

fans_dir = cfg["fans_dir"]
aosp_dir = cfg["aosp_dir"]
aosp_compilation_cmd_file = cfg["aosp_compilation_cmd_file"]
aosp_compilation_cc1_cmd_file = os.path.join(fans_dir, cfg["aosp_compilation_cc1_cmd_file"])
aosp_clang_location = cfg["aosp_clang_location"]
service_related_filepath_storage_location = os.path.join(fans_dir,cfg[
    "service_related_filepath_storage_location"])

cc1_cmd = dict()
if os.path.exists(aosp_compilation_cc1_cmd_file):
    cc1_cmd = ast.literal_eval(open(aosp_compilation_cc1_cmd_file).read())

def gen_cc1_cmd(cmd, filepath):
    compile_cmd = cmd.split('] ')[1].strip()
    if "/bin/bash -c" in cmd:
        cmd = compile_cmd[:-1] + " -v 2>&1 | grep %s" % aosp_clang_location + '"'
    else:
        cmd = compile_cmd + " -v 2>&1 | grep %s" % aosp_clang_location

    os.chdir(aosp_dir)
    result = os.popen(cmd).read().strip()
    # print(filepath, result)
    cc1_cmd[filepath] = result


def main():
    service_related_files = open(
        service_related_filepath_storage_location).read().strip().split("\n")

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
            filepath = line.split(" ")[-1].strip().strip('"').strip("'")
            if filepath.startswith("external/"):
                continue
            if '/jni/' in filepath:
                continue
            if filepath not in service_related_files:
                continue
            if filepath in cc1_cmd:
                continue
            if filepath.endswith(".cpp") or filepath.endswith(".cc"):
                print("Generate cc1 cmd for %s" % filepath)
                gen_cc1_cmd(line, filepath)
    json.dump(cc1_cmd, open(aosp_compilation_cc1_cmd_file, "w"), indent=4, sort_keys=True)


if __name__ == "__main__":
    main()
