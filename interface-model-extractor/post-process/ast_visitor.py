import logging
from jsonutil import get_formatted_json
from parcel import ParcelManager, SerializationType
from collections import OrderedDict
from variable_type import special_type, basic_types
import copy
import json
import os
import re

logger = logging.getLogger(__name__)
logger.setLevel(logging.ERROR)


class ASTVisitor:

    def __init__(self, type_map):
        self.parcel_manager = ParcelManager(type_map)
        self.type_map = type_map
        self.raw_structure_set = set()
        self.structure_read_write = ""
        self.special_parcelable_function = set()
        self.misc_parcel_related_function = set()

    def set_structure_read_write(self, value):
        self.structure_read_write = value

    def walk_declref(self, declref):
        var = OrderedDict()
        var["name"] = declref["name"]["$"]
        var["type"] = declref["type"]["$"]
        if "argv" in declref:
            var["argv"] = OrderedDict()
            for key, argv in declref["argv"].items():
                tmp = OrderedDict()
                if len(argv["name"]) != 0:
                    tmp["name"] = argv["name"]["$"]
                else:
                    tmp["name"] = ""
                tmp["type"] = argv["type"]["$"]
                var["argv"][key] = tmp
        return var

    def walk_string(self, string):
        var = OrderedDict()
        if len(string) == 1:
            var["name"] = self.parcel_manager.alloc_variable()
            var["value"] = string["$"]
            var["type"] = "string"
        else:
            var["name"] = self.parcel_manager.alloc_variable()
            var["type"] = "string"
            var["value"] = ""
        return var

    def walk_cstyle_cast_expr(self, expr):
        logger.debug("Start walking cstyle cast expr.")
        var = OrderedDict()
        var_type = expr["type"]["$"]
        if len(expr) == 2:
            tmp = OrderedDict()
            key = expr.keys()[-1]
            tmp[key] = expr[key]
            var = self.get_variable_info(tmp)
            if "opcode" in var:
                logger.debug(var)
                var = var["lhs"]
            if var_type in ["long long", "long", "int", "unsigned int", "_Bool"]:
                pass
            elif " bool" in var_type:
                pass
            else:
                var["type"] = self.parcel_manager.update_variable_attr(
                    var["name"], "type", var_type)
        else:
            logger.error("walk_cstyle_cast_expr is not completed (size>2).")
        logger.debug("Finish walking cstyle cast expr.")
        return var

    def walk_gnu_null_expr(self, null):
        var = OrderedDict()
        var["name"] = self.parcel_manager.alloc_variable()
        var["type"] = "void *"
        var["value"] = "NULL"
        return var

    def walk_integerliteral(self, integer):
        var = OrderedDict()
        if "DeclRef" in integer:
            decl_name = integer["DeclRef"]["name"]["$"]
            decl_type = integer["DeclRef"]["type"]["$"]
            var["name"] = decl_name
            var["type"] = decl_type
            var["value"] = integer["value"]["$"]
            return var
        var["name"] = self.parcel_manager.alloc_variable()
        var["type"] = "IntegerLiteral"
        var["value"] = integer["value"]["$"]
        return var

    def walk_floatingliteral(self, flt):
        var = OrderedDict()
        var["name"] = self.parcel_manager.alloc_variable()
        var["type"] = "FloatingLiteral"
        var["value"] = flt["value"]["$"]
        return var

    def walk_memberexpr(self, memberexpr):
        member = OrderedDict()
        if len(memberexpr) == 1:
            # might be asBinder
            if "DeclRef" in memberexpr:
                member = self.walk_declref(memberexpr["DeclRef"])
                member["signature"] = member["type"]
            else:
                logger.error("MemberExpr is not completed when len = 1.")
                exit(0)
        elif len(memberexpr) == 2:
            member["name"] = memberexpr["name"]["$"]
            member["type"] = memberexpr["type"]["$"]
            if member["type"] == "<dependent type>":
                if member["name"] == "size":
                    member["type"] = "int"
                elif member["name"] in ["first", "second"]:
                    member["type"] = "float"
                else:
                    logger.error("<dependent type> is not completed, meeting %s." %
                                 member["name"])
                    exit(0)
            else:
                logger.error("MemberExpr is not completed when len = 2.")
                exit(0)
        elif len(memberexpr) == 3:
            member["name"] = memberexpr['name']['$']
            member["type"] = memberexpr['type']['$']
            member["signature"] = memberexpr["signature"]["$"]
        else:
            member["name"] = memberexpr['name']['$']
            member["type"] = memberexpr['type']['$'].replace(
                "const", "").strip()
            member["signature"] = memberexpr["signature"]["$"]
            if "DeclRef" in memberexpr:
                cls_var = self.walk_declref(memberexpr["DeclRef"])
                if member["name"] == "c_str":
                    # this is special
                    member["type"] = cls_var["type"]
                member["name"] = cls_var["name"] + "." + member["name"]
            elif "MemberExpr" in memberexpr:
                cls_var = self.walk_memberexpr(memberexpr["MemberExpr"])
                if member["name"] == "c_str":
                    # this is special
                    member["type"] = cls_var["type"]
                member["name"] = cls_var["name"] + "." + member["name"]
            elif "CXXOperatorCallExpr" in memberexpr:
                cls_var = self.walk_cxxoperatorcallexpr(
                    memberexpr["CXXOperatorCallExpr"])
                if "->" not in cls_var["name"]:
                    member["name"] = cls_var["name"] + "." + member["name"]
                else:
                    member["name"] = cls_var["name"] + member["name"]
            elif "CXXMemberCallExpr" in memberexpr:
                cls_var = self.walk_call(memberexpr["CXXMemberCallExpr"])
                member["name"] = cls_var["name"] + "." + member["name"]
            elif "CXXThisExpr" in memberexpr:
                pass
            elif "CXXConstructExpr" in memberexpr:
                cls_var = self.walk_cxxconstructexpr(
                    memberexpr["CXXConstructExpr"])
                member["type"] = cls_var["type"]
                member["name"] = cls_var["name"] + "." + member["name"]
            elif "ImplicitCastExpr" in memberexpr:
                cls_var = self.walk_implicitcastexpr(
                    memberexpr["ImplicitCastExpr"])
                # member["type"] = cls_var["type"]
                member["name"] = cls_var["name"] + "." + member["name"]
            elif "ArraySubscriptExpr" in memberexpr:
                cls_var = self.walk_array_subscript_expr(
                    memberexpr["ArraySubscriptExpr"])
                member["name"] = cls_var["name"] + "." + member["name"]
            elif "UnaryOperator" in memberexpr:
                # TODO: not considering opcode
                # http://androidxref.com/9.0.0_r3/xref/frameworks/native/libs/input/Input.cpp#505
                cls_var = self.walk_unaryoperator(memberexpr["UnaryOperator"])
                cls_var = cls_var["lhs"]

                member["name"] = cls_var["name"] + "." + member["name"]
            else:
                logger.error("MemberExpr is not completed when len=4.")
                exit(0)
        return member

    def walk_unaryoperator(self, unaryoperator):
        opcode = unaryoperator["opcode"]["$"]
        unary = OrderedDict()
        if len(unaryoperator) == 1:
            var = OrderedDict()
            var["name"] = self.parcel_manager.alloc_variable()
            if opcode == "*":
                var["type"] = "this"
            else:
                logger.error("walk_unaryoperator is not completed.")
                exit(0)
            return opcode, var
        key = unaryoperator.keys()[1]
        if key.startswith("CXXOperatorCallExpr"):
            tmp = self.walk_cxxoperatorcallexpr(
                unaryoperator["CXXOperatorCallExpr"])
            if isinstance(tmp, dict):
                var = tmp
            else:
                logger.error(
                    "Unexpected when dealing CXXOperatorCallExpr in walk unaryoperator."
                )
                exit(0)
        else:
            op = OrderedDict()
            op[key] = unaryoperator[key]
            var = self.get_variable_info(op)

        unary["opcode"] = opcode
        if opcode == "*":
            var["type"] = var["type"].strip("*")
        elif opcode=="-":
            if var["type"]=="IntegerLiteral":
                var["value"]= -var["value"]
        unary["lhs"] = var
        return unary

    def walk_unary_expr_or_type_trait_expr(self, expr):
        var = OrderedDict()
        if "DeclRef" in expr:
            var["name"] = expr['op']["$"] + \
                '(' + expr["DeclRef"]["name"]['$'] + ')'
            if expr['op']["$"] == "sizeof":
                var["type"] = "unsigned int"
            else:
                var["type"] = expr["DeclRef"]["type"]["$"]
        elif "ArgType" in expr:
            var["name"] = expr['op']["$"] + '(' + expr["ArgType"]['$'] + ')'
            var["type"] = "unsigned int"
        elif "value" in expr:
            var["name"] = self.parcel_manager.alloc_variable()
            var["type"] = expr["type"]["$"]
            var["value"] = expr["value"]["$"]
            var["sizeof"] = expr["sizeof"]["$"]
        else:
            logger.error(get_formatted_json(expr))
            logger.error("Walk UnaryExprOrTypeTraitExpr is not completed!")
            exit(0)
        return var

    def walk_cxxthisexpr(self, expr):
        var = OrderedDict()
        var["name"] = self.parcel_manager.alloc_variable()
        var["type"] = expr["Type"]["$"].strip("*").strip()
        return var

    def get_simplified_type(self, var_type):
        if "::size_type" in var_type:
            return "size_t"
        if var_type == "const int &(*)(size_t) const":
            return "int"
        var_type = var_type.replace("const", "")
        var_type = var_type.replace("&", "")
        var_type = var_type.replace("std::", "")
        var_type = var_type.replace(" (size_t)", "")
        var_type = var_type.strip()
        return var_type

    def walk_implicitcastexpr(self, expr):
        kind = expr["type"]["$"]
        if len(expr) < 2:
            var = OrderedDict()
            var["type"] = kind
            var["name"] = self.parcel_manager.alloc_variable()
            return var
        key = expr.keys()[1]
        tmp = OrderedDict()
        tmp[key] = expr[key]
        var = self.get_variable_info(tmp)
        if isinstance(var, tuple):
            # input("implicit cast expr tuple")
            return var
        logger.debug(get_formatted_json(var))

        if "type" in var:
            simplified_var_type = self.get_simplified_type(var["type"])
            if kind in basic_types and kind != "_Bool":
                if simplified_var_type not in basic_types:
                    # print(get_formatted_json(var))
                    # print(get_formatted_json(expr))
                    return var

        if "IBinder" not in kind and \
            "void *" not in kind and \
            "android::Parcelable" not in kind and \
            "android::IInterface" not in kind and \
            "type" in var and \
            "IntegerLiteral" != var["type"] and \
            "class" not in var["type"] and \
                "struct" not in var["type"] and \
                    "[" not in var["type"]:
            var["type"] = kind
        return var

    def walk_array_subscript_expr(self, expr):
        lhs = self.get_variable_info(expr["LHS"])
        rhs = self.get_variable_info(expr["RHS"])
        var = OrderedDict()
        var["name"] = lhs["name"] + "[" + rhs["name"] + "]"
        tmp = lhs["type"]
        if "[" in tmp:
            tmp = tmp.split("[")[0].strip()
        if "*" in tmp:
            tmp =tmp.split("*")[0].strip()
        var["type"] = tmp
        return var

    def get_variable_info(self, variable):
        var = OrderedDict()
        key = variable.keys()[0]
        if key.startswith("String") and len(variable) == 1:
            var = self.walk_string(variable[key])
        elif key.startswith("DeclRef"):
            var = self.walk_declref(variable[key])
        elif key.startswith("GNUNullExpr"):
            var = self.walk_gnu_null_expr(variable[key])
        elif key.startswith("IntegerLiteral"):
            var = self.walk_integerliteral(variable[key])
        elif key.startswith("MemberExpr"):
            var = self.walk_memberexpr(variable[key])
            var["name"] = var["name"].replace("->.","->")
            delimiters=[".","->"]
            regexPattern = '|'.join(map(re.escape, delimiters))
            tmp = re.split(regexPattern,var["name"])
            new_name = ""
            curr = ""
            for item in tmp:
                if item=="":
                    continue
                curr = curr+item
                delimiter = ""
                if len(curr)<len(var["name"]):
                    if var["name"][len(curr)]=="-":
                        delimiter="->"
                    elif var["name"][len(curr)]==".":
                        delimiter="."
                    else:
                        logger.error("Strange deimiter meeted.")
                        print(var["name"])
                        exit(0)
                new_name += item.split("::")[-1]+delimiter
                curr+=delimiter
            var["name"] = new_name
        elif key.startswith("CXXMemberCallExpr") or key.startswith("CallExpr"):
            var = self.walk_call(variable[key])
        elif key.startswith("CStyleCastExpr"):
            var = self.walk_cstyle_cast_expr(variable[key])
        elif key.startswith("NULL"):
            var["name"] = self.parcel_manager.alloc_variable()
            var["type"] = "void *"
            var["value"] = "NULL"
        elif key.startswith("True"):
            var["name"] = self.parcel_manager.alloc_variable()
            var["value"] = True
            var["type"] = "_Bool"
        elif key.startswith("False"):
            var["name"] = self.parcel_manager.alloc_variable()
            var["value"] = False
            var["type"] = "_Bool"
        elif key.startswith("CXXThisExpr"):
            var = self.walk_cxxthisexpr(variable[key])
        elif key.startswith("ImplicitCastExpr"):
            var = self.walk_implicitcastexpr(variable[key])
        elif key.startswith("ConditionalOperator"):
            cons, op1, op2 = self.walk_conditional_operator(variable[key])
            var["name"] = self.parcel_manager.alloc_variable()
            var["type"] = "int"
        elif key.startswith("BinaryConditionalOperator"):
            var = self.walk_binary_conditional_operator(variable[key])
        elif key.startswith("CXXOperatorCallExpr"):
            var = self.walk_cxxoperatorcallexpr(variable[key])
        elif key.startswith("BinaryOperator"):
            var = self.walk_binaryoperator(variable[key])
        elif key.startswith("CXXConstructExpr"):
            # TODO: this should be improved, maybe?
            construct = variable[key]
            var = self.walk_cxxconstructexpr(construct)
        elif key.startswith("ArraySubscriptExpr"):
            var = self.walk_array_subscript_expr(variable[key])
        elif key.startswith("UnaryOperator"):
            var = self.walk_unaryoperator(variable[key])
        elif key.startswith("FloatingLiteral"):
            var = self.walk_floatingliteral(variable[key])
        elif key.startswith("UnaryExprOrTypeTraitExpr"):
            var = self.walk_unary_expr_or_type_trait_expr(variable[key])
        elif key.startswith("CXXStaticCastExpr"):
            var = self.walk_cxx_static_cast_expr(variable[key])
        else:
            logger.error("Get Variable Info is not completed.")
            logger.error(get_formatted_json(variable))
            exit(0)
        return var

    def walk_cxxoperatorcallexpr(self, expr):
        logger.debug("Start walking CXXOperatorCallExpr.")
        if "android::base::LogMessage:" in str(expr):
            var = OrderedDict()
            var["name"] = self.parcel_manager.alloc_variable()
            var["type"] = "int"
            return var
        item = []
        for key, value in expr.items():
            tmp = OrderedDict()
            tmp[key] = value
            item.append(tmp)

        # extract opcode
        if "DeclRef1" in item[0]:
            opcode = item[0]["DeclRef1"]["name"]["$"].split("::")[-1]
        elif "DeclRef" in item[0]:
            opcode = item[0]["DeclRef"]["name"]["$"].split("::")[-1]
        elif "ImplicitCastExpr1" in item[0]:
            # http://androidxref.com/9.0.0_r3/xref/system/connectivity/wificond/scanning/single_scan_settings.cpp#38
            # TODO:
            if "android::base::LogSeverity" in item[0]["ImplicitCastExpr1"][
                    "type"]["$"]:
                var = OrderedDict()
                var["name"] = self.parcel_manager.alloc_variable()
                var["type"] = "int"
                return var
            else:
                opcode = item[0]["ImplicitCastExpr1"]["DeclRef"]["name"][
                    "$"].split("::")[-1]
        elif "ImplicitCastExpr" in item[0]:
            opcode = item[0]["ImplicitCastExpr"]["DeclRef"]["name"]["$"].split(
                "::")[-1]
        else:
            logger.error(
                "Walk CXXOperatorCallExpr is not completed when extracting opcode.")
            exit(0)

        opcode = opcode.split("::")[-1]

        if opcode == "operator[]":
            var0 = OrderedDict()
            var0 = self.get_variable_info(item[0])

            var1 = OrderedDict()
            var1 = self.get_variable_info(item[1])

            var2 = OrderedDict()
            var2 = self.get_variable_info(item[2])

            var = OrderedDict()
            var["name"] = var1["name"] + "[{}]".format(var2["name"])
            if var0["type"].endswith("&(size_t) const"):
                var["type"] = var0["type"].replace(" &(size_t) const","")
            elif var0["type"].endswith("&(*)(size_t) const"):
                var["type"] = var0["type"].replace(" &(*)(size_t) const","")
            elif var0["type"].endswith("::size_type)"):
                var["type"] = var0["type"].split("::reference")[0]
                var["type"] = var["type"].split(", class")[0][len("std::__1::vector<"):]
                if "vector<" in var["type"]:
                    var["type"]+=">"
            elif var0["type"].endswith(":size_type) const"):
                var["type"] = var0["type"].split("::const_reference")[0]
                var["type"] = var["type"].split(", class")[0][len("std::__1::vector<"):]
                if "vector<" in var["type"]:
                    var["type"]+=">"
            else:
                # we should give a more clear var type.
                # print(var0["type"])
                # raw_input()
                var["type"] = var0["type"]
            return var
        elif opcode == "operator*":
            var = OrderedDict()
            var = self.get_variable_info(item[1])
            return var
        elif opcode == "operator->":
            var = self.get_variable_info(item[1])
            var["name"] = var["name"] + "->"
            return var
        elif opcode == "operator++":
            var = self.get_variable_info(item[1])
            lhs = OrderedDict()
            lhs["opcode"] = "++"
            lhs["lhs"] = var
            return lhs
        elif opcode == "operator==" or opcode == "operator!=" or opcode == "operator=":
            # deal with left hand side
            left = item[1]
            lhs = OrderedDict()
            lhs = self.get_variable_info(left)

            # deal with right hand side
            right = item[2]
            rhs = OrderedDict()
            rhs = self.get_variable_info(right)
            logger.debug(lhs)
            logger.debug(rhs)
            if opcode == "operator=":
                if lhs["type"]=="class keystore::KeyStoreServiceReturnCode":
                    return rhs
                else:
                    self.update_var_assgin(lhs, rhs)
                    return lhs
            else:
                return opcode, lhs, rhs

        else:
            logger.error("Walk CXXOperatorCallExpr is not completed.")
            logger.error(get_formatted_json(expr))
            exit(0)
        logger.debug("Finish walking CXXOperatorCallExpr.")

    def walk_cxx_static_cast_expr(self, expr):
        logger.debug("Start walking CXXStaticCastExpr.")

        kind = expr["type"]["$"]
        key = expr.keys()[1]
        tmp = OrderedDict()
        tmp[key] = expr[key]
        var = self.get_variable_info(tmp)
        if "type" in var:
            simplified_var_type = self.get_simplified_type(var["type"])
            if kind in basic_types and kind != "_Bool":
                if simplified_var_type not in basic_types:
                    logger.debug("Finish walking CXXStaticCastExpr.")
                    return var
        if "IBinder" not in kind and \
            "void *" not in kind and \
            "android::Parcelable" not in kind and \
            "android::IInterface" not in kind and \
            kind!="_Bool" and \
            kind!="unsigned char" and \
            "size_t" not in kind and \
            "type" in var and \
            "IntegerLiteral" != var["type"] and \
            "class" not in var["type"] and \
                "struct" not in var["type"]:
            var["type"] = kind
            if self.parcel_manager.is_variable_exists(var["name"]):
                self.parcel_manager.update_variable_attr(var["name"], "type", var["type"])
        logger.debug("Finish walking CXXStaticCastExpr.")
        return var

    def argv_cond(self, cond):
        var = OrderedDict()
        op, lhs, rhs = cond["opcode"], cond["lhs"], cond["rhs"]
        constraint = OrderedDict()
        constraint["name"] = self.parcel_manager.alloc_constraint()
        constraint["opcode"] = op
        constraint["lhs"] = lhs
        constraint["rhs"] = rhs
        constraint["probability"] = 0.5
        constraint["status"] = True
        self.parcel_manager.add_constraint(constraint)

        self.parcel_manager.active_constraint.append(constraint["name"])

        # True
        op1 = OrderedDict()
        op1["name"] = self.parcel_manager.alloc_variable()
        op1["type"] = "IntegerLiteral"
        op1["value"] = 1

        # False
        op2 = OrderedDict()
        op2["name"] = self.parcel_manager.alloc_variable()
        op2["type"] = "IntegerLiteral"
        op2["value"] = 0

        var["True"] = op1
        var["False"] = op2

        return var

    def walk_argv(self, argvs):
        logger.debug("Start walking argv.")
        argv_list = []
        for key, argv in argvs.items():
            var = OrderedDict()
            if key.startswith("UnaryOperator"):
                var = self.walk_unaryoperator(argv)
                var = var["lhs"]
            elif key.startswith("CXXOperatorCallExpr"):
                tmp = self.walk_cxxoperatorcallexpr(argv)
                if isinstance(tmp, dict):
                    var = tmp
                else:
                    if tmp[0] == "operator!=":
                        cond = OrderedDict()
                        cond["opcode"] = tmp[0]
                        cond["lhs"] = tmp[1]
                        cond["rhs"] = tmp[2]
                        var = self.argv_cond(cond)
                    else:
                        logger.error(tmp)
                        logger.error(
                            "Walk argv is not completed when dealing with CXXOperatorCallExpr."
                        )
                        exit(0)
            elif key == "BinaryOperator":
                # TODO: to be improved
                bop = self.walk_binaryoperator(argv)
                if "opcode" in bop:
                    opcode = bop["opcode"]
                    if opcode == "==" or opcode == "!=" or opcode == ">=" or opcode == ">":
                        var = self.argv_cond(bop)
                    elif opcode in ["&", "|"]:
                        var = OrderedDict()
                        var["name"] = self.parcel_manager.alloc_variable()
                        var["type"] = "int"
                        var["relation"] = bop
                    else:
                        logger.error(
                            "Binary Operator in walk argv is not completed.")
                        logger.error(get_formatted_json(argv))
                        exit(0)
                else:
                    var = bop
            elif key == "ConditionalOperator":
                constraint, op1, op2 = self.walk_conditional_operator(argv)
                var["True"] = op1
                var["False"] = op2
                constraint["name"] = self.parcel_manager.alloc_constraint()
                self.parcel_manager.add_constraint(constraint)
                self.parcel_manager.active_constraint.append(
                    constraint["name"])
            elif key == "CStyleCastExpr":
                var = self.walk_cstyle_cast_expr(argv)
            elif key == "CXXStaticCastExpr":
                var = self.walk_cxx_static_cast_expr(argv)
            elif key.startswith("ImplicitCastExpr"):
                var = self.walk_implicitcastexpr(argv)
                if isinstance(var, tuple):
                    cond = OrderedDict()
                    cond["opcode"] = var[0]
                    cond["lhs"] = var[1]
                    cond["rhs"] = var[2]
                    var = self.argv_cond(cond)
                elif "opcode" in var:
                    logger.debug(get_formatted_json(var))
                    if var["opcode"] == "&":
                        logger.debug(get_formatted_json(var))
                        var = var["lhs"]
                    elif var["opcode"] == "*":
                        var = var["lhs"]
                        var["type"] = var["type"].strip("*")
                    else:
                        var = self.argv_cond(var)
            else:
                info = OrderedDict()
                info[key] = argv
                var = self.get_variable_info(info)

            argv_list.append(var)
        logger.debug("Finish walking argv.")
        return argv_list

    def update_special_argv_type(self, argv):
        tmp = argv["type"]

        if tmp.startswith("const "):
            tmp = tmp[6:]

        if tmp in special_type:
            argv["type"] = special_type[tmp]
            tmp = argv["type"]

        true_argv_type = tmp
        while true_argv_type in self.type_map:
            true_argv_type = self.type_map[true_argv_type]
        if true_argv_type != tmp:
            if "struct" in true_argv_type or "union" in true_argv_type:
                self.raw_structure_set.add(tmp)
        if "struct" in tmp or "union" in tmp:
            tmp = tmp.replace("const", "")
            tmp = tmp.replace("*", "")
            tmp = tmp.replace("[]", "")
            tmp = tmp.split("[")[0]
            tmp = tmp.strip()
            self.raw_structure_set.add(tmp)

        return tmp

    def add_argv(self, var, argv):
        var["argv"] = []
        for item in argv:
            tmp = OrderedDict()
            tmp["name"] = item["name"]
            tmp["type"] = item["type"]
            var["argv"].append(tmp)
            if "android::Parcel" not in item["type"]:
                item["serialization_type"] = SerializationType.COMMON
                self.parcel_manager.add_variable(item)

    def walk_call(self, call):

        logger.debug("Start walking call.")

        # the call's function name
        qualified_function_name = ""
        func = OrderedDict()

        return_var = OrderedDict()

        var = OrderedDict()

        # is this a parcel call, e.g., parcel.readxxx()
        # by default, we think it is not.
        is_parcel_call = 0

        # variable in data or reply or others?
        serialization_type = SerializationType.COMMON

        # is data or reply inside argv
        is_argv_related_with_parcel = False

        # step 1. get call's basic infomation
        # e.g., function name, serialization_type, var_name(if exists)
        for key, value in call.items():
            if key=="call":
                raw_input("call meeted?") # maybe we should remove them
            elif key == "MemberExpr":
                func = self.walk_memberexpr(value)
                if "android::Parcel::" in func["name"]:
                    is_parcel_call = 1
                if "android::Parcel::read" in func["name"]:
                    serialization_type |= SerializationType.INPUT
                if "android::Parcel::write" in func["name"]:
                    serialization_type |= SerializationType.OUTPUT

                if return_var["type"] == "<dependent type>":
                    # for http://androidxref.com/9.0.0_r3/xref/frameworks/av/include/media/Interpolator.h#245
                    if func["name"] in ["size", "android::Interpolator::setInterpolatorType"]:
                        return_var["type"] = "unsigned int"
                    else:
                        if func["name"] == "parcel":
                            func["name"] = "missing_funcname"
                        is_parcel_call = 1
                        if "readFromParcel" in self.structure_read_write:
                            serialization_type |= SerializationType.INPUT
                        if "writeToParcel" in self.structure_read_write:
                            serialization_type |= SerializationType.OUTPUT
                        return_var["type"] = "int"
                if func["name"].endswith("::get"):
                    # for function with a.get()
                    qualified_function_name = func["name"].split(".")[0]
                elif "." in func["name"]:
                    qualified_function_name = func["name"].split(".")[-1]
                elif "->" in func["name"]:
                    qualified_function_name = func["name"].split("->")[-1]
                else:
                    qualified_function_name = func["name"]

                func["name"] = qualified_function_name
            elif key == "argv":
                argv = self.walk_argv(value)
                for item in argv:
                    # special for ConditionalOperator
                    # TODO: Parcel might in ConditionalOperator
                    if "True" in item:
                        continue
                    if "android::Parcel" not in item["type"]:
                        continue
                    is_argv_related_with_parcel = True
            elif key == "ReturnType":
                return_var["type"] = value["$"]
            else:
                logging.error("unpected key meeted when walking call: "+str(key))
        logging.info("function name     : " + str(func["name"]))
        logging.info("argument list: " + str(argv))

        # step 2. deal with the function.
        # input("deal with "+qualified_function_name)
        if "::" in qualified_function_name:
            non_qualified_function_name = qualified_function_name.split(
                "::")[-1]
        else:
            non_qualified_function_name = qualified_function_name

        if is_parcel_call == 1:
            if len(argv) == 0:
                if non_qualified_function_name == "dataSize":
                    serialization_type = SerializationType.COMMON
                    return_var["name"] = "dataSize"
                elif non_qualified_function_name == "dataAvail":
                    serialization_type = SerializationType.COMMON
                    return_var["name"] = "dataAvail"
                elif non_qualified_function_name == "readFileDescriptor":
                    return_var["name"] = self.parcel_manager.alloc_variable()
                    return_var["type"] = "FileDescriptor"
                elif non_qualified_function_name == "writeNoException" or non_qualified_function_name == "readExceptionCode":
                    return_var["name"] = self.parcel_manager.alloc_variable()
                    return_var["type"] = "class android::binder::Status"
                    
                    status = OrderedDict()
                    status["name"] = "mException"
                    status["type"] = "int"
                    status["serialization_type"] = SerializationType.COMMON
                    self.parcel_manager.add_variable(status)

                elif non_qualified_function_name == "missing_funcname":
                    raw_input("meet missing_funcname when len(argv)=0")
                    exit(0)
                else:
                    return_var["name"] = self.parcel_manager.alloc_variable()
                if non_qualified_function_name == "readNativeHandle":
                    return_var["type"] = return_var["type"].strip("*").strip()
                return_var["serialization_type"] = serialization_type
                self.parcel_manager.add_variable(return_var)
            elif len(argv) == 1:
                if non_qualified_function_name == "checkInterface":
                    var["name"] = "ServiceInterfaceDescriptor"
                    var["type"] = "class android::String16"
                    var["serialization_type"] = 1
                    self.parcel_manager.add_variable(var)
                    return_var = var
                elif non_qualified_function_name == "readInplace":
                    var["name"] = self.parcel_manager.alloc_variable()
                    var["type"] = "void *"  # return_var["type"]
                    var["serialization_type"] = serialization_type
                    if "value" in argv[0]:
                        var["size"] = argv[0]["value"]
                        var["type"] = argv[0]["sizeof"]
                        self.raw_structure_set.add(var["type"])
                    elif argv[0]["name"] not in self.parcel_manager.active_parcel[
                            -1].data:
                        # gussing size....
                        var["size"] = self.parcel_manager.active_parcel[-1].data[-1]
                    else:
                        var["size"] = argv[0]["name"]
                    self.parcel_manager.add_variable(var)
                    return_var["name"] = var["name"]
                elif non_qualified_function_name == "writeInplace":
                    var_name = self.parcel_manager.alloc_variable()
                    tmp_name = argv[0]["name"]
                    if "sizeof" in tmp_name:
                        var_type = tmp_name[7:-1]
                    else:
                        var_type = "void *"
                    var = OrderedDict()
                    var["name"] = var_name
                    var["type"] = var_type
                    var["serialization_type"] = serialization_type
                    self.parcel_manager.add_variable(var)
                    return_var["name"] = var["name"]
                elif non_qualified_function_name == "readString16Inplace":
                    var["name"] = self.parcel_manager.alloc_variable()
                    var["type"] = "char16_t *"
                    var["serialization_type"] = serialization_type
                    return_var["type"] = "char16_t *"
                    return_var["name"] = var["name"]
                    self.parcel_manager.add_variable(var)
                elif non_qualified_function_name == "writeDupFileDescriptor":
                    argv[0]["type"] = "FileDescriptor"
                    argv[0]["serialization_type"] = serialization_type
                    self.parcel_manager.add_variable(argv[0])
                elif non_qualified_function_name == "read" or non_qualified_function_name == "write":
                    # special for LightFlattenable and Flattenable variable.
                    argv[0]["type"] = argv[0]["type"].strip("*")
                    argv[0]["serialization_type"] = serialization_type
                    self.parcel_manager.add_variable(argv[0])
                elif non_qualified_function_name == "readParcelable" or non_qualified_function_name == "writeParcelable":
                    # http://androidxref.com/9.0.0_r3/xref/frameworks/native/libs/binder/Parcel.cpp#2157
                    argv[0]["have_parcelable"] = 1
                    argv[0]["serialization_type"] = serialization_type
                    argv[0]["type"] = argv[0]["type"].strip("*")
                    self.parcel_manager.add_variable(argv[0])
                elif non_qualified_function_name == "readParcelableVector" or non_qualified_function_name == "writeParcelableVector":
                    argv[0]["have_parcelable"] = 1
                    argv[0]["serialization_type"] = serialization_type
                    self.parcel_manager.add_variable(argv[0])
                elif non_qualified_function_name == "writeUtf8VectorAsUtf16Vector" or non_qualified_function_name == "readUtf8VectorFromUtf16Vector":
                    argv[0]["utf8"] = True
                    argv[0]["serialization_type"] = serialization_type
                    self.parcel_manager.add_variable(argv[0])
                elif non_qualified_function_name == "writeNullableParcelable" or non_qualified_function_name == "readNullableStrongBinder":
                    argv[0]["serialization_type"] = serialization_type
                    self.parcel_manager.add_variable(argv[0])
                elif non_qualified_function_name == "writeUtf8AsUtf16" or non_qualified_function_name == "readUtf8FromUtf16":
                    argv[0]["utf8"] = True
                    argv[0]["serialization_type"] = serialization_type
                    self.parcel_manager.add_variable(argv[0])
                elif non_qualified_function_name == "writeUniqueFileDescriptor" or non_qualified_function_name == "readUniqueFileDescriptor":
                    argv[0]["takeOwnership"] = True
                    argv[0]["serialization_type"] = serialization_type
                    argv[0]["type"] = "FileDescriptor"
                    self.parcel_manager.add_variable(argv[0])
                elif non_qualified_function_name == "resizeOutVector":
                    var = OrderedDict()
                    var["name"] = self.parcel_manager.alloc_variable() + \
                        "_vector_size"
                    var["serialization_type"] = SerializationType.INPUT
                    var["type"] = "int"
                    self.parcel_manager.add_variable(var)
                elif non_qualified_function_name == "enforceInterface":
                    argv[0]["name"] = "ServiceInterfaceDescriptor"
                    argv[0]["type"] = "class android::String16"
                    argv[0]["serialization_type"] = SerializationType.INPUT
                    self.parcel_manager.add_variable(argv[0])
                    return_var = argv[0]
                elif non_qualified_function_name == "missing_funcname":
                    logger.debug(get_formatted_json(argv[0]))
                    if "mFirstSlope" in argv[0]["name"] or "mLastSlope" in argv[0]["name"]:
                        argv[0]["type"] = "float"
                    else:
                        logger.error("unexpected missing_funcname %s meeted." % argv[0]["name"])
                        exit(0)
                    argv[0]["serialization_type"] = serialization_type
                    self.parcel_manager.add_variable(argv[0])
                else:
                    # special for ConditionalOperator and part CXXOperatorCallExpr
                    if "True" in argv[0]:
                        op1 = argv[0]["True"]
                        op1["serialization_type"] = serialization_type
                        self.parcel_manager.add_variable(op1)

                        # deal with the False branch
                        self.parcel_manager.update_constraint_attr(
                            self.parcel_manager.active_constraint[-1], "status", False)
                        op2 = argv[0]["False"]
                        op2["serialization_type"] = serialization_type
                        self.parcel_manager.add_variable(op2)
                        # pop out the constraint
                        self.parcel_manager.active_constraint.pop()
                    else:
                        argv[0]["serialization_type"] = serialization_type
                        if non_qualified_function_name in [
                                "readInt32", "writeNativeHandle"
                        ]:
                            argv[0]["type"] = argv[0]["type"].strip(
                                "*").strip()
                        self.parcel_manager.add_variable(argv[0])
            elif len(argv) == 2:
                if non_qualified_function_name == "write" or non_qualified_function_name == "read":
                    argv[0]["type"] = self.update_special_argv_type(argv[0])
                    if "value" in argv[1]:
                        if "*" in str(argv[1]["value"]):
                            argv1value = argv[1]["value"]
                            left, right = argv1value.split("*")
                            # structSize
                            if self.parcel_manager.get_variable_attr(left,"sizeof") is not None:
                                argv[0]["structSize"] = left
                            else:
                                argv[0]["structSize"] = right
                            # size
                            if self.parcel_manager.get_variable_attr(left,"type")=="IntegerLiteral" and self.parcel_manager.get_variable_attr(right,"type")=="IntegerLiteral":
                                if argv[0]["structSize"]==left:
                                    argv[0]["size"]=right
                                else:
                                    argv[0]["size"]=left
                            elif serialization_type == SerializationType.OUTPUT:
                                i = -1
                                while True:
                                    var_name = self.parcel_manager.active_parcel[-1].reply[i]
                                    var_type = self.parcel_manager.get_variable_attr(var_name, "type")
                                    if var_type in ["size_t","IntegerLiteral","int64_t","long","long long","unsigned int","int","int32_t","uint32_t","ssize_t","unsigned long"]:
                                        break
                                    else:
                                        # print(var_type)
                                        # raw_input("strange type")
                                        i -=1
                                argv[0]["size"] = self.parcel_manager.active_parcel[-1].reply[i]
                            else:
                                if self.parcel_manager.get_variable_attr(left,"sizeof") is not None:
                                    argv[0]["size"] = right
                                else:
                                    argv[0]["size"] = left
                            if "[" not in argv[0]["type"]:
                                argv[0]["type"] = argv[0]["type"].strip(
                                    "* ") + '[' + argv[0]["size"] + ']'
                            else:
                                argv[0]["type"] = argv[0]["type"].split("[")[0].strip()+'[' + argv[0]["size"] + ']'
                        elif "IntegerLiteral" == argv[1]["type"]:
                            if "[" in argv[1]["sizeof"]:
                                # it is a constant array?
                                size = int(argv[1]["sizeof"].split("[")[-1][:-1],
                                           10)
                                argv[0]["size"] = size
                                argv[0]["structSize"] = argv[1]["value"] / size
                            else:
                                argv[0]["size"] = 1
                                argv[0]["structSize"] = argv[1]["value"]
                        else:
                            argv[0]["size"] = argv[1]["value"]
                    else:
                        varCnt = self.parcel_manager.get_variable_attr(argv[1]["name"], "count")
                        if varCnt is None or varCnt == 1:
                            argv[0]["size"] = argv[1]["name"]
                        else:
                            argv[0]["size"] = argv[1]["name"]+ '_' + str(varCnt - 1)
                    argv[0]["serialization_type"] = serialization_type
                    self.parcel_manager.add_variable(argv[0])
                    if "serialization_type" not in argv[1]:
                        argv[1]["serialization_type"] = SerializationType.COMMON
                    self.parcel_manager.add_variable(argv[1])
                elif non_qualified_function_name == "writeByteArray":
                    argv[1]["size"] = argv[0]["name"]
                    argv[1]["serialization_type"] = serialization_type
                    if "value" in argv[1]:
                        del argv[1]["value"]
                    logger.debug(get_formatted_json(argv[0]))
                    logger.debug(get_formatted_json(argv[1]))
                    argv[1]["type"] = "char ["+argv[1]["size"]+"]"
                    self.parcel_manager.add_variable(argv[1])
                elif non_qualified_function_name == "readBlob":
                    var = argv[1]
                    var["serialization_type"] = serialization_type
                    tmp = self.parcel_manager.get_previous_io_variable()
                    if tmp == None:
                        raw_input(
                            "There should be a variable, which is the size of the blob")
                        var["size"] = argv[0]["name"]
                    else:
                        var["size"] = tmp["name"]
                    var["type"] = "class android::Parcel::Blob"
                    self.parcel_manager.add_variable(var)
                elif non_qualified_function_name == "writeFileDescriptor":
                    argv[0]["serialization_type"] = serialization_type
                    argv[0]["takeOwnership"] = argv[1]["value"]
                    self.parcel_manager.add_variable(argv[0])
                elif non_qualified_function_name == "writeParcelFileDescriptor" or non_qualified_function_name == "readParcelFileDescriptor":
                    var = OrderedDict()
                    var["name"] = self.parcel_manager.alloc_variable()
                    var["type"] = "_Bool"
                    var["serialization_type"] = serialization_type
                    self.parcel_manager.add_variable(var)

                    argv[0]["serialization_type"] = serialization_type
                    argv[0]["takeOwnership"] = argv[1]["name"]
                    self.parcel_manager.add_variable(argv[0])
                elif non_qualified_function_name == "setData":
                    argv[0]["size"] = argv[1]["name"]
                    argv[0]["serialization_type"] = SerializationType.INPUT
                    self.parcel_manager.add_variable(argv[0])
                else:
                    logger.error("Walk call not completed.")
                    exit(0)
            elif len(argv) == 3:
                if non_qualified_function_name == "appendFrom":
                    pass
                elif non_qualified_function_name == "writeBlob":
                    var = argv[2]
                    var["serialization_type"] = serialization_type
                    tmp = self.parcel_manager.get_previous_io_variable()
                    if tmp == None:
                        raw_input(
                            "There should be a variable, which is the size of the blob.")
                        var["size"] = argv[0]["name"]
                    else:
                        var["size"] = tmp["name"]
                    var["type"] = "class android::Parcel::Blob"
                    self.parcel_manager.add_variable(var)
                elif non_qualified_function_name == "readBlob":
                    raw_input("three argvs in readBlob???")
                    var = argv[1]
                    var["serialization_type"] = serialization_type
                    var["size"] = argv[0]["name"]
                    var["type"] = "class android::Parcel::Blob"
                    self.parcel_manager.add_variable(var)
                else:
                    logger.error(
                        "Unexpected parcel call with 3 arguments exists.")
                    exit(0)
            else:
                if non_qualified_function_name == "init":
                    pass
                else:
                    logger.error(
                        "Unexpected parcel call with >=4 arguments exists.")
                    exit(0)
                # calllocal...
        elif "callLocal" in non_qualified_function_name:
            # print argv[2]
            descriptor = OrderedDict()
            descriptor["name"] = "ServiceInterfaceDescriptor"
            descriptor["type"] = "android::String16"
            descriptor["serialization_type"] = 1
            self.parcel_manager.add_variable(descriptor)
            #  {
            #         "name": "constraint0",
            #         "opcode": "!",
            #         "lhs": {
            #             "name": "ServiceInterfaceDescriptor",
            #             "type": "android::String16",
            #             "serialization_type": 1
            #         },
            #         "probability": 0.1
            #     }
            cons = OrderedDict()
            cons["name"] = self.parcel_manager.alloc_constraint()
            cons["status"] = True
            cons["opcode"] = "!"
            cons["lhs"] = descriptor
            cons["probability"] = 0.1
            self.parcel_manager.add_constraint(cons)
            self.parcel_manager.active_constraint.append(cons["name"])
            old_active_parcel = self.parcel_manager.backup_active_parcel()
            self.parcel_manager.clear_active()
            self.parcel_manager.recover_active_parcel(old_active_parcel)
            self.parcel_manager.update_constraint_attr(
                cons["name"], "status", False)
            for key, argv in argv[2]["argv"].items():
                # if it is a parcelable structure
                # we should use parcel.writeparcelable()
                argv["callLocal"] = 1
                if argv["type"].endswith("*"):
                    argv["serialization_type"] = SerializationType.OUTPUT
                    argv["type"] = argv["type"].strip("*")
                else:
                    argv["serialization_type"] = SerializationType.INPUT
                self.parcel_manager.add_variable(argv)
        elif is_argv_related_with_parcel is True:
            if non_qualified_function_name == "writeToParcel" or non_qualified_function_name == "readFromParcel":
                if "android::Interpolator<float, float>" in func["name"]:
                    var["name"] = self.parcel_manager.alloc_variable()
                    var["type"] = "class android::Interpolator"
                elif "android::readFromParcel" == qualified_function_name:
                    # for BnResourceManagerService
                    var = argv[1]
                elif len(argv) == 2:
                    var["name"] = self.parcel_manager.alloc_variable()
                    var["func_name"] = func["name"]
                    var["signature"] = func["signature"]
                    var["type"] = "Function"
                    self.add_argv(var, argv)
                    self.misc_parcel_related_function.add("+".join(
                        [qualified_function_name, func["signature"]]))
                else:
                    key = call["MemberExpr"].keys()[-1]
                    tmp = OrderedDict()
                    tmp[key] = call["MemberExpr"][key]
                    var = self.get_variable_info(tmp)
                    var["name"] = var["name"].strip("->")
                    var["type"] = var["type"].strip("*").strip()

                if non_qualified_function_name == "writeToParcel":
                    var["serialization_type"] = SerializationType.OUTPUT
                else:
                    var["serialization_type"] = SerializationType.INPUT
                self.parcel_manager.add_variable(var)
                if var["serialization_type"] == SerializationType.OUTPUT and var["name"]=="_aidl_status":
                    # indicates that the interface file corresponding to this variable is generated by aidl
                    constraint = OrderedDict()
                    constraint["name"] = self.parcel_manager.alloc_constraint()
                    constraint["opcode"] = "=="
                    
                    lhs = OrderedDict()
                    lhs["name"] = "mException"
                    lhs["type"] = "int"
                    constraint["lhs"] = lhs

                    lhs["serialization_type"] = SerializationType.COMMON
                    self.parcel_manager.add_variable(lhs)

                    rhs = OrderedDict()
                    rhs["name"] = "android::EXNONE"
                    rhs["type"] = "int"
                    rhs["value"] = 0
                    constraint["rhs"] = rhs
                    
                    constraint["status"] = True
                    constraint["probability"] = 0.9

                    self.parcel_manager.add_constraint(constraint)
                    self.parcel_manager.active_constraint.append(constraint["name"])

            elif qualified_function_name == "android::ComposerState::read":
                var = self.walk_declref(call["MemberExpr"]["DeclRef"])
                var["serialization_type"] = SerializationType.INPUT
                self.special_parcelable_function.add("+".join([
                    func["name"], func["signature"], var["type"], "readFromParcel"
                ]))
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "android::DisplayState::read":
                var = self.walk_declref(call["MemberExpr"]["DeclRef"])
                var["serialization_type"] = SerializationType.INPUT
                self.special_parcelable_function.add("+".join([
                    func["name"], func["signature"], var["type"], "readFromParcel"
                ]))
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "android::layer_state_t::read":
                var["name"] = self.parcel_manager.alloc_variable()
                var["type"] = "struct android::layer_state_t"
                var["serialization_type"] = SerializationType.INPUT
                self.special_parcelable_function.add("+".join([
                    func["name"], func["signature"], var["type"], "readFromParcel"
                ]))
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "readDecryptHandleFromParcelData":
                var = copy.deepcopy(argv[0])
                var["type"] = var["type"].strip("*").strip()
                var["serialization_type"] = SerializationType.INPUT
                self.special_parcelable_function.add("+".join([
                    qualified_function_name, func["signature"], var["type"],
                    "readFromParcel"
                ]))
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "writeDecryptHandleToParcelData":
                var = copy.deepcopy(argv[0])
                var["type"] = var["type"].strip("*").strip()
                var["serialization_type"] = SerializationType.OUTPUT
                self.special_parcelable_function.add("+".join([
                    qualified_function_name, func["signature"], var["type"],
                    "writeToParcel"
                ]))
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "android::MetaData::createFromParcel":
                var = OrderedDict()
                var["name"] = self.parcel_manager.alloc_variable()
                var["type"] = "class android::MetaData"
                var["serialization_type"] = SerializationType.INPUT
                self.parcel_manager.add_variable(var)
                self.special_parcelable_function.add("+".join([
                    qualified_function_name, func["signature"], var["type"],
                    "readFromParcel"
                ]))
            elif qualified_function_name == "keystore::readKeyParameterFromParcel":
                var = OrderedDict()
                var["name"] = self.parcel_manager.alloc_variable()
                var["type"] = "struct android::hardware::keymaster::V4_0::KeyParameter"
                var["serialization_type"] = SerializationType.INPUT
                self.parcel_manager.add_variable(var)
                self.special_parcelable_function.add("+".join([
                    qualified_function_name, func["signature"], var["type"],
                    "readFromParcel"
                ]))
                return_var=var
            elif qualified_function_name == "keystore::readParamSetFromParcel":
                var = OrderedDict()
                var["name"] = self.parcel_manager.alloc_variable()
                var["serialization_type"] = SerializationType.INPUT
                var["type"] = "struct android::hardware::hidl_vec<struct android::hardware::keymaster::V4_0::KeyParameter>"
                self.parcel_manager.add_variable(var)
                self.special_parcelable_function.add("+".join([
                    qualified_function_name, func["signature"], var["type"],
                    "readFromParcel"
                ]))
                return_var = var
            elif qualified_function_name == "keystore::writeParamSetToParcel":
                var = OrderedDict()
                var["name"] = argv[0]["name"]
                var["serialization_type"] = SerializationType.OUTPUT
                var["type"] = "struct android::hardware::hidl_vec<struct android::hardware::keymaster::V4_0::KeyParameter>"
                self.parcel_manager.add_variable(var)
                self.special_parcelable_function.add("+".join([
                    qualified_function_name, func["signature"], var["type"],
                    "writeToParcel"
                ]))
            elif qualified_function_name == "keystore::writeKeyParameterToParcel":
                var = OrderedDict()
                var["name"] = argv[0]["name"]
                var["serialization_type"] = SerializationType.OUTPUT
                var["type"] = "struct android::hardware::keymaster::V4_0::KeyParameter"
                self.parcel_manager.add_variable(var)
                self.special_parcelable_function.add("+".join([
                    qualified_function_name, func["signature"], var["type"],
                    "writeToParcel"
                ]))
            elif qualified_function_name == "android::MetaDataBase::updateFromParcel":
                var = OrderedDict()
                var["name"] = self.parcel_manager.alloc_variable()
                var["serialization_type"] = SerializationType.INPUT
                var["type"] = "class android::MetaDataBase"
                self.parcel_manager.add_variable(var)
                self.special_parcelable_function.add("+".join([
                    qualified_function_name, func["signature"], var["type"],
                    "readFromParcel"
                ]))
            elif qualified_function_name == "android::media::midi::MidiDeviceInfo::readStringVector":
                var = copy.deepcopy(argv[1])
                var["serialization_type"] = SerializationType.INPUT
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "android::media::midi::MidiDeviceInfo::writeStringVector":
                var = copy.deepcopy(argv[1])
                var["serialization_type"] = SerializationType.OUTPUT
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "android::BnDrm::readVector":
                var = copy.deepcopy(argv[1])
                var["serialization_type"] = SerializationType.INPUT
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "android::BnDrm::writeVector":
                var = copy.deepcopy(argv[1])
                var["serialization_type"] = SerializationType.OUTPUT
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "android::BnCrypto::readVector":
                var = copy.deepcopy(argv[1])
                var["serialization_type"] = SerializationType.INPUT
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "android::BnCrypto::writeVector":
                var = copy.deepcopy(argv[1])
                var["serialization_type"] = SerializationType.OUTPUT
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "android::readVector":
                var = copy.deepcopy(argv[1])
                var["serialization_type"] = SerializationType.INPUT
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "android::view::Surface::readMaybeEmptyString16":
                var = OrderedDict()
                var["name"] = self.parcel_manager.alloc_variable()
                var["serialization_type"] = SerializationType.INPUT
                var["type"] = "class android::String16"
                self.parcel_manager.add_variable(var)
            elif qualified_function_name == "__android_log_print":
                pass
            elif qualified_function_name == "android::IBinder::transact":
                pass
            else:
                # input("Parcel passed into function...")
                var["name"] = self.parcel_manager.alloc_variable()
                var["func_name"] = func["name"]
                var["signature"] = func["signature"]
                var["type"] = "Function"
                self.add_argv(var, argv)
                serialization_type = SerializationType.COMMON
                # http://androidxref.com/9.0.0_r3/xref/frameworks/av/media/libmediaplayerservice/MediaPlayerService.cpp#982
                if qualified_function_name == "android::IMediaPlayer::setMetadataFilter" or \
                        qualified_function_name == "android::IMediaPlayer::setParameter" or \
                        qualified_function_name == "android::setSchedPolicy" or \
                        qualified_function_name == "android::IGraphicBufferProducer::createFromParcel" or \
                        qualified_function_name == "keystore::readCertificateChainFromParcel" or \
                        qualified_function_name == "android::MediaPlayer2::setAudioAttributes_l" or \
                        qualified_function_name == "android::MediaPlayer2Interface::setParameter" or \
                        qualified_function_name == "(anonymous namespace)::unmarshallFilter" or \
                        qualified_function_name == "android::(anonymous namespace)::unmarshallFilter" or \
                        qualified_function_name == "(anonymous namespace)::unmarshallAudioAttributes" or \
                        qualified_function_name == "android::(anonymous namespace)::unmarshallAudioAttributes" or \
                        qualified_function_name == "android::MediaPlayerService::Client::setAudioAttributes_l" or \
                        qualified_function_name == "android::os::PersistableBundle::readFromParcelInner" or\
                        qualified_function_name == "keystore::readKeymasterBlob" or \
                        qualified_function_name == "android::MediaPlayerBase::setParameter":
                    serialization_type = SerializationType.INPUT
                elif qualified_function_name == "android::IMediaExtractor::getMetrics" or \
                        qualified_function_name == "android::IMediaRecorder::getMetrics" or \
                        qualified_function_name == "android::MediaRecorderBase::getMetrics" or \
                        qualified_function_name == "android::IMediaPlayer::getMetadata" or \
                        qualified_function_name == "android::MediaPlayer2Interface::getMetadata" or \
                        qualified_function_name == "android::MediaPlayerBase::getMetadata" or \
                        qualified_function_name == "android::IMediaPlayer::getParameter" or \
                        qualified_function_name == "android::MediaPlayer2Interface::getParameter" or \
                        qualified_function_name == "android::MediaPlayerBase::getParameter" or \
                        qualified_function_name == "android::IGraphicBufferProducer::exportToParcel" or \
                        qualified_function_name == "keystore::writeCertificateChainToParcel" or \
                        qualified_function_name == "android::NuPlayer2::getTrackInfo" or \
                        qualified_function_name == "android::NuPlayer2::getSelectedTrack" or \
                        qualified_function_name == "android::IMediaPlayerService::pullBatteryData" or \
                        qualified_function_name == "android::MediaPlayerService::BatteryTracker::pullBatteryData" or \
                        qualified_function_name == "android::NuPlayer::getTrackInfo" or \
                        qualified_function_name == "android::NuPlayer::getSelectedTrack" or \
                        qualified_function_name == "android::os::PersistableBundle::writeToParcelInner" or \
                        qualified_function_name == "keystore::writeKeymasterBlob":
                    serialization_type = SerializationType.OUTPUT
                elif qualified_function_name == "android::IMediaPlayer::invoke" or \
                        qualified_function_name == "android::MediaPlayer2Interface::invoke" or \
                        qualified_function_name == "android::MediaPlayerBase::invoke":
                    serialization_type = SerializationType.INPUT | SerializationType.OUTPUT
                elif qualified_function_name =="android::AMessage::setPointer":
                    pass
                else:
                    logger.error(
                        "Misc function is not completed when len(argv)=1, meeting %s"
                        % qualified_function_name)
                    input()
                var["serialization_type"] = serialization_type
                self.parcel_manager.add_variable(var)

                self.misc_parcel_related_function.add("+".join(
                    [qualified_function_name, func["signature"]]))
        else:
            if non_qualified_function_name == "asBinder":
                # special case.
                if argv[0]["type"]=="class android::IGraphicBufferProducer":
                    argv[0]["type"]="class android::sp<class android::IGraphicBufferProducer>"
                return_var = argv[0]
            elif non_qualified_function_name == "asInterface":
                return_var = argv[0]
            elif non_qualified_function_name == "interface_cast":
                # return_var = argv[0]
                return_var["name"] = argv[0]["name"]
                self.parcel_manager.update_variable_attr(argv[0]["name"], "type",
                                                         return_var["type"])
            elif non_qualified_function_name == "resize":
                tmp = OrderedDict()
                last_key = call["MemberExpr"].keys()[-1]
                tmp[last_key] = call["MemberExpr"][last_key]
                var = self.get_variable_info(tmp)
                var["serialization_type"] = SerializationType.COMMON
                var["size"] = argv[0]["name"]
                self.parcel_manager.add_variable(var)
            elif non_qualified_function_name == "GetClientInterfaces":
                # http://androidxref.com/9.0.0_r3/xref/system/connectivity/wificond/server.cpp#191
                self.parcel_manager.update_variable_attr(
                    argv[0]["name"], "type", "vector<class android::sp<class android::net::wifi::IClientInterface>>")
            elif non_qualified_function_name == "GetApInterfaces":
                # samilar as above
                self.parcel_manager.update_variable_attr(
                    argv[0]["name"], "type", "vector<class android::sp<class android::net::wifi::IApInterface>>")
            elif non_qualified_function_name == "dup":
                return_var = argv[0]
            elif "android::base::unique_fd_impl<android::base::DefaultCloser>::operator int" in func[
                    "name"]:
                # special...
                # func["name"] has been set to b, e.g a.b
                return_var["name"] = self.walk_memberexpr(
                    call["MemberExpr"])["name"].split(".")[0]
                return_var["type"] = "FileDescriptor"
                return_var["serialization_type"] = serialization_type
                self.parcel_manager.add_variable(return_var)
            elif "memcpy"==func["name"]:
                self.parcel_manager.update_variable_attr(argv[0]["name"],"name",argv[1]["name"])
                argv[0]["name"] = argv[1]["name"]
                if argv[2]["type"]=="IntegerLiteral":
                    self.parcel_manager.update_variable_attr(argv[0]["name"],"type",argv[2]["sizeof"])
                    self.parcel_manager.update_variable_attr(argv[0]["name"],"structSize",argv[2]["value"])
                else:
                    self.parcel_manager.update_variable_attr(argv[0]["name"],"type",argv[1]["type"])
            elif "android::FOURCC"==func["name"]:
                value = 0
                for item in argv:
                    if "value" in item:
                        value  = (value<<8)+item["value"]
                    else:
                        logger.error(get_formmated_json(argv))
                        logger.error("Now we only support argument who has determined value when dealing FOURCC.")
                        exit(0)
                return_var["name"] = self.parcel_manager.alloc_variable()
                return_var["serialization_type"] = SerializationType.COMMON
                return_var["value"] = value
                self.parcel_manager.add_variable(return_var)
            elif len(argv) == 0 and "MemberExpr" in call and "type" in call[
                    "MemberExpr"] and call["MemberExpr"]["type"][
                        "$"] == "<bound member function type>":
                # special for drmInfo->getMimeType()
                if non_qualified_function_name.startswith(
                        "get") or non_qualified_function_name.startswith("set"):
                    return_var["name"] = non_qualified_function_name[3:]
                else:
                    tmp = self.walk_memberexpr(call["MemberExpr"])
                    if tmp["name"].endswith("::operator int"):
                        tmp["name"] = tmp["name"].split(".")[0].split("::")[-1]
                        return_var["name"] = tmp["name"]+".toInt"
                    elif tmp["name"].endswith("::operator bool"):
                        tmp["name"] = tmp["name"].split(".")[0].split("::")[-1]
                        return_var["name"] = tmp["name"]+".toBool" 
                    elif tmp["name"].endswith("::operator float"):
                        if "android::LayerDebugInfo::mColor" in tmp["name"]:
                            tmp["name"]=tmp["name"].replace(".android::half::operator float","")
                            name_splited = tmp["name"].split("::")
                            return_var["name"]  = name_splited[2].replace(".android","")+name_splited[-1]
                        else:
                            tmp["name"] = tmp["name"].split(".")[0].split("::")[-1]
                            return_var["name"] = tmp["name"]+".toFloat" 
                    elif tmp["name"].endswith("::get"):
                        # for function with a.get()
                        return_var["name"] = tmp["name"].split(".")[0]
                    elif tmp["name"].endswith("::c_str"):
                        return_var["name"] = tmp["name"].split(".")[0]
                        return_var["type"] = "char *"
                    elif "." in tmp["name"] or "->" in tmp["name"]:
                        prefix = tmp["name"].split(".")[0]
                        suffix = tmp["name"].split("::")[-1]
                        if prefix.startswith("implicit"):
                            return_var["name"] = suffix
                        else:
                            if "->" in prefix:
                                return_var["name"] = prefix+suffix
                            else:
                                return_var["name"] = prefix+"."+suffix
                    else:
                        return_var["name"] = tmp["name"]
                    # return_var["name"] = non_qualified_function_name
                return_var["serialization_type"] = serialization_type
                # do not add while statement cond value
                if non_qualified_function_name != "end":
                    self.parcel_manager.add_variable(return_var)
            else:
                pass
        if "name" not in return_var:
            return_var["name"] = self.parcel_manager.alloc_variable()
        return return_var

    def update_var_assgin(self, lhs, rhs):
        if rhs["type"]=="void *" and "value" in rhs and rhs["value"]=="NULL":
            return
        # if  "CXXConstructExpr" not in variable and "BinaryOperator" not in variable:
        serialization_type = self.parcel_manager.get_variable_attr(
            rhs["name"], "serialization_type")
        # means this variable is parcel or reply related variable
        # we need just update rhs variable
        # and we should return rhs variable
        if serialization_type is not None and SerializationType.is_related_with_parcel(
                serialization_type):
            if rhs["name"].startswith("implicit"):
                rhs["name"] = self.parcel_manager.update_variable_attr(
                    rhs["name"], "name", lhs["name"])
            # be careful with file descriptor and ...
            # if call["ReturnType"]["$"] not in ["sp<class android::IBinder>"]:
            if rhs["type"] != "FileDescriptor" and lhs["type"] != "size_t":
                if lhs["type"] in [
                        "long long", "long", "int", "unsigned int", "_Bool","unsigned char"
                ]:
                    pass
                elif " bool" in lhs["type"]:
                    pass
                elif "android::IBinder" in lhs["type"]:
                    pass
                elif "Function" == rhs["type"]:
                    pass
                elif rhs["type"] in lhs["type"] and "*" in lhs["type"]:
                    logger.debug(get_formatted_json(lhs))
                    logger.debug(get_formatted_json(rhs))
                    pass
                else:
                    rhs["type"] = self.parcel_manager.update_variable_attr(
                        rhs["name"], "type", lhs["type"])
            lhs = rhs
        else:
            # update value
            if "value" in rhs:
                lhs["value"] = self.parcel_manager.update_variable_attr(
                    lhs["name"], "value", rhs["value"])
            if "android::IBinder" in lhs["type"]:
                lhs["type"] = self.parcel_manager.update_variable_attr(
                    lhs["name"], "type", rhs["type"])
            lhs["serialization_type"] = SerializationType.COMMON
            self.parcel_manager.add_variable(lhs)

    def walk_var_assign(self, variable):
        logger.debug("Start walking Var")
        lhs_info = OrderedDict()
        logger.debug(get_formatted_json(variable))
        if len(variable["lhs"]) == 2:
            lhs_info["DeclRef"] = variable["lhs"]
        else:
            logger.error("Walk Var Assign is not completed when dealing lhs.")
            exit(0)
        lhs = self.get_variable_info(lhs_info)
        if "rhs" not in variable:
            logger.debug("Finish walking var.")
            return lhs

        rhs_info = variable["rhs"]
        rhs_key = rhs_info.keys()[0]
        rhs = OrderedDict()
        if rhs_key.startswith("CXXConstructExpr"):
            rhs = self.walk_cxxconstructexpr(rhs_info[rhs_key])
        elif rhs_key.startswith("CXXStaticCastExpr"):
            rhs = self.walk_cxx_static_cast_expr(rhs_info[rhs_key])
        elif rhs_key.startswith("DeclRef"):
            rhs = self.walk_declref(rhs_info[rhs_key])
            if rhs["name"] == "data" and "android::Parcel" in rhs["type"]:
                # print(lhs, rhs)
                input("walk var assign meeted Parcel..")
                lhs["serialization_type"] = SerializationType.INPUT
                self.parcel_manager.add_variable(lhs)
                logger.debug("Finish walking var.")
                return lhs
            else:
                logger.error(
                    "Walk var assign is not completed when dealing DeclRef(rhs).")
                exit(0)
        elif rhs_key.startswith("BinaryOperator"):
            bop = self.walk_binaryoperator(rhs_info[rhs_key])
            # http://androidxref.com/9.0.0_r3/xref/frameworks/av/media/libmedia/IMediaMetadataRetriever.cpp#362
            if "opcode" in bop:
                if bop["opcode"] in ["!=", "==", "&&", "&"]:
                    rhs["name"] = self.parcel_manager.alloc_variable()
                    rhs["type"] = "int"
            elif "+" in bop["value"]:
                rhs = bop
            else:
                logger.error(
                    "Walk var assign is not completed when dealing BinaryOperator.")
                exit(0)
        else:
            rhs = self.get_variable_info(rhs_info)
            if "opcode" in rhs:
                rhs = rhs["lhs"]
        self.update_var_assgin(lhs, rhs)
        logger.debug("Finish walking var.")
        return lhs

    def walk_hs(self, hs):
        logger.debug("Start walking hs.")
        key = hs.keys()[0]
        var = OrderedDict()
        if key.startswith("CXXOperatorCallExpr"):
            if "android::details::TMat44<float>::" in str(hs):
                # special for SurfaceFlinger
                var = OrderedDict()
                var["name"] = "mClientColorMatrix[i][j]"
                var["type"] = "float"
                return var
            else:
                # http://androidxref.com/9.0.0_r3/xref/frameworks/av/media/libmedia/IMediaExtractorService.cpp#87
                var["opcode"], var["lhs"], var["rhs"] = self.walk_cxxoperatorcallexpr(
                    hs[key])
        elif key.startswith("CXXConstructExpr"):
            var = self.walk_cxxconstructexpr(hs["CXXConstructExpr"])
        elif key.startswith("BinaryOperator"):
            var = self.walk_binaryoperator(hs[key])
        else:
            info = OrderedDict()
            info[key] = hs[key]
            var = self.get_variable_info(info)
            if "name" in var:
                varCnt = self.parcel_manager.get_variable_attr(var["name"], "count")
                if varCnt>1:
                    var["name"] = var["name"]+"_"+str(varCnt-1)
        logger.debug("Finish walking hs.")

        return var

    def walk_binaryoperator(self, binaryoperator):

        logger.debug("Start walking binary operator.")

        opcode = binaryoperator["opcode"]["$"]
        lhs = self.walk_hs(binaryoperator["LHS"])
        logger.info("lhs:")
        logger.info(get_formatted_json(lhs))
        rhs = self.walk_hs(binaryoperator["RHS"])
        logger.info("rhs:")
        logger.info(get_formatted_json(rhs))

        if opcode == "=" and ("CXXMemberCallExpr" in binaryoperator["RHS"]
                              or "CStyleCastExpr" in binaryoperator["RHS"] or "ImplicitCastExpr" in binaryoperator["RHS"] or "CallExpr" in binaryoperator["RHS"] or "CXXStaticCastExpr" in binaryoperator["RHS"]):
            # we exclude BinaryConditionalOperator
            logger.info(get_formatted_json(binaryoperator["RHS"]))
            logger.info("meet = in binaryoperator")
            # i, status is not a meaningful name
            # if lhs["name"] != 'i' and lhs["name"] != "status" and lhs["name"] != "result":
            self.update_var_assgin(lhs, rhs)
            logger.debug("Finish walking binary operator.")
            return lhs
        if opcode == "|" and lhs["type"] == "IntegerLiteral" and rhs[
                "type"] == "IntegerLiteral":
            var = OrderedDict()
            var["type"] = "IntegerLiteral"
            var["value"] = lhs["value"] | rhs["value"]
            var["name"] = self.parcel_manager.alloc_constraint()
            logger.debug("Finish walking binary operator.")
            return var
        bop = OrderedDict()
        bop["opcode"] = opcode
        bop["lhs"] = lhs
        bop["rhs"] = rhs
        if opcode == "&&":
            if "opcode" not in bop["lhs"]:
                tmp = OrderedDict()
                tmp["opcode"] = ""
                tmp["lhs"] = bop["lhs"]
                bop["lhs"] = tmp
            if "opcode" not in bop["rhs"]:
                tmp = OrderedDict()
                tmp["opcode"] = ""
                tmp["lhs"] = bop["rhs"]
                bop["rhs"] = tmp
        if opcode == "+" or opcode == "-" or opcode == "*" or opcode == "/" or opcode == "%":
            if "IntegerLiteral" in lhs["type"]:
                bop["type"] = rhs["type"]
            else:
                bop["type"] = lhs["type"]

        if opcode == "+" or opcode == "-" or opcode == "*" or opcode == "/" or opcode == "%":
            var = OrderedDict()
            var["name"] = self.parcel_manager.alloc_variable()
            var["type"] = bop["lhs"]["type"]
            var["value"] = bop["lhs"]["name"] + opcode + bop["rhs"]["name"]
            var["serialization_type"] = SerializationType.COMMON
            self.parcel_manager.add_variable(var)
            if not self.parcel_manager.is_variable_exists(lhs["name"]):
                lhs["serialization_type"] = SerializationType.COMMON
                self.parcel_manager.add_variable(lhs)
            if not self.parcel_manager.is_variable_exists(rhs["name"]):
                rhs["serialization_type"] = SerializationType.COMMON
                self.parcel_manager.add_variable(rhs)
            # TODO: detail calculation method for var
            logger.debug("Finish walking binary operator.")
            return var
        logger.debug("Finish walking binary operator.")
        return bop

    def walk_conditional_operator(self, expr):
        logger.debug("Start walking conditional operator.")
        tmp = []
        for key, value in expr.items():
            d = OrderedDict()
            d[key] = value
            tmp.append(d)

        # get the constraint
        constraint = OrderedDict()

        if "CXXOperatorCallExpr" in tmp[0]:
            opcode, lhs, rhs = self.walk_cxxoperatorcallexpr(
                tmp[0]["CXXOperatorCallExpr"])
            constraint["opcode"] = opcode
            constraint["lhs"] = lhs
            constraint["rhs"] = rhs
            constraint["status"] = True
        elif "BinaryOperator" in tmp[0]:
            constraint = self.walk_binaryoperator(tmp[0]["BinaryOperator"])
            constraint["status"] = True
        elif "DeclRef" in tmp[0]:
            constraint["opcode"] = "!="
            lhs = self.get_variable_info(tmp[0])
            constraint["lhs"] = lhs
            lhs["serialization_type"] = SerializationType.COMMON
            self.parcel_manager.add_variable(lhs)

            rhs = OrderedDict()
            rhs["name"] = self.parcel_manager.alloc_variable()
            rhs["type"] = "IntegerLiteral"
            rhs["value"] = 0
            rhs["serialization_type"] = SerializationType.COMMON
            self.parcel_manager.add_variable(rhs)

            constraint["rhs"] = rhs
            constraint["status"] = True
        elif "MemberExpr" in tmp[0]:
            constraint["opcode"] = ""
            # print(tmp[0])
            constraint["lhs"] = self.walk_memberexpr(tmp[0]["MemberExpr"])
            constraint["status"] = True
        elif "ImplicitCastExpr" in tmp[0]:
            constraint["opcode"] = ""
            constraint["lhs"] = self.walk_implicitcastexpr(
                tmp[0]["ImplicitCastExpr"])
            constraint["status"] = True
        else:
            logger.error("walk conditional operator is not completed..")
            logger.error(get_formatted_json(expr))
            exit(0)
        # deal with the True branch
        op1 = self.get_variable_info(tmp[1])
        # deal with the False branch
        op2 = self.get_variable_info(tmp[2])
        constraint["probability"] = 0.5
        logger.debug("Finish walking conditional operator.")
        return constraint, op1, op2

    def walk_cxxconstructexpr(self, expr):

        logger.debug("Start walking CXXConstruct Expr.")
        construct = OrderedDict()
        construct["signature"] = expr["signature"]["$"]
        construct["name"] = expr["name"]["$"]
        construct["serialization_type"] = SerializationType.COMMON
        construct["argv"] = []
        for key, value in expr["member"].items():
            tmp = OrderedDict()
            tmp[key] = value
            var = self.get_variable_info(tmp)
            if "android::Parcel" in var["type"]:
                if construct["name"]=="android::media::Metadata::Metadata":
                    continue
                elif var["name"] == "data":
                    construct["serialization_type"] |= SerializationType.INPUT
                elif var["name"] == "reply":
                    construct["serialization_type"] |= SerializationType.OUTPUT
                else:
                    logger.error(get_formatted_json(construct))
                    logger.error(get_formatted_json(var))
                    raw_input("self.walk_cxxconstructexpr meeted unexpetced parcel..")
                    logger.error("walk cxxconstruct expr is not completed..")
                    exit(0)
            construct["argv"].append(var)

        return_var = OrderedDict()
        if len(construct["argv"]) == 0:
            if construct["name"] == "android::sp<android::IBinder>::sp":
                return_var["name"] = self.parcel_manager.alloc_variable()
                return_var["type"] = "class android::sp<class android::IBinder>"
                return_var["serialization_type"] = SerializationType.COMMON
            elif construct["name"] == "std::__1::vector<android::sp<android::IBinder>, std::__1::allocator<android::sp<android::IBinder> > >::vector":
                # special for binder vector
                return_var["name"] = self.parcel_manager.alloc_variable()
                return_var["type"] = "class vector<class android::sp<class android::IBinder>>"
                return_var["serialization_type"] = SerializationType.COMMON
            elif construct["name"]=="android::Parcel::ReadableBlob::ReadableBlob":
                return_var["name"] = self.parcel_manager.alloc_variable()
                return_var["type"] = "class android::Parcel::ReadableBlob"
                return_var["serialization_type"] = SerializationType.COMMON
            elif construct["name"]=="android::Parcel::WritableBlob::WritableBlob":
                return_var["name"] = self.parcel_manager.alloc_variable()
                return_var["type"] = "class android::Parcel::WritableBlob"
                return_var["serialization_type"] = SerializationType.COMMON
            elif construct["name"]=="android::Parcel::Parcel":
                return_var["name"] = self.parcel_manager.alloc_variable()
                return_var["type"] = "class android::Parcel"
                return_var["serialization_type"] = SerializationType.COMMON
            else:
                return_var["name"] = self.parcel_manager.alloc_variable()
                return_var["serialization_type"] = construct["serialization_type"]
                return_var["type"] = "Function"
                return_var["func_name"] = construct["name"]
                return_var["signature"] = construct["signature"]
                self.add_argv(return_var, construct["argv"])
                self.parcel_manager.add_variable(return_var)
        elif len(construct["argv"]) == 1:
            if "android::String" in construct["signature"] or "char *" in construct["signature"]:
                return_var = construct["argv"][0]
            elif "android::sp<android::IInterface>::sp" == construct["name"]:
                return_var = construct["argv"][0]
            elif "android::sp<android::IBinder>::sp" == construct["name"]:
                return_var = construct["argv"][0]
            elif "android::sp<" in construct["name"] and  "::I" in construct["name"]:
                return_var = construct["argv"][0]
            elif "android::half::half" == construct["name"]:
                self.parcel_manager.update_variable_attr(construct["argv"][0]["name"],"type","class android::half")
                return_var = construct["argv"][0]
                return_var["type"] = "class android::half"
            elif construct["signature"]=="void (class android::hardware::keymaster::V4_0::NullOr<struct android::hardware::keymaster::V4_0::KeyParameter> &&) noexcept":
                return_var = construct["argv"][0]
            else:
                return_var["name"] = self.parcel_manager.alloc_variable()
                return_var["serialization_type"] = construct["serialization_type"]
                if construct[
                        "name"] == "android::IGraphicBufferProducer::QueueBufferInput::QueueBufferInput":
                    return_var[
                        "type"] = "struct android::IGraphicBufferProducer::QueueBufferInput"
                else:
                    return_var["type"] = "Function"
                    return_var["func_name"] = construct["name"]
                    return_var["signature"] = construct["signature"]
                    self.add_argv(return_var, construct["argv"])
                self.parcel_manager.add_variable(return_var)
        else:
            return_var["name"] = self.parcel_manager.alloc_variable()
            return_var["serialization_type"] = construct["serialization_type"]
            return_var["type"] = "Function"
            return_var["func_name"] = construct["name"]
            return_var["signature"] = construct["signature"]
            self.add_argv(return_var, construct["argv"])
            self.parcel_manager.add_variable(return_var)
        logger.debug("Finish walking CXXConstruct expr.")
        return return_var

    def walk_cond(self, cond):
        logger.debug("Start walking condition statement.")

        constraint = OrderedDict()
        constraint["name"] = self.parcel_manager.alloc_constraint()
        if "UnaryOperator" in cond:
            # if(!a)
            unary = self.walk_unaryoperator(cond["UnaryOperator"])
            constraint["opcode"] = unary["opcode"]
            constraint["lhs"] = unary["lhs"]
        elif "DeclRef" in cond:
            # if (a)
            lhs = self.get_variable_info(cond)
            constraint["opcode"] = ""
            constraint["lhs"] = lhs
        elif "BinaryOperator" in cond:
            # if(a>b)
            bop = self.walk_binaryoperator(cond["BinaryOperator"])
            constraint["opcode"] = bop["opcode"]
            constraint["lhs"] = bop["lhs"]
            constraint["rhs"] = bop["rhs"]
        elif "CXXOperatorCallExpr" in cond:
            opcode, lhs, rhs = self.walk_cxxoperatorcallexpr(
                cond["CXXOperatorCallExpr"])
            constraint["opcode"] = opcode
            constraint["lhs"] = lhs
            constraint["rhs"] = rhs
        elif "CallExpr" in cond:
            # TODO: maybe we should be more careful.
            lhs = self.walk_call(cond["CallExpr"])
            constraint["opcode"] = ""
            constraint["lhs"] = lhs
        elif "CXXMemberCallExpr" in cond:
            lhs = self.walk_call(cond["CXXMemberCallExpr"])
            constraint["opcode"] = ""
            constraint["lhs"] = lhs
        elif "MemberExpr" in cond:
            lhs = self.walk_memberexpr(cond["MemberExpr"])
            constraint["opcode"] = ""
            constraint["lhs"] = lhs
        elif "False" in cond:
            constraint["opcode"] = ""
            lhs = OrderedDict()
            lhs["name"] = self.parcel_manager.alloc_variable()
            lhs["type"] = "IntegerLiteral"
            lhs["value"] = 0
            constraint["lhs"] = lhs
        elif "ImplicitCastExpr" in cond:
            constraint["opcode"] = ""
            lhs = self.walk_implicitcastexpr(cond["ImplicitCastExpr"])
            constraint["lhs"] = lhs
        else:
            logger.error("Walk Cond is not completed")
            logger.error(get_formatted_json(cond))
            exit(0)
        logger.debug("Finish walking condition statement.")
        return constraint

    def walk_then_or_else(self, then_else):
        logger.debug("Start walking then or else statement.")
        # actually we can just use the compoundstmt.
        path_status_var = self.walk_compoundstmt(then_else)
        logger.debug("Finish walking then or else statement.")
        return path_status_var

    def is_error_path(self, path_status_var):
        status_var_name = path_status_var["name"]
        if status_var_name == "" or "android::NO_ERROR" in status_var_name or "android::OK" in status_var_name or "status" in status_var_name:
            return 0
        return 1

    def walk_ifstmt(self, ifstmt):

        logger.debug("Start walking IfStmt.")
        logger.debug(get_formatted_json(ifstmt))
        if "Var" in ifstmt:
            self.walk_var_assign(ifstmt["Var"])
        cons = self.walk_cond(ifstmt["IfCond"])
        cons["status"] = True
        cons["probability"] = 0.5
        self.parcel_manager.add_constraint(cons)
        self.parcel_manager.active_constraint.append(cons["name"])
        old_active_parcel = self.parcel_manager.backup_active_parcel()
        then_path_status_var = self.walk_then_or_else(ifstmt["Then"])
        logger.debug("path status var in then statement: " +
                     str(then_path_status_var["name"]))
        # like this
        # if a2:
        #   write(a3)
        #   return
        if then_path_status_var["name"] != "":
            logging.info("Return exists inside then statement.")
            self.parcel_manager.clear_active()
            self.parcel_manager.recover_active_parcel(old_active_parcel)
        else:
            pass
        self.parcel_manager.update_constraint_attr(
            cons["name"], "status", False)
        # deal with else
        else_path_status_var = OrderedDict()
        else_path_status_var["name"] = ""
        if "Else" in ifstmt:
            else_path_status_var = self.walk_then_or_else(ifstmt["Else"])
            self.parcel_manager.active_constraint.pop()
        elif then_path_status_var["name"] == "":
            # special for checkInterface
            if cons["name"] == "constraint0" and cons["lhs"]["name"] == "ServiceInterfaceDescriptor":
                pass
            else:
                self.parcel_manager.active_constraint.pop()
        # consider the condition probability
        if self.is_error_path(then_path_status_var):
            if self.is_error_path(else_path_status_var):
                pass
            else:
                self.parcel_manager.update_constraint_attr(cons["name"], "probability",
                                                           0.1)
        else:
            if self.is_error_path(else_path_status_var):
                self.parcel_manager.update_constraint_attr(cons["name"], "probability",
                                                           0.9)
            else:
                pass
        logger.debug("Finish walking IfStmt.")

    def walk_binary_conditional_operator(self, expr):
        logger.debug("Start walking binary conditional operator.")
        logger.debug(get_formatted_json(expr))

        # condition
        condition_expr = expr["BinaryCondition"]
        logger.debug("BinaryCondition:")
        logger.debug(get_formatted_json(condition_expr))
        if "ImplicitCastExpr" in condition_expr:
            condition_expr = condition_expr["ImplicitCastExpr"]
            if "CXXMemberCallExpr" in condition_expr:
                condition_expr = condition_expr["CXXMemberCallExpr"]
                var = self.walk_call(condition_expr)
            elif "ImplicitCastExpr" in condition_expr:
                condition_expr = condition_expr["ImplicitCastExpr"]
                var = self.walk_implicitcastexpr(condition_expr)
            else:
                logger.error(
                    "No ImplicitCastExpr when walking binary conditional operator.")
                exit(0)
            var["serialization_type"] = SerializationType.COMMON
            self.parcel_manager.add_variable(var)
        elif "CallExpr" in condition_expr:
            condition_expr = condition_expr["CallExpr"]
            var = self.walk_call(condition_expr)
        else:
            logger.error(
                "No ImplicitCastExpr when walking binary conditional operator.")
            exit(0)

        constraint = OrderedDict()
        constraint["name"] = self.parcel_manager.alloc_constraint()
        constraint["opcode"] = ""
        constraint["lhs"] = var
        constraint["status"] = True
        constraint["probability"] = 0.5
        self.parcel_manager.add_constraint(constraint)
        self.parcel_manager.active_constraint.append(constraint["name"])

        # backup active parcel
        old_active_parcel = self.parcel_manager.backup_active_parcel()

        # true branch
        true_node = expr["TrueNode"]
        logger.debug("TrueNode:")
        logger.debug(get_formatted_json(true_node))
        if "CXXMemberCallExpr" in true_node:
            true_node = true_node["CXXMemberCallExpr"]
            if true_node == condition_expr:
                true_var = var
            else:
                logger.error(
                    "True node is not equal to condition when meeting CXXMemberCallExpr.")
                exit(0)
        elif "ImplicitCastExpr" in true_node:
            true_node = true_node["ImplicitCastExpr"]
            if true_node == condition_expr:
                true_var = var
            else:
                logger.error(
                    "True node is not equal to condition when meeting ImplicitCastExpr.")
                exit(0)
        elif "CallExpr" in true_node:
            true_node = true_node["CallExpr"]
            if true_node == condition_expr:
                true_var = var
            else:
                logger.error(
                    "True node is not equal to condition when meeting ImplicitCastExpr.")
                exit(0)
        else:
            logger.error(
                "True node in walk_binary_conditional_operator in not completed.")
            exit(0)

        # false branch
        self.parcel_manager.update_constraint_attr(
            constraint["name"], "status", False)
        false_node = expr["FalseNode"]
        logger.debug("FalseNode:")
        logger.debug(get_formatted_json(false_node))
        if "CXXMemberCallExpr" in false_node:
            var = self.walk_call(false_node["CXXMemberCallExpr"])
        elif "BinaryConditionalOperator" in false_node:
            self.walk_binary_conditional_operator(
                false_node["BinaryConditionalOperator"])
        elif "CallExpr" in false_node:
            self.walk_call(false_node["CallExpr"])
        elif "ConditionalOperator" in false_node:
            cons, op1, op2 = self.walk_conditional_operator(
                false_node["ConditionalOperator"])
        else:
            logger.error(
                "False node in walk_binary_conditional_operator in not completed.")
            exit(0)

        # pop the last active constraint
        self.parcel_manager.active_constraint.pop()
        logger.debug("Finish walking binary conditional operator.")
        return var

    def walk_forinit(self, forinit):
        logger.debug("Start walking for Init Statement.")

        if "Var" in forinit:
            var = self.walk_var_assign(forinit["Var"])
        elif "IntegerLiteral" in forinit:
            input("IntegerLiteral inside Forinit")
            start_value = forinit["IntegerLiteral"]["value"]["$"]
            var_name = forinit["IntegerLiteral"]["Var"]["name"]["$"]
            var_type = forinit["IntegerLiteral"]["Var"]["type"]["$"]
        elif len(forinit) == 0:
            var = OrderedDict()
            return var
        else:
            logger.error("Walk For Init is not completed.")
            logger.error(get_formatted_json(forinit))
            exit(0)
        logger.debug("Finish walking for Init Statement.")
        return var

    def walk_for_inc(self, for_inc):

        inc = OrderedDict()
        if "UnaryOperator" in for_inc:
            # ++i
            unary = self.walk_unaryoperator(for_inc["UnaryOperator"])
            opcode = unary["opcode"]
            var = unary["lhs"]
        elif "BinaryOperator" in for_inc:
            tmp = for_inc["BinaryOperator"]
            opcode = tmp["opcode"]["$"]
            if opcode == ",":
                unary = self.walk_unaryoperator(tmp["LHS"]["UnaryOperator"])
                opcode = unary["opcode"]
                var = unary["lhs"]
            elif opcode == "=":
                opcode = "++"
                var = OrderedDict()
                var["name"] = "interator"
                var["type"] = "int"
            else:
                logger.error(
                    "Walk for inc is not completed when dealing with bop.")
                exit(0)
        elif "CXXOperatorCallExpr" in for_inc:
            tmp = for_inc["CXXOperatorCallExpr"]
            var = self.walk_cxxoperatorcallexpr(tmp)
            if "opcode" in var:
                opcode = var["opcode"]
                var = var["lhs"]
            else:
                logger.error(
                    "Walk for inc is not completed when dealing with ImplicitCastExpr."
                )
                exit(0)
        else:
            logger.error("Walk for inc is not completed.")
            logger.error(get_formatted_json(for_inc))
            exit(0)
        inc["opcode"] = opcode
        inc["name"] = var["name"]
        inc["type"] = var["type"]
        return inc

    def walk_forstmt(self, forstmt):

        logger.debug("Start walking for statement.")

        var = self.walk_forinit(forstmt["ForInit"])
        var["serialization_type"] = SerializationType.COMMON
        if "name" in var:
            self.parcel_manager.add_variable(var)
        else:
            # too hack..
            inc = self.walk_for_inc(forstmt["ForInc"])
            if "name" not in var:
                var["name"] = inc["name"]
                var["type"] = inc["type"]
                self.parcel_manager.add_variable(var)

        counter = OrderedDict()
        counter["name"] = var["name"]
        counter["type"] = var["type"]
        counter["value"] = 0

        for_info = OrderedDict()
        for_name = self.parcel_manager.alloc_loop()
        self.parcel_manager.active_loop.append(for_name)
        for_info["name"] = for_name
        for_info["counter"] = counter

        constraint = self.walk_cond(forstmt["ForCond"])
        constraint["probability"] = 0.5
        self.parcel_manager.add_constraint(constraint)
        for_info["constraint"] = constraint["name"]

        inc = self.walk_for_inc(forstmt["ForInc"])
        for_info["inc"] = inc

        self.parcel_manager.add_loop(for_info)

        # we could just use compoundstmt
        self.walk_compoundstmt(forstmt["ForBody"])
        # pop the last one
        self.parcel_manager.active_loop.pop()
        logger.debug("Finish walking for statement.")

    def walk_cxxforrangestmt(self, forrangestmt):

        logger.debug("Start walking CXX For Range Statement.")

        # before cycle, there might be some size hint..
        possible_rhs = OrderedDict()
        possible_rhs["name"] = self.parcel_manager.variable[-1]["name"]
        possible_rhs["type"] = self.parcel_manager.variable[-1]["type"]

        var_name = self.parcel_manager.alloc_variable()
        var_type = "unsigned int"
        lhs = OrderedDict()
        lhs["name"] = var_name
        lhs["type"] = var_type
        lhs["value"] = 0
        lhs["serialization_type"] = SerializationType.COMMON

        self.parcel_manager.add_variable(lhs)

        counter = OrderedDict()
        counter["name"] = var_name

        counter["type"] = var_type
        counter["value"] = 0

        for_info = OrderedDict()
        for_info["counter"] = counter
        for_name = self.parcel_manager.alloc_loop()
        self.parcel_manager.active_loop.append(for_name)

        for_info["name"] = for_name
        cons = OrderedDict()
        cons["name"] = self.parcel_manager.alloc_constraint()
        cons["opcode"] = "<"
        cons["lhs"] = lhs

        true_rhs = OrderedDict()
        flag = 0
        if "Var1" in forrangestmt:
            vector_key = forrangestmt["Var1"]["rhs"].keys()[0]
            tmp = OrderedDict()
            tmp[vector_key] = forrangestmt["Var1"]["rhs"][vector_key]
            vector = self.get_variable_info(tmp)
            vectorname = vector["name"]
            vector_size = self.parcel_manager.get_variable_attr(
                vectorname, "size")
            true_rhs["type"] = "unsigned int"
            true_rhs["name"] = vector_size
            flag = 1
        elif "Var" in forrangestmt:
            # http://androidxref.com/9.0.0_r3/xref/frameworks/av/include/media/Interpolator.h#249
            # unknown size
            possible_rhs = self.parcel_manager.get_previous_io_variable()
            if possible_rhs is None:
                true_rhs["name"] = "unknown"
                true_rhs["type"] = "IntegerLiteral"
                true_rhs["value"] = 1
            else:
                true_rhs = possible_rhs
            flag = 1
        else:
            logger.error("Walk CXXForRangeStmt failed, missing constraint.")
            exit(0)

        if flag == 0:
            cons["rhs"] = possible_rhs
        else:
            cons["rhs"] = true_rhs
        cons["probability"] = 0.5
        self.parcel_manager.add_constraint(cons)

        for_info["constraint"] = cons["name"]

        inc = OrderedDict()
        inc["opcode"] = "++"
        inc["name"] = var_name
        inc["type"] = var_type
        for_info["inc"] = inc

        self.parcel_manager.add_loop(for_info)
        # we could just use compoundstmt
        self.walk_compoundstmt(forrangestmt["ForBody"])
        # remove the last active loop
        self.parcel_manager.active_loop.pop()
        logger.debug("Finish walking CXX For Range Statement.")

    def walk_whilestmt(self, whilestmt):

        logger.debug("Start walking while statement.")
        while_name = self.parcel_manager.alloc_loop()
        self.parcel_manager.active_loop.append(while_name)
        whilecond = whilestmt["WhileCond"]
        while_info = OrderedDict()
        if "CallExpr" in whilecond:
            call = whilecond["CallExpr"]
            func_name = call["MemberExpr"]["name"]["$"]
            if "::hasNext" in func_name:
                while_info["var_name"] = "iterator"
            else:
                logger.error(
                    "Walk While Statement(CallExpr) is not completed.")
                logger.error(get_formatted_json(whilestmt))
                exit(0)
            # guess constraint
            # for more clear, we should use
            rhs = OrderedDict()
            rhs["name"] = self.parcel_manager.variable[-1]["name"]
            rhs["type"] = self.parcel_manager.variable[-1]["type"]

            counter = OrderedDict()
            var_name = self.parcel_manager.alloc_variable()
            var_type = "unsigned int"
            counter["name"] = var_name
            counter["type"] = var_type
            counter["value"] = 0
            counter["serialization_type"] = SerializationType.COMMON
            self.parcel_manager.add_variable(counter)
            while_info["counter"] = counter

            inc = OrderedDict()
            inc["opcode"] = "++"
            inc["name"] = var_name
            inc["type"] = var_type
            while_info["inc"] = inc

            cons = OrderedDict()
            cons["name"] = self.parcel_manager.alloc_constraint()
            cons["opcode"] = "<"
            cons["lhs"] = counter
            rhs["type"] = "unsigned int"
            cons["rhs"] = rhs
            cons["probability"] = 0.9

            self.parcel_manager.add_constraint(cons)
            while_info["constraint"] = cons["name"]
            while_info["name"] = while_name
            self.parcel_manager.add_loop(while_info)
        elif "CXXOperatorCallExpr" in whilecond:

            op, lhs, rhs = self.walk_cxxoperatorcallexpr(
                whilecond["CXXOperatorCallExpr"])
            # print(op, lhs, rhs)

            rhs = OrderedDict()
            # guess ?
            rhs["name"] = self.parcel_manager.variable[-1]["name"]
            rhs["type"] = self.parcel_manager.variable[-1]["type"]

            counter = OrderedDict()
            var_name = self.parcel_manager.alloc_variable()
            var_type = "unsigned int"
            counter["name"] = var_name
            counter["type"] = var_type
            counter["value"] = 0
            counter["serialization_type"] = SerializationType.COMMON
            self.parcel_manager.add_variable(counter)
            while_info["counter"] = counter

            inc = OrderedDict()
            inc["opcode"] = "++"
            inc["name"] = var_name
            inc["type"] = var_type
            while_info["inc"] = inc

            cons = OrderedDict()
            cons["name"] = self.parcel_manager.alloc_constraint()
            cons["opcode"] = "<"
            cons["lhs"] = counter

            cons["rhs"] = rhs
            cons["probability"] = 0.9
            self.parcel_manager.add_constraint(cons)

            while_info["constraint"] = cons["name"]
            while_info["name"] = while_name
            self.parcel_manager.add_loop(while_info)
        elif "BinaryOperator" in whilecond:
            bop = self.walk_binaryoperator(whilecond["BinaryOperator"])
            lhs = bop["lhs"]
            rhs = bop["rhs"]

            counter = OrderedDict()
            counter["name"] = lhs["name"]
            counter["type"] = lhs["type"]
            counter["value"] = 0
            while_info["counter"] = counter

            inc = OrderedDict()
            inc["opcode"] = "++"
            inc["name"] = counter["name"]
            inc["type"] = counter["type"]
            while_info["inc"] = inc

            cons = OrderedDict()
            cons["name"] = self.parcel_manager.alloc_constraint()
            cons["opcode"] = "<"
            cons["lhs"] = lhs
            cons["rhs"] = rhs
            cons["probability"] = 0.9
            self.parcel_manager.add_constraint(cons)

            while_info["constraint"] = cons["name"]
            while_info["name"] = while_name
            self.parcel_manager.add_loop(while_info)
        elif "CXXMemberCallExpr" in whilecond:
            call = whilecond["CXXMemberCallExpr"]
            func_name = call["MemberExpr"]["name"]["$"]
            if "::hasNext" in func_name:
                while_info["var_name"] = "iterator"
            else:
                logger.error(
                    "Walk While Statement(CXXMemberCallExpr) is not completed.")
                logger.error(get_formatted_json(whilestmt))
                exit(0)
            # guess constraint
            # for more clear, we should use
            rhs = OrderedDict()
            rhs["name"] = self.parcel_manager.variable[-1]["name"]
            rhs["type"] = self.parcel_manager.variable[-1]["type"]

            counter = OrderedDict()
            var_name = self.parcel_manager.alloc_variable()
            var_type = "unsigned int"
            counter["name"] = var_name
            counter["type"] = var_type
            counter["value"] = 0
            counter["serialization_type"] = SerializationType.COMMON
            self.parcel_manager.add_variable(counter)
            while_info["counter"] = counter

            inc = OrderedDict()
            inc["opcode"] = "++"
            inc["name"] = var_name
            inc["type"] = var_type
            while_info["inc"] = inc

            cons = OrderedDict()
            cons["name"] = self.parcel_manager.alloc_constraint()
            cons["opcode"] = "<"
            cons["lhs"] = counter
            rhs["type"] = "unsigned int"
            cons["rhs"] = rhs
            cons["probability"] = 0.9

            self.parcel_manager.add_constraint(cons)
            while_info["constraint"] = cons["name"]
            while_info["name"] = while_name
            self.parcel_manager.add_loop(while_info)
        elif "ImplicitCastExpr" in whilecond:
            pass
        else:
            logger.error("Walk while cond is not completed.")
            logger.error(get_formatted_json(whilestmt))
            exit(0)
        self.walk_compoundstmt(whilestmt["WhileBody"])
        self.parcel_manager.active_loop.pop()
        logger.debug("Finish walking while statement.")

    def walk_dostmt(self, dostmt):
        # TODO: improve..
        self.walk_compoundstmt(dostmt["DoBody"])
        # self.parcel_manager.active_loop.pop()
        logger.debug("Finish walking do statement.")

    def walk_switch(self, switch):
        if "DeclRef" in switch:
            switch_var = self.walk_declref(switch["DeclRef"])
            del switch["DeclRef"]
        elif "MemberExpr" in switch:
            tmp = dict()
            tmp["MemberExpr"] = switch["MemberExpr"]
            switch_var = self.get_variable_info(tmp)
            del switch["MemberExpr"]
        elif "ImplicitCastExpr" in switch:
            switch_var = self.walk_implicitcastexpr(switch["ImplicitCastExpr"])
            del switch["ImplicitCastExpr"]
        elif "CallExpr" in switch:
            switch_var = self.walk_call(switch["CallExpr"])
            del switch["CallExpr"]
        else:
            logger.error("Walk switch var is not completed.")
            logger.error(get_formatted_json(switch[switch.keys()[0]]))
            exit(0)
        if "serialization_type" not in switch_var:
            switch_var["serialization_type"] = SerializationType.COMMON
        self.parcel_manager.add_variable(switch_var)
        del switch_var["serialization_type"]
        if "ReturnStmt" in switch:
            del switch["ReturnStmt"]
        for key, code in switch.items():
            if not key.startswith("code"):
                if key.startswith("CXXMemberCallExpr"):
                    self.walk_call(code)
                elif key.startswith("CompoundStmt"):
                    self.walk_compoundstmt(code)
                elif key.startswith("ReturnStmt"):
                    self.walk_returnstmt(code)
                elif key.startswith("BinaryOperator"):
                    self.walk_binaryoperator(code)
                else:
                    print(key)
                    logger.error("Walk switch is not completed.")
                    logger.error(get_formatted_json(code))
                    exit(0)
                # http://androidxref.com/9.0.0_r3/xref/system/security/keystore/keystore_aidl_hidl_marshalling_utils.cpp#130
                continue
            rhs = OrderedDict()
            rhs["name"] = self.parcel_manager.alloc_variable()
            rhs["type"] = "IntegerLiteral"
            rhs["value"] = code["@left"]
            # rhs["serialization_type"] = SerializationType.COMMON
            # self.parcel_manager.add_variable(rhs)
            # del rhs["serialization_type"]
            cons = OrderedDict()
            cons["name"] = self.parcel_manager.alloc_constraint()
            cons["opcode"] = "=="
            cons["lhs"] = copy.deepcopy(switch_var)
            cons["rhs"] = rhs
            cons["probability"] = 0.9
            cons["status"] = True
            self.parcel_manager.add_constraint(cons)

            self.parcel_manager.active_constraint.append(cons["name"])

            del code["@left"]
            del code["@right"]
            if len(code) == 0:
                continue
            while "code" in code:
                code = code["code"]
                del code["@left"]
                del code["@right"]
            if "CompoundStmt" in code:
                self.walk_compoundstmt(code)
            else:
                for expr in code:
                    if expr == "BinaryOperator":
                        self.walk_binaryoperator(code[expr])
                    elif expr == "CXXMemberCallExpr":
                        self.walk_call(code[expr])
                    elif expr == "CXXOperatorCallExpr":
                        self.walk_cxxoperatorcallexpr(code[expr])
                    elif expr == "Var":
                        self.walk_var_assign(code[expr])
                    elif expr == "ReturnStmt":
                        self.walk_returnstmt(code[expr])
                    else:
                        logger.error(expr)
                        logger.error(
                            "Walk Switch is not completed when not meeting compoundstmt."
                        )
                        exit(0)
            self.parcel_manager.active_constraint.pop()

    def walk_returnstmt(self, returnstmt):
        logger.debug("Start walking Return Statement.")
        return_var = self.get_variable_info(returnstmt)
        if "opcode" in return_var:
            # TODO: We should be more careful, now we only consider unary..
            return_var = return_var["lhs"]
        if "serialization_type" not in return_var:
            return_var["serialization_type"] = SerializationType.COMMON
        varCnt = self.parcel_manager.get_variable_attr(
            return_var["name"], "count")
        if varCnt is None:
            self.parcel_manager.add_variable(return_var)
        elif varCnt == 1:
            pass
            # self.parcel_manager.update_variable_attr(return_var["name"], "serialization_type", 2)
        else:
            # means this variable is a return variable.
            # update the latest variable
            lastestName = return_var["name"] + '_' + str(varCnt - 1)
            # self.parcel_manager.update_variable_attr(lastestName, "serialization_type", 2)
            return_var["name"] = lastestName
        self.parcel_manager.set_return_var(return_var["name"])
        logger.debug("Finish walking Return Statement")
        # self.parcel_manager.clear_active()
        path_status_var = OrderedDict()
        previous_var = self.parcel_manager.get_previous_io_variable()
        if previous_var is not None and "value" in previous_var and previous_var["name"].startswith("android::"):
            path_status_var = previous_var
        else:
            path_status_var = return_var
        logger.debug("Finish walking Return Statement.")
        return path_status_var

    def getFiledAttr(self, field, attr):
        if "$" in field[attr]:
            return field[attr]["$"]
        return ""

    def walk_cxxrecorddecl(self, cxxrecorddecl):

        for key, field in cxxrecorddecl.items():
            var = OrderedDict()
            var["name"] = self.getFiledAttr(field, "name")
            var["type"] = self.getFiledAttr(field, "type")

            if var["type"].startswith("union "):
                tmp = var["type"]
                tmp = tmp.strip()
                self.raw_structure_set.add(tmp)
            if "struct" in var["type"]:
                tmp = var["type"]
                tmp = tmp.replace("const", "")
                tmp = tmp.replace("*", "")
                if "[" in tmp and "[]" not in tmp:
                    tmp = tmp.split("[")
                    tmp = tmp[0].strip()
                tmp = tmp.strip()
                # print(tmp)
                self.raw_structure_set.add(tmp)
            var["serialization_type"] = SerializationType.INPUT
            self.parcel_manager.add_variable(var)

    def walk_compoundstmt(self, compoundstmt):
        logger.debug("Start walking compountstmt.")
        path_status_var = OrderedDict()
        # means no path
        path_status_var["name"] = ""
        for key, value in compoundstmt.items():
            if key.startswith("CXXMemberCallExpr"):
                self.walk_call(value)
            elif key.startswith("CallExpr"):
                self.walk_call(value)
            elif key.startswith("Var"):
                self.walk_var_assign(value)
            elif key.startswith("CompoundStmt"):
                path_status_var = self.walk_compoundstmt(value)
            elif key.startswith("CXXConstructExpr"):
                self.walk_cxxconstructexpr(value)
            elif key.startswith("CXXStaticCastExpr"):
                self.walk_cxx_static_cast_expr(value)
            elif key.startswith("CStyleCastExpr"):
                self.walk_cstyle_cast_expr(value)
            elif key.startswith("CXXOperatorCallExpr"):
                self.walk_cxxoperatorcallexpr(value)
            elif key.startswith("DeclRef"):
                name = value["name"]["$"]
                types = value["type"]["$"]
                # TODO: e.g.  StatsDimensionsValue.cpp
                # self.parcel_manager.add_variable(name, types, serialization_type)
            elif key.startswith("DeclStmt"):
                pass
            elif key.startswith("BinaryOperator"):
                self.walk_binaryoperator(value)
            elif key.startswith("IfStmt"):
                self.walk_ifstmt(value)
            elif key.startswith("ReturnStmt"):
                path_status_var = self.walk_returnstmt(value)
            elif key.startswith("ForStmt"):
                self.walk_forstmt(value)
            elif key.startswith("CXXForRangeStmt"):
                self.walk_cxxforrangestmt(value)
            elif key.startswith("WhileStmt"):
                self.walk_whilestmt(value)
            elif key.startswith("switch"):
                self.walk_switch(value)
            elif key.startswith("BinaryConditionalOperator"):
                self.walk_binary_conditional_operator(value)
            elif key.startswith("DoStmt"):
                self.walk_dostmt(value)
            elif key.startswith("ImplicitCastExpr"):
                self.walk_implicitcastexpr(value)
            else:
                logger.error("Unexpected key {} in compoundstmt.".format(key))
                logger.error(get_formatted_json(value))
                exit(0)
        logger.debug("Finish walking compoundstmt.")
        return path_status_var

    def append_to_code_list(self, codelist, transaction):
        left = transaction['@left']
        right = transaction['@right']
        if isinstance(left, int) and isinstance(right, int):
            codelist += range(left, right)
        elif isinstance(left, int):
            codelist.append(left)
        else:
            logger.error(left)
            logger.error(right)
            logger.error(
                "parse_one_transaction is not completed when dealing code.")
            exit(0)

    def store_one_file(self, filename, new_data):
        existing_data = open(filename).read().strip().split("\n")
        if filename.endswith("_function.txt"):
            for item in new_data:
                if item not in existing_data:
                    print("Find new function %s" % item)
        new_data = set(existing_data) | new_data
        with open(filename, "w") as f:
            for item in new_data:
                f.write(str(item)+"\n")

    def store_related_info(self):
        cfg = json.load(open("../../fans.cfg"))
        fans_dir = cfg["fans_dir"]
        interface_model_extractor_dir = os.path.join(
            fans_dir, cfg["interface_model_extractor_dir"])

        self.store_one_file(os.path.join(
            interface_model_extractor_dir, "used_types.txt"), self.parcel_manager.used_types)

        self.store_one_file(os.path.join(interface_model_extractor_dir,
                                         "raw_structure_types.txt"), self.raw_structure_set)

        misc_parcel_related_function_storage_location = os.path.join(
            fans_dir, cfg["misc_parcel_related_function_storage_location"])
        self.store_one_file(
            misc_parcel_related_function_storage_location, self.misc_parcel_related_function)

        special_parcelable_function_storage_location = os.path.join(
            fans_dir, cfg["special_parcelable_function_storage_location"])
        self.store_one_file(
            special_parcelable_function_storage_location, self.special_parcelable_function)

    def parse_one_transaction(self, transaction):
        """Walk one transaction.
        """
        logging.debug("Start walking one transaction.")

        codelist = []
        self.append_to_code_list(codelist, transaction)
        while "code" in transaction:
            transaction = transaction["code"]
            self.append_to_code_list(codelist, transaction)
            del transaction["@left"]
            del transaction["@right"]

        for key, value in transaction.items():
            if key == "@left" or key == "@right":
                continue
            elif key.startswith("CompoundStmt"):
                self.walk_compoundstmt(value)
            elif key.startswith("ReturnStmt"):
                self.walk_returnstmt(value)
            elif key.startswith("IfStmt"):
                self.walk_ifstmt(value)
            else:
                logging.debug("Unexpected key: " + key)
                exit(0)
        tx = self.parcel_manager.dump()
        new_tx = OrderedDict()
        new_tx["code"] = codelist
        tx = OrderedDict(new_tx.items()+tx.items())
        self.store_related_info()
        logging.debug("Finish walking one transaction.")
        return codelist, tx

    def walk_structure(self, structure):
        key = structure.keys()[0]
        value = structure[key]
        if key.startswith("Var"):
            self.walk_var_assign(value)
        else:
            self.walk_compoundstmt(value)
        parcel = self.parcel_manager.dump()
        self.store_related_info()
        return parcel

    def walk_parcel_function(self, function):
        key = function.keys()[2]
        stmt = function[key]
        # fake left and right which is not used inside structure.
        if key == "ReturnStmt":
            self.walk_returnstmt(stmt)
        elif key == "CompoundStmt":
            self.walk_compoundstmt(stmt)
        elif key == "switch":
            self.walk_switch(stmt)
        elif key == "Var":
            self.walk_var_assign(stmt)
        elif key == "CXXMemberCallExpr":
            self.walk_call(stmt)
        elif key == "IfStmt":
            self.walk_ifstmt(stmt)
        else:
            logging.error("Walk Parcel Function is not completed for %s." % key)
            exit(0)
        argv_stmt = function["argv"]
        argv = []
        for key, arg in argv_stmt.items():
            tmp = OrderedDict()
            if "$" in arg["name"]:
                tmp["name"] = arg["name"]["$"]
            else:
                # for those like this http://androidxref.com/9.0.0_r3/xref/frameworks/av/media/libmediaplayer2/mediaplayer2.cpp#598
                tmp["name"] = ""
            tmp["type"] = arg["type"]["$"]
            if self.parcel_manager.is_variable_exists(tmp["name"]) is False:
                tmp["serialization_type"] = SerializationType.COMMON
                self.parcel_manager.add_variable(tmp)
            argv.append(tmp)
        parcel = self.parcel_manager.dump()
        self.store_related_info()
        parcel["argv"] = argv
        return parcel

    def walk_raw_structure(self, structure):
        key = structure.keys()[0]
        CXXRecordDecl = structure[key]
        self.walk_cxxrecorddecl(CXXRecordDecl)
        parcel = self.parcel_manager.dump()
        self.store_related_info()
        return parcel, self.raw_structure_set