
from xmljson import BadgerFish
from collections import OrderedDict
import json
import lxml

def get_formatted_json(output):
    return json.dumps(output, indent=4, separators=(",", ":"))

def xml2json(xmlfile):
    bf = BadgerFish(dict_type=OrderedDict)
    return bf.data(lxml.etree.fromstring(open(xmlfile).read()))

def replace_constraint_attr(constraint, varName, attr, new):
    opcode = constraint["opcode"]
    if opcode == "!" or opcode=="":
        lhs = constraint["lhs"]
        if lhs["name"] == varName:
            lhs[attr] = new
    elif opcode == "||" or opcode == "&&":
        lhs = constraint["lhs"]
        replace_constraint_attr(lhs, varName, attr, new)
        rhs = constraint["rhs"]
        replace_constraint_attr(rhs, varName, attr, new)
    else:
        if "opcode" not in constraint["lhs"]:
            lhs = constraint["lhs"]
            if lhs["name"] == varName:
                lhs[attr] = new
        elif constraint["lhs"]["opcode"]=="&" or constraint["lhs"]["opcode"]=="-":
            pass
        elif constraint["lhs"]["opcode"]==">=":
            replace_constraint_attr(constraint["lhs"], varName, attr, new)
        else:
            print("replace_constraint_attr lhs is not completed.")
            print(constraint)
            exit(0)
        if "opcode" not in constraint["rhs"]:
            rhs = constraint["rhs"]
            if rhs["name"] == varName:
                rhs[attr] = new
        elif constraint["rhs"]["opcode"]=="&" or constraint["rhs"]["opcode"]=="-":
            pass
        else:
            print("replace_constraint_attr rhs is not completed.")
            print(constraint)
            exit(0)



def replace_loop_attr(loop, varName, attr, new):
    if loop["counter"]["name"] == varName:
        loop["counter"][attr] = new
    if loop["inc"]["name"] == varName:
        loop["inc"][attr] = new


def replace_var_attr(variable, varName, attr, new):
    if variable["name"] == varName:
        variable[attr] = new


def replace_attr(parcel, varName, attr, new):
    # replace variable attr
    for varible in parcel["variable"]:
        replace_var_attr(varible, varName, attr, new)
    # replace constraint related
    for constraint in parcel["constraint"]:
        replace_constraint_attr(constraint, varName, attr, new)
    # replace loop related
    for loop in parcel["loop"]:
        replace_loop_attr(loop, varName, attr, new)

def replace_name_wapper(parcel, varName, attr, new):
    for poss in parcel["possibility"]:
        for i in range(len(poss["data"])):
            if poss["data"][i]==varName:
                poss["data"][i] = new
        for i in range(len(poss["reply"])):
            if poss["reply"][i]==varName:
                poss["reply"][i]=new
    replace_attr(parcel, varName, attr, new)



def add_qualified_name(data, qualifiedName):
    for key, value in data.items():
        if key in [
                "possibility", "variable", "dependency", "under_constraint"
        ]:
            for c in value:
                add_qualified_name(c, qualifiedName)
        elif key == "constraint":
            if isinstance(value, list):
                for c in value:
                    add_qualified_name(c, qualifiedName)
            else:
                if qualifiedName not in data[key]:
                    data[key] = qualifiedName + "::" + data[key]
        elif key == "loop":
            for i in range(len(value)):
                if isinstance(value[i], dict):
                    add_qualified_name(value[i], qualifiedName)
                else:
                    if qualifiedName not in value[i]:
                        value[i] = qualifiedName + "::" + value[i]
        elif key in ["data", "reply", "self_constraint", "loop_counter"]:
            for i in range(len(value)):
                if qualifiedName not in value[i]:
                    value[i] = qualifiedName + "::" + value[i]
        elif isinstance(value, dict):
            add_qualified_name(value, qualifiedName)
        elif key == "name" or key == "return" or key == "size":
            if isinstance(data[key], str):
                if qualifiedName not in data[key]:
                    data[key] = qualifiedName + "::" + data[key]
