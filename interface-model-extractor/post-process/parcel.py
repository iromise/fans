from collections import OrderedDict
import json
from json import dumps
import os
import sys
import copy
import logging
import string
import random
from jsonutil import get_formatted_json, replace_attr
from variable_type import special_type,basic_types,get_true_complex_type

logger = logging.getLogger(__name__)

class SerializationType:
    # bit1 : is input
    # bit2 : is output
    COMMON = 0
    INPUT = 1
    OUTPUT = 1 << 1

    @staticmethod
    def is_input(value):
        return value & SerializationType.INPUT

    @staticmethod
    def is_output(value):
        return value & SerializationType.OUTPUT

    @staticmethod
    def is_related_with_parcel(value):
        return SerializationType.is_input(
            value) or SerializationType.is_output(value)


class Parcel:
    def __init__(self, possible_path_cnt):
        self.possible_path_cnt = possible_path_cnt
        self.data = []
        self.reply = []
        self.flags = -1
        self.ret = ""

    def dump(self):
        output = OrderedDict()
        output["id"] = self.possible_path_cnt
        output["data"] = self.data
        output["reply"] = self.reply
        output["return"] = self.ret
        return output


class ParcelManager:
    def __init__(self,type_map):
        self.type_map = type_map
        self.used_types = set()
        # possible path number in one transaction.
        self.possible_path_cnt = 0
        # implicit variable cnt in one transaction.
        # implicit variable does not have name, e.g., ans = func(data.readint())
        self.implicit_variable_cnt = 0

        self.variable = []

        # loop info inside one transaction
        # every loop item is a json.
        self.loop = []
        self.loop_counter = []
        # active loop
        self.active_loop = []

        # constraints info inside one transaction.
        self.constraint = []
        # active constraints
        self.active_constraint = []

        # active parcel when walking one transaction.
        self.active_parcel = []
        # inactive parcel when walking one transaction.
        self.finished_parcel = []

        # init the basic parcel info
        parcel = Parcel(self.possible_path_cnt)
        self.active_parcel.append(parcel)

    def alloc_variable(self):
        """alloc one variable for implicit variable
        Returns:
            string -- implicit variable name
        """
        self.implicit_variable_cnt += 1
        return "implicit_" + str(self.implicit_variable_cnt)

    def add_variable(self, variable):

        var = copy.deepcopy(variable)

        if var["type"] == "FileDescriptor" and "takeOwnership" not in var:
            var["takeOwnership"] = False
        if "string" in var["type"] and "utf8" not in var:
            var["utf8"] = False

        var_name = var["name"]

        if var["type"] in special_type:
            var["type"] = special_type[variable["type"]]

        var_type = var["type"]

        serialization_type = variable["serialization_type"]

        # self constraint
        var["self_constraint"] = []

        # under other's constraint
        var["under_constraint"] = []
        for cons_name in self.active_constraint:
            tmp = OrderedDict()
            tmp["name"] = cons_name
            tmp["status"] = self.get_constraint_attr(cons_name, "status")
            var["under_constraint"].append(tmp)

        var["relation"] = []

        var["loop"] = []
        for i in self.active_loop:
            var["loop"].append(i)

        var["usedBy"] = []
        var["dependency"] = []

        #  how many times this variable exists.
        var["count"] = 1

        if not self.is_variable_exists(var_name):
            self.variable.append(var)
        else:
            existing_count = self.get_variable_attr(var_name, "count")
            existing_serialization_type = self.get_variable_attr(
                var_name, "serialization_type")
            # This variable has already exists,
            # the new variable is not related with data or reply
            # so new variable must be reference to the old variable.
            if not SerializationType.is_related_with_parcel(
                    serialization_type):
                return
            elif SerializationType.is_related_with_parcel(
                    existing_serialization_type):
                self.update_variable_attr(var_name, "count",
                                          existing_count + 1)
                var_name = var_name + '_' + str(existing_count)
                var["name"] = var_name
                self.variable.append(var)
            else:
                # serialization_type != 0 and existing_serialization_type is common.
                # NEW variable is in data or reply
                # but old variable is neither.
                self.update_variable_attr(var_name, "serialization_type",
                                          serialization_type)
                self.update_variable_attr(var_name, "under_constraint",
                                          var["under_constraint"])
                if "IBinder" not in var_type:
                    self.update_variable_attr(var_name, "type", var_type)
                if "size" in variable:
                    self.update_variable_attr(var_name, "size",
                                              variable["size"])
                if "value" in variable:
                    self.update_variable_attr(var_name, "value",
                                              variable["value"])
                if "structSize" in variable:
                    self.update_variable_attr(var_name, "structSize",
                                              variable["structSize"])
                if "takeOwnership" in variable:
                    self.update_variable_attr(var_name, "takeOwnership",
                                              variable["takeOwnership"])
                if "have_parcelable" in variable:
                    self.update_variable_attr(var_name, "have_parcelable",
                                              variable["have_parcelable"])
                if "utf8" in variable:
                    self.update_variable_attr(var_name, "utf8",
                                              variable["utf8"])
        for item in self.active_parcel:
            if SerializationType.is_input(serialization_type):
                item.data.append(var_name)
            if SerializationType.is_output(serialization_type):
                item.reply.append(var_name)

    def set_return_var(self, var_name):
        for item in self.active_parcel:
            item.ret = var_name



    def get_variable_attr(self, var_name, attr):
        """get attr of one variable 
        Arguments:

            var_name {string} -- variable name.

            attr {string} -- the attr want to get.

        Returns:
            [corresponding type] -- if attr exists.
            else return None.
        """
        for item in self.variable:
            if var_name == item["name"]:
                if attr in item:
                    return item[attr]
                else:
                    return None
        return None
    def get_previous_io_variable(self):
        i = 1
        var = OrderedDict()
        while len(self.variable)-i>=0:
            idx = len(self.variable)-i
            if self.variable[idx]["serialization_type"]!=SerializationType.COMMON:
                var["name"] = self.variable[idx]["name"]
                var["type"] = self.variable[idx]["type"]
                if "value" in self.variable[idx]:
                    var["value"] = self.variable[idx]["value"]
                return var
            i+=1
        return None

    def update_variable_attr(self, var_name, attr, new_value):
        if attr == "name":
            if self.is_variable_exists(new_value):
                cnt = self.get_variable_attr(new_value, "count")
                new_value = new_value + "_" + str(cnt + 1)
        if attr == "type":
            if new_value in special_type:
                new_value = special_type[new_value]
        for item in self.variable:
            if item["name"] == var_name:
                item[attr] = new_value
                # if attr==name, we should also update variable in data or reply
                # here we can only update variable in active parcel as
                # no update will cross return statement?
                if attr == "name":
                    for item in self.active_parcel:
                        if var_name in item.data:
                            idx = item.data.index(var_name)
                            item.data[idx] = new_value
                        if var_name in item.reply:
                            idx = item.reply.index(var_name)
                            item.reply[idx] = new_value
                # if attr == "serialization_type" and SerializationType.isReturn(new_value):
                #     for parcel in self.active_parcel:
                #         parcel.ret = var_name
                break
        return new_value

    def is_variable_exists(self, var_name):
        for item in self.variable:
            if item["name"] == var_name:
                return True
        return False

    def add_relation(self, name, relation):
        for item in self.variable:
            if item["name"] == name:
                item["relation"].append(relation)

    def alloc_constraint(self):
        return "constraint" + str(len(self.constraint))

    def add_constraint(self, cons):
        """add constraint

        Arguments:
            cons {json} -- describe a constraint.
        """
        self.constraint.append(cons)
        self.update_self_constraint(cons)

    def update_self_constraint(self, cons):
        logger.debug(get_formatted_json(cons))
        opcode = cons["opcode"]
        if opcode == "!" or opcode == "":
            if "opcode" in cons["lhs"]:
                cons["lhs"]["name"] = cons["name"] + "_lhs"
                self.update_self_constraint(cons["lhs"])
                return
            lhs_name = cons["lhs"]["name"]
            new_cons = self.get_variable_attr(lhs_name, "self_constraint")
            if new_cons is not None:
                new_cons.append(cons["name"])
                self.update_variable_attr(lhs_name, "self_constraint",
                                          new_cons)
        elif opcode == "&&" or opcode == "||" or opcode == "&":
            lhs = cons["lhs"]
            lhs["name"] = cons["name"] + "_lhs"
            lhs["probability"] = cons["probability"]
            rhs = cons["rhs"]
            rhs["name"] = cons["name"] + "_rhs"
            rhs["probability"] = cons["probability"]
            self.update_self_constraint(lhs)
            self.update_self_constraint(rhs)
        else:
            if "opcode" not in cons["lhs"]:
                lhs_name = cons["lhs"]["name"]
                lhs_self_constraint = self.get_variable_attr(
                    lhs_name, "self_constraint")
                if lhs_self_constraint is not None:
                    lhs_self_constraint.append(cons["name"])
                    self.update_variable_attr(lhs_name, "self_constraint",
                                              lhs_self_constraint)
            elif cons["lhs"]["opcode"] == "&" or cons["lhs"]["opcode"] == "-":
                pass
            elif cons["lhs"]["opcode"] == ">=":
                cons["lhs"]["name"] = cons["name"] + "_lhs"
                self.update_self_constraint(cons["lhs"])
            else:
                logger.error("update_self_constraint lhs is not completed.")
                logger.debug(get_formatted_json(cons))
                exit(0)

            if "opcode" not in cons["rhs"]:
                rhsname = cons["rhs"]["name"]
                rhs_self_constraint = self.get_variable_attr(
                    rhsname, "self_constraint")
                if rhs_self_constraint is not None:
                    rhs_self_constraint.append(cons["name"])
                    self.update_variable_attr(rhsname, "self_constraint",
                                              rhs_self_constraint)
            elif cons["rhs"]["opcode"] == "&" or cons["rhs"]["opcode"] == "-":
                pass
            else:
                logger.error("update_self_constraint rhs is not completed.")
                logger.debug(get_formatted_json(cons))
                exit(0)

    def update_single_constraint_attr(self, cons, attr, newvalue):
        cons[attr] = newvalue
        if attr == "probability":
            if cons["opcode"] == "&&" or cons["opcode"] == "||":
                self.update_single_constraint_attr(cons["lhs"], attr, newvalue)
                self.update_single_constraint_attr(cons["rhs"], attr, newvalue)

    def update_constraint_attr(self, name, attr, newvalue):
        for item in self.constraint:
            if item["name"] == name:
                self.update_single_constraint_attr(item, attr, newvalue)

    def get_constraint_attr(self, name, attr):
        for item in self.constraint:
            if item["name"] == name:
                return item[attr]

    def alloc_loop(self):
        return "loop" + str(len(self.loop))

    def add_loop(self, loop_info):
        self.loop.append(loop_info)

    def clear_active(self):
        """When meeting return statement,
        we should move active parcel into finished parcel.
        """
        for item in self.active_parcel:
            self.finished_parcel.append(item)
        self.active_parcel = []

    def backup_active_parcel(self):
        """When deal with if statment, we should backup our
        active parcel.
        Returns:
            [parcel list] -- The old parcel list.
        """

        old_active_parcel = copy.deepcopy(self.active_parcel)
        return old_active_parcel

    def recover_active_parcel(self, parcel):
        """Recover active parcel using given parcel list.
        Arguments:
            old_active_parcel
        """
        self.active_parcel = copy.deepcopy(parcel)
        for item in self.active_parcel:
            self.possible_path_cnt = self.possible_path_cnt + 1
            item.possible_path_cnt = self.possible_path_cnt

    def add_if_parcel(self, if_parcel):
        """May be this function can be done inside if statment"""
        self.active_parcel = if_parcel + self.active_parcel

    def is_array_type(self, var_type):
        if "(*" in var_type:
            # skip function pointer.
            return False
        if "vector<" in var_type or "Vector<" in var_type or "[" in var_type:
            return True
        if "*" in var_type:
            return True
        return False

    def update_variable_type(self, code):
        variable = code["variable"]
        variable_len = len(variable)
        i = 0
        while i < variable_len:
            var = variable[i]
            var_type = var["type"]
            # e.g. ::std::
            var_type = var_type.lstrip("::")
            var_type = var_type.replace("const", "")
            var_type = var_type.replace("&", "")
            var_type = var_type.replace("std::", "")
            var_type = var_type.replace(" (size_t)", "")
            var_type = var_type.replace("__1::","")
            var_type = var_type.strip()

            if "vector<" in var_type or "Vector<" in var_type:
                # make vector type much better
                # "const class std::__1::vector<unsigned char, class std::__1::allocator<unsigned char> >"
                if "allocator" in var_type:
                    var_type = var_type.split(", class")[0] + ">"
                if ", struct" in var_type:
                    var_type = var_type.split(", struct")[0] +">"
                if "class basic_string<char>" in var_type:
                    var_type = var_type.replace(
                        "class basic_string<char>", "string")
                if "allocator" in var_type:
                    var_type = var_type.split(", allocator")[0]+">"
                if "vector<class vector<unsigned char>" == var_type:
                    # TODO.... very special for android::os::IncidentReportArgs
                    var_type="vector<unsigned char>"
                if "class unique_ptr<" in var_type:
                    var_type = var_type.replace("class unique_ptr<","")
                if "class shared_ptr<" in var_type:
                    var_type = var_type.replace("class shared_ptr<","")
            else:
                if var_type == "native_handle_t *" or var_type == "native_handle *":
                    var_type = "struct native_handle"
                if "unique_ptr<" in var_type:
                    # e.g. std::unique_ptr<std::string>
                    var_type = var_type.split("unique_ptr<")[1][:-1]
                    if ", struct" in var_type:
                        var_type = var_type.split(", struct")[0]
                if "sp<" in var_type and "::I" not in var_type:
                    # this should be a simple type, not interface
                    var_type = var_type.split("sp<")[-1][:-1]
                if "NullOr<" in var_type:
                    var_type = var_type.split("NullOr<")[-1][:-1]
                var_type = var_type.strip()
                if var_type == "android::HInterfaceToken":
                    var_type = "vector<uint8_t>"

            if var_type!=get_true_complex_type(self.type_map,var_type):
                var["type_alias"] = 1
                var_type = self.type_map[var_type]
            if "sp<" in var_type and "::I" in var_type:
                if "vector<" in var_type:
                    interface_name = var_type.split("::")[-1][:-2]
                else:
                    interface_name = var_type.split("::")[-1][:-1]
                var["interfaceName"]=interface_name

            var["type"] = var_type
            self.used_types.add(var_type)
            replace_attr(code, var["name"], "type", var_type)

            if var["type"] in [
                    "const S", "S", "android::media::VolumeShaper::S",
                    "android::media::VolumeShaper::T"
            ]:
                replace_attr(code, var["name"], "type", "float")

            if self.is_array_type(var_type):
                item = copy.deepcopy(var)
                item["self_constraint"] = []
                item["under_constraint"] = []
                item["relation"] = []
                item["dependency"] = []
                item["count"] = 1
                item["name"] = var["name"] + "::item"
                if "structSize" in var:
                    item["size"] = var["structSize"]
                    del item["structSize"]

                if "vector<" in var_type or "Vector<" in var_type:
                    var_type = var_type.split("tor<")[-1][:-1]
                    if "size" not in var:
                        self.update_variable_attr(var["name"], "size", -1)
                if "[" in var_type and "[]" not in var_type:
                    tmp = var_type.split("[")
                    var_type = tmp[0].strip()
                    if "size" not in var:
                        self.update_variable_attr(var["name"], "size",
                                                  int(tmp[-1][:-1], 10))
                if "[]" in var_type:
                    tmp = var_type.split("[")
                    var_type = tmp[0].strip()
                if "*" in var_type:
                    var_type = var_type.replace("*", "")
                    if "size" not in var:
                        self.update_variable_attr(var["name"], "size", -1)
                var_type = var_type.strip()
                if var_type!=get_true_complex_type(self.type_map,var_type):
                    var_type = self.type_map[var_type]
                    item["type_alias"] = 1
                item["type"] = var_type
                self.used_types.add(var_type)

                variable.insert(i + 1, item)
                variable_len = len(variable)
            i += 1

    def dump(self):

        code = OrderedDict()

        logging.info("Dump finished parcel")
        code["dependency"] = []

        code["possibility"] = []
        for item in self.finished_parcel:
            code["possibility"].append(item.dump())
        logging.info("Dump Active Parcel")
        for item in self.active_parcel:
            code["possibility"].append(item.dump())
        code["variable"] = self.variable
        code["constraint"] = self.constraint
        for item in code["constraint"]:
            if "status" in item:
                del item["status"]
        code["loop"] = self.loop

        self.update_variable_type(code)
        return code