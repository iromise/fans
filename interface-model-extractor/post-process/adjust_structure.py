import os
import json
from jsonutil import replace_attr,replace_name_wapper
import logging


cfg = json.load(open("../../fans.cfg"))

fans_dir = cfg["fans_dir"]

rough_structure_data_dir = os.path.join(
    cfg["rough_interface_related_data_dir"], "structure")
interface_model_extractor_dir = os.path.join(
    fans_dir, cfg["interface_model_extractor_dir"])
precise_parcelable_structure_data_dir = os.path.join(interface_model_extractor_dir,"model","structure","parcelable")

basic_types = open("data/basic_type.txt").read().split("\n")
logger = logging.getLogger(__name__)
logger.setLevel(logging.ERROR)

def join_path_list(start, pathList):
    for path in pathList:
        start = os.path.join(start, path)
    return start


def is_bad(name):
    tmp = name.split("::")[-1]
    if tmp == "value" or tmp.startswith("value_") or tmp.startswith("implicit"):
        return True
    return False


def getType(variable, name):
    for item in variable:
        if item["name"] == name:
            return item["type"]


dataPath = os.path.join(precise_parcelable_structure_data_dir, "data")
replyPath = os.path.join(precise_parcelable_structure_data_dir, "reply")


def getVarIdx(variable, name):
    for i in range(len(variable)):
        if variable[i]["name"] == name:
            return i


def try_adjust(data,reply):
    dataposs = data["possibility"][-1]["data"]
    replyposs = reply["possibility"][-1]["reply"]
    if len(dataposs)!=len(replyposs):
        logger.warning("The data's length and the reply's length is not euqal. Wow...")
        return
    for i in range(len(dataposs)):
        logger.debug('%d %s %s' %(i, dataposs[i],replyposs[i]))
        dataIdx = getVarIdx(data["variable"], dataposs[i])
        replyIdx = getVarIdx(reply["variable"], replyposs[i])

        # first we should consider type
        dataType = data["variable"][dataIdx]["type"]
        replyType = reply["variable"][replyIdx]["type"]
        if dataType != replyType:
            if dataType in basic_types and replyType in basic_types:
                logger.warning("May be something bad happened..")
                logger.warning("%s %s" %(dataposs[i], replyposs[i]))
                pass
            elif dataType in basic_types:
                # replace dataType with replyType for dataposs[i]
                replace_attr(data, dataposs[i], "type", replyType)
            else:
                replace_attr(reply, replyposs[i], "type", dataType)

        # then we can consider name
        isDataBad = is_bad(dataposs[i])
        isReplyBad = is_bad(replyposs[i])
        if isDataBad == isReplyBad:
            logger.info("Name: We can not improve anything.")
        elif isDataBad == True:
            replace_name_wapper(data, dataposs[i], "name", replyposs[i])
            # dataposs[i] = replyposs[i]
        else:
            replace_name_wapper(reply, replyposs[i], "name", dataposs[i])
            # replyposs[i]=dataposs[i]



for filename in os.listdir(dataPath):
    if not os.path.isdir(filename):
        logger.debug("Dealing with "+filename)
        dataFile = os.path.join(dataPath, filename)
        rawData = json.load(open(dataFile))
        data = rawData[filename[:-5]]

        replyFile = os.path.join(replyPath, filename)
        if not os.path.exists(replyFile):
            continue
        rawReply = json.load(open(replyFile))
        reply = rawReply[filename[:-5]]
        try_adjust(data,reply)
        # dump it.
        parcel = json.dumps(rawData, indent=4, separators=(",", ":"))
        open(dataFile, "w").write(parcel)
        parcel = json.dumps(rawReply, indent=4, separators=(",", ":"))
        open(replyFile, "w").write(parcel)