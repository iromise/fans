import json
import os
import difflib
from collections import OrderedDict

serviceMap = OrderedDict()
dataStructureMap = OrderedDict()
replyStructureMap = OrderedDict()

input = []
output = []
nameAndTypes = []
txCnt = 0
uselessNames = [
    "implicit", "res", "status", "android::NO_MEMORY", "ret", "android::BAD_VALUE",
    "android::NAME_NOT_FOUND", "i", "j", "android::OK", "ServiceInterfaceDescriptor","size"
]
uselessNames.append("width")
uselessNames.append("height")
uselessNames.append("flags")
uselessNames.append("offset")
uselessNames.append("count")
uselessNames.append("Count")
uselessNames.append("index")
uselessNames.append("key")
uselessNames.append("enabled")

basicTypes = open("../interface-model-extractor/post-process/data/basic_type.txt").read().split("\n") 
basicTypes +=["string","android::String16","android::String8","class android::String16","android::String8","class android::String8"]
basicTypes +=["FileDescriptor","Function","unsigned char *","char *","void *"]
func2svc = json.load(open("../interface-model-extractor/post-process/data/func2svcname.json"))
cfg = json.load(open("../fans.cfg"))

fans_dir = cfg["fans_dir"]

interface_model_dir = os.path.join(fans_dir, cfg["interface_model_extractor_dir"],"model")

interface_dependency_file = os.path.join(fans_dir,cfg["interface_dependency_dir"],"interface_dependency.txt")

typedef_path = os.path.join(interface_model_dir,"typemap.txt")
data = open(typedef_path).read().strip().split("\n")
typedef_map = dict()
for item in data:
    if item == "" or "type-parameter" in item or "decltype" in item or "typename" in item:
        continue
    name, under = item.split("+")
    typedef_map[name] = under


def getVarTypeAndName(varName, variables):
    for item in variables:
        if item["name"] == varName:
            varType = item["type"]
            if "vector<" in varType or "Vector<" in varType:
                if varName!="sessionId" and varName!="keySetId" and varName!="signature":
                    varType = varType.split("tor<")[-1][:-1]
                    varName = varName + "::item"
            if "[" in varType:
                tmp = varType.split("[")
                varType = tmp[0].strip()
                varName = varName + "::item"
            if "*" in varType:
                varType = varType.replace("*", "")
                varName = varName + "::item"
            varType = varType.strip()
            return varName, varType


def isBasicType(varType):
    if varType in basicTypes:
        return True
    return False


def isUselessName(varName):
    varName = varName.split("::")[-1]
    varName = varName.split(".")[-1]
    if varName in uselessNames:
        return True
    else:
        tmp = varName.split("_")
        if len(tmp) != 2:
            return False
        else:
            prefix = tmp[0]
            suffix = tmp[1]
            if suffix.isdigit() and prefix in uselessNames:
                return True
            elif prefix in ["in","out"] and suffix in uselessNames:
                return True
            else:
                return False


def checkUseless(varName, varType):
    if varName == "ServiceInterfaceDescriptor":
        return True
    if isUselessName(varName) and isBasicType(varType):
        return True
    return False


def walkParcel(filename, itemName, possIdx, variables, parcel, isinput):
    # isinput, data:1; reply 0
    for varIdx in range(0, len(parcel)):
        # print(parcel[varIdx],variables)
        varName, varType = getVarTypeAndName(parcel[varIdx], variables)
        nameAndTypes.append([varName, varType])

        if checkUseless(varName, varType):
            if varName.endswith("::item"):
                print(varName, varType)
                raw_input("useless???")
            continue
        node = OrderedDict()
        node["filename"] = filename
        if filename in serviceMap:
            node["type"] = "Transaction"
        else:
            node["type"] = "Structure"
        node["name"] = itemName
        node["possIdx"] = possIdx
        node["varIdx"] = varIdx
        node["varName"] = varName
        node["varType"] = varType
        if isinput == 1:
            input.append(node)
        else:
            output.append(node)


def walkItem(filename, itemName, itemInfo):
    global txCnt
    possibility = itemInfo["possibility"]
    variables = itemInfo["variable"]
    for possIdx in range(0, len(possibility)):
        txCnt += 1
        item = possibility[possIdx]
        data = item["data"]
        walkParcel(filename, itemName, possIdx, variables, data, 1)
        reply = item["reply"]
        walkParcel(filename, itemName, possIdx, variables, reply, 0)


def walkJson(filename, jsonInfo):
    for itemName, itemInfo in jsonInfo.items():
        walkItem(filename, itemName, itemInfo)


def loadJsonInfo(dirName, serviceMap):
    files = os.listdir(dirName)
    for file in files:
        if not os.path.isdir(file):
            info = json.load(
                open(os.path.join(dirName, str(file))),
                object_pairs_hook=OrderedDict)
            serviceMap[str(file)] = info


def isSameType(inputType, outputType):
    if inputType == outputType:
        return True
    else:
        return False


def isNameSimilar(inputName, outputName):
    return difflib.SequenceMatcher(None, inputName, outputName).ratio() > 0.9

def simplified_name(varname):
    # suppose all var name that begin with "in_" or "out_"
    # are inside interface generated.

    # lower case: requestId, mRequestId
    if varname.startswith("in_"):
        return varname[3:].lower()
    elif varname.startswith("out_"):
        return varname[4:].lower()
    else:
        varname = varname.split("::")[-1]
        varname = varname.split(".")[-1]
        varname = varname.split("->")[-1]
        return varname.lower()


def gen_variable_dependency():
    for data in input:
        for reply in output:
            # self structure
            if data["filename"] == reply["filename"] and data[
                    "type"] == "Structure":
                continue
            if data["filename"]=="class android::MetaDataBase.json" and reply["filename"]=="class android::MetaData.json":
                # very special.
                continue
            # exclude self dependency
            if data["name"] == reply["name"]:
                continue

            # when getting dependency from structure,
            # the reply should be able to be generated by others..
            if reply["type"] == "Structure":
                filename = reply["filename"]
                if len(replyStructureMap[filename][reply["name"]]
                       ["dependency"]) == 0:
                    continue
            flag = False
            if isSameType(data["varType"], reply["varType"]):
                # Maybe we do not need to consider the blob dependency as blob will be inside another structure.
                if data["varType"]=="class android::Parcel::Blob":
                    flag = False
                elif data["varType"] not in basicTypes:
                    if "Vector<" in data["varType"]:
                        if isNameSimilar(data["varName"],reply["varName"]):
                            flag=True
                        else:
                            flag=False
                    else:
                        flag = True
                else:
                    if data["varName"].endswith(
                            "::item") or reply["varName"].endswith("::item"):
                        flag = False
                    elif not data["varName"].startswith("implicit"):
                        data_name = data["varName"]
                        reply_name = reply["varName"]
                        data_name = simplified_name(data_name)
                        reply_name = simplified_name(reply_name)
                        if isNameSimilar(data_name, reply_name):
                            flag = True
            if flag:
                filename = data["filename"]
                if filename in serviceMap:
                    info = serviceMap[filename]
                elif filename in dataStructureMap:
                    info = dataStructureMap[filename]
                else:
                    print("error......")
                    exit(0)
                item = info[data["name"]]
                variables = item["variable"]
                for var in variables:
                    if var["name"] == data["varName"]:
                        if reply not in var["dependency"]:
                            var["dependency"].append(reply)


def get_interface(name):
    if name=="BnBinder":
        return "IBinder"
    return func2svc[name]["interfaceName"]

def gen_interface_dependency():
    f = open(interface_dependency_file, "w")
    f.write('digraph graphname {\n')
    dependencySet = set()
    for name in serviceMap.keys():
        for reply in output:
            # do not consider interface dependency inside structure item
            # if you consider that, be sure that the reply should be able to be generated by others.
            if reply["type"] == "Structure":
                continue

            service_interface = get_interface(name[:-5])
            if "sp<" in reply["varType"] and "::" + service_interface + ">" in reply[
                    "varType"]:
                dependency = get_interface(
                    reply["filename"][:-5]) + " -> " + get_interface(name[:-5])
                dependencySet.add(dependency)
                for tx in serviceMap[name]:
                    serviceMap[name][tx]["dependency"].append(reply)
    for data in input:
        if data["type"] == "Structure":
            continue
        for name in serviceMap.keys():
            # 1. the data should be in the service
            # 2. the data must be a sp<>
            if data["filename"] == name and "sp<" in data[
                    "varType"] and ">" in data["varType"]:
                dependency = get_interface("Bn" + data["varType"].split(
                    "::I")[-1][:-1]) + " -> " + get_interface(
                        name[:-5]) + " [style=dotted];"
                dependencySet.add(dependency)
    for item in dependencySet:
        f.write(item + "\n")
    f.write('}\n')


def gen_structure_dependency():
    for name in replyStructureMap:
        # remove .json suffix
        structure_type = name[:-5]
        for reply in output:
            # only consider the structure directly generated by the transaction
            if reply["type"] == "Transaction" and reply["varType"] == structure_type:
                for item in replyStructureMap[name]:
                    replyStructureMap[name][item]["dependency"].append(reply)


def gen_dependency():
    # if one structure can be generated by another service.
    gen_structure_dependency()

    # interface dependency
    gen_interface_dependency()

    # variable dependency
    gen_variable_dependency()


def dumpInfo(infoMap, path):
    for name in infoMap.keys():
        open(path + "/" + name, "w").write(
            json.dumps(infoMap[name], indent=4, separators=(',', ': ')))


def main():
    interface_model_service_dir = os.path.join(interface_model_dir,"service")
    loadJsonInfo(interface_model_service_dir, serviceMap)
    for filename, itemInfo in serviceMap.items():
        walkJson(filename, itemInfo)
    interface_model_parcelable_structure_data_dir = os.path.join(interface_model_dir,"structure","parcelable","data")
    loadJsonInfo(interface_model_parcelable_structure_data_dir, dataStructureMap)
    for filename, itemInfo in dataStructureMap.items():
        walkJson(filename, itemInfo)

    interface_model_parcelable_structure_reply_dir = os.path.join(interface_model_dir,"structure","parcelable","reply")
    loadJsonInfo(interface_model_parcelable_structure_reply_dir, replyStructureMap)
    for filename, itemInfo in replyStructureMap.items():
        walkJson(filename, itemInfo)

    gen_dependency()

    dumpInfo(serviceMap, interface_model_service_dir)
    dumpInfo(dataStructureMap, interface_model_parcelable_structure_data_dir)
    dumpInfo(replyStructureMap, interface_model_parcelable_structure_reply_dir)

if __name__ == "__main__":
    main()