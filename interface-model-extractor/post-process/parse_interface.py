import json
import logging
import os
from jsonutil import xml2json,get_formatted_json,replace_name_wapper
from collections import OrderedDict
from ast_visitor import ASTVisitor
from variable_type import load_simplified_typemap
import argparse

cfg = json.load(open("../../fans.cfg"))

fans_dir = cfg["fans_dir"]

rough_interface_data_dir = os.path.join(
    cfg["rough_interface_related_data_dir"], "interface")
interface_model_extractor_dir = os.path.join(
    fans_dir, cfg["interface_model_extractor_dir"])

already_parsed_interfaces_file = os.path.join(interface_model_extractor_dir, "already_parsed_interfaces.txt")
already_parsed_interfaces = []
if os.path.exists(already_parsed_interfaces_file):
    already_parsed_interfaces = open(already_parsed_interfaces_file).read().split("\n")

interface_model_extractor_tmp_dir = os.path.join(interface_model_extractor_dir, "tmp")

interface_model_dir = os.path.join(interface_model_extractor_dir, "model")


black_interfaces = open('data/customized_interface_list.txt').read().strip().split("\n")
tmp = open('data/not_related_interface_list.txt').read().strip().split("\n")
black_interfaces +=tmp

# actually, only top level interfaces have service name,
# here, we assign multi-level interfaces service name according to the function name.
func2svc = json.load(open("data/func2svcname.json"))
type_map = load_simplified_typemap()


logging.basicConfig(format='%(levelname)s - %(message)s')
logger = logging.getLogger(__name__)
logger.setLevel(logging.ERROR)

# this function deal with some special cases
# in order to generate better dependency...
def special_deal(tx_key,tx):
    # http://androidxref.com/9.0.0_r3/xref/frameworks/av/camera/aidl/android/hardware/camera2/ICameraDeviceUser.aidl#96
    if tx_key=="ICameraDeviceUser::9-9" or tx_key=="ICameraDeviceUser::8-8":
        replace_name_wapper(tx,"_aidl_return","name","in_streamId")


def parse_transactions(transactions, interface_name):
    """Walk transactions one by one in data.
    """
    logger.debug("Start walking trasanctions.")
    all_txs = OrderedDict()
    for key, transaction in transactions.items():
        if key.startswith("code"):
            logger.info("Start dealing code " + key.strip("code"))
            ast_walker = ASTVisitor(type_map)
            codelist,tx = ast_walker.parse_one_transaction(transaction)
            tx_key = interface_name + "::" + str(codelist[0]) + "-" + str(codelist[-1])
            special_deal(tx_key,tx)
            all_txs[tx_key] = tx
        elif key.startswith("ReturnStmt"):
            continue
        elif key.startswith("BinaryOperator"):
            continue
        elif key.startswith("CompoundStmt"):
            continue
        else:
            logger.error("Unexpected key: " + key+" in parse_transactions")
            logger.error(get_formatted_json(transactions[key]))
            exit(0)
    logger.debug("Finish walking trasanctions.")
    return all_txs


def parse_one_interface(filename):
    logger.info("Start parsing file: " + filename)
    funcname = filename.split("::")[-2]
    data = xml2json(os.path.join(rough_interface_data_dir, filename))

    tmp_interface_file = os.path.join(
        interface_model_extractor_tmp_dir, "tmp_interface_file.json")
    open(tmp_interface_file, "w").write(get_formatted_json(data))
    last_switch = data['onTransact'].keys()[-1]
    transactions = data['onTransact'][last_switch]
    del transactions["DeclRef"]

    serviceName = func2svc[funcname]["serviceName"]
    interfaceToken = func2svc[funcname]["interfaceToken"]
    interfaceName =func2svc[funcname]["interfaceName"]

    all_txs = parse_transactions(transactions, interfaceName)

    for key in all_txs:
        all_txs[key]["serviceName"] = serviceName
        all_txs[key]["interfaceName"] = interfaceName
        all_txs[key]["interfaceToken"] = interfaceToken
    interface_model_service_dir = os.path.join(interface_model_dir, "service")
    interface_file = os.path.join(interface_model_service_dir,funcname + ".json")
    open(interface_file, "w").write(
        get_formatted_json(all_txs))
    logger.info("Finish parsing file: " + filename)


def parse_interfaces():
    for filename in os.listdir(rough_interface_data_dir):
        if not os.path.isdir(filename):
            if filename not in already_parsed_interfaces:
                filename = str(filename)
                funcname = filename.split("::")[-2]
                if funcname in black_interfaces:
                    continue
                if "::BnHw" in filename or "BBinder" in filename:
                    continue
                parse_one_interface(filename)
                open(already_parsed_interfaces_file, "a").write(filename + "\n")

def test_one_interface(target_interface_name):
    for filename in os.listdir(rough_interface_data_dir):
        if not os.path.isdir(filename):
            funcname = filename.split("::")[-2]
            if funcname in black_interfaces:
                continue
            if target_interface_name== func2svc[funcname]["interfaceName"]:
                parse_one_interface(filename)
                raw_input()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="parse interfaces")
    parser.add_argument("-i","--interface",help="parse one interface")
    args = parser.parse_args()
    if args.interface:
        test_one_interface(args.interface)
    else:
        parse_interfaces()
