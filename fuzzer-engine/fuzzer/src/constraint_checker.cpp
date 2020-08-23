#include <fuzzer/constraint_checker.h>
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/types/int_type.h>
#include <fuzzer/types/types.h>
#include <fuzzer/utils/random.h>
#include <string>
#include <typeinfo>

ConstraintChecker::ConstraintChecker(ParcelReaderWriter *parcelReaderWriter) {
  this->parcelReaderWriter = parcelReaderWriter;
}

bool ConstraintChecker::check(const string &constraintName) {
  int32_t consIdx = parcelReaderWriter->getConstraintIdx(constraintName);
  return this->check(parcelReaderWriter->constraint[consIdx]);
}
bool ConstraintChecker::check(const Json::Value &constraint) {

  string constraintName = constraint["name"].asString();
  string opcode = constraint["opcode"].asString();
  Json::Value lhs = constraint["lhs"];

  FUZZER_LOGD("Check %s status.", constraintName.c_str());
  if (parcelReaderWriter->constraintStatus.count(constraintName) != 0) {
    FUZZER_LOGD("Use cached constraint status: %d.",
                (int32_t)parcelReaderWriter->constraintStatus[constraintName]);
    return parcelReaderWriter->constraintStatus[constraintName];
  }

  bool status;
  if (opcode == "!") {
    status = checkNot(constraint);
  } else if (opcode == "") {
    status = checkSelf(constraint);
  } else if (opcode == "&&") {
    Json::Value rhs = constraint["rhs"];
    bool left, right;
    spaceNum += 2;
    left = this->check(lhs);
    if (left == false) {
      FUZZER_LOGD("Short-circuit evaluation as lhs is not satisfied.");
      status = false;
    } else {
      right = this->check(rhs);
      status = left && right;
    }
    spaceNum -= 2;
  } else if (opcode == "||") {
    Json::Value rhs = constraint["rhs"];
    bool left, right;
    spaceNum += 2;
    left = this->check(lhs);
    if (left == true) {
      FUZZER_LOGD("Short-circuit evaluation as lhs is satisfied.");
      status = true;
    } else {
      right = this->check(rhs);
      status = left || right;
    }
    spaceNum -= 2;
  } else {
    // <, <=, ==, >=, >.
    Json::Value rhs = constraint["rhs"];

    string lhsName = lhs["name"].asString();
    string rhsName = rhs["name"].asString();

    bool lhsCacheStatus = parcelReaderWriter->getVarCachedStatus(lhs);
    bool rhsCacheStatus = parcelReaderWriter->getVarCachedStatus(rhs);
    // check if variable is cached in map, both lhs and rhs.
    // if not, random return true or false.
    if (lhsCacheStatus == 0 || rhsCacheStatus == 0) {

      if (lhsCacheStatus == 0) {
        FUZZER_LOGD("lhs %s is not cached.", lhsName.c_str());
      } else {
        FUZZER_LOGD("rhs %s is not cached.", rhsName.c_str());
      }

      int32_t lhsLoopIdx, rhsLoopIdx;
      lhsLoopIdx = parcelReaderWriter->getCounterLoopIdx(lhsName);
      rhsLoopIdx = parcelReaderWriter->getCounterLoopIdx(rhsName);

      if (lhsCacheStatus == 0 && lhsLoopIdx != -1) {
        FUZZER_LOGD("lhs is a loop counter, we can init it.");
        parcelReaderWriter->setLoopCounter(lhsLoopIdx, 0);
        status = this->check(constraint);
      } else if (rhsCacheStatus == 0 && rhsLoopIdx != -1) {
        FUZZER_LOGD("rhs is a loop counter, we can init it.");
        parcelReaderWriter->setLoopCounter(rhsLoopIdx, 0);
        status = this->check(constraint);
      } else {
        // meaning this checker returns a random result;
        this->random = true;
        if (IntType<int>::nOutOf(constraint["probability"].asFloat() * 10,
                                 10)) {
          FUZZER_LOGD("Constraint %s's status is unknown, we randomly treat it "
                      "as true.",
                      constraint["name"].asCString());
          status = true;
        } else {
          FUZZER_LOGD("Constraint %s's status is unknown, we randomly treat it "
                      "as false.",
                      constraint["name"].asCString());
          status = false;
        }
      }
    } else {
      VarType lhsType, rhsType;
      if (varTypeMap.count(lhs["type"].asString())) {
        lhsType = varTypeMap[lhs["type"].asString()];
      } else {
        FUZZER_LOGE("Unexpected lhsType %s meeted.", lhs["type"].asCString());
        exit(0);
      }

      if (varTypeMap.count(rhs["type"].asString())) {
        rhsType = varTypeMap[rhs["type"].asString()];
      } else {
        FUZZER_LOGE("Unexpected rhsType %s meeted.", rhs["type"].asCString());
        exit(0);
      }

      map<string, int> &intMap = parcelReaderWriter->intMap;
      // map<string, size_t> &sizetMap = parcelReaderWriter->sizetMap;
      map<string, uint32_t> &uintMap = parcelReaderWriter->uintMap;
      map<string, int64_t> &int64Map = parcelReaderWriter->int64Map;
      map<string, uint64_t> &uint64Map = parcelReaderWriter->uint64Map;
      map<string, float> &floatMap = parcelReaderWriter->floatMap;
      map<string, string> &stringMap = parcelReaderWriter->stringMap;

      if (lhsType == INT32_TYPE && rhsType == INT32_TYPE) {
        FUZZER_LOGD("compare %d %s %d.", intMap[lhsName], opcode.c_str(),
                    intMap[rhsName]);
        status = compare(opcode, intMap[lhsName], intMap[rhsName]);
      }
      // } else if (lhsType == "size_t" && rhsType == "size_t") {
      //   status = compare(opcode, sizetMap[lhsName], sizetMap[rhsName]);
      // }
      else if (lhsType == UINT32_TYPE && rhsType == UINT32_TYPE) {
        status = compare(opcode, uintMap[lhsName], uintMap[rhsName]);
      } else if (lhsType == INT32_TYPE && rhsType == INTEGER_LITERAL) {
        status = compare(opcode, intMap[lhsName], rhs["value"].asInt());
      } else if (lhsType == INT32_TYPE && rhsType == UINT64_TYPE) {
        status = compare(opcode, (uint64_t)intMap[lhsName], uint64Map[rhsName]);
      } else if (lhsType == UINT32_TYPE && rhsType == INTEGER_LITERAL) {
        status = compare(opcode, uintMap[lhsName], rhs["value"].asUInt());
      } else if (lhsType == UINT32_TYPE && rhsType == INT32_TYPE) {
        status = compare(opcode, uintMap[lhsName], (uint32_t)intMap[rhsName]);
      } else if (lhsType == UINT32_TYPE && rhsType == INT64_TYPE) {
        status = compare(opcode, (int64_t)uintMap[lhsName], int64Map[rhsName]);
      } else if (lhsType == INT64_TYPE && rhsType == INT64_TYPE) {
        status = compare(opcode, int64Map[lhsName], int64Map[rhsName]);
      } else if (lhsType == UINT64_TYPE && rhsType == UINT64_TYPE) {
        status = compare(opcode, uint64Map[lhsName], uint64Map[rhsName]);
      } else if (lhsType == UINT64_TYPE && rhsType == INT64_TYPE) {
        status =
            compare(opcode, uint64Map[lhsName], (uint64_t)int64Map[rhsName]);
      } else if (lhsType == UINT64_TYPE && rhsType == INTEGER_LITERAL) {
        status = compare(opcode, uint64Map[lhsName], rhs["value"].asUInt64());
      } else if (lhsType == INT64_TYPE && rhsType == INTEGER_LITERAL) {
        status = compare(opcode, int64Map[lhsName], rhs["value"].asInt64());
      } else if (lhsType == UINT64_TYPE && rhsType == INT32_TYPE) {
        status = compare(opcode, uint64Map[lhsName], (uint64_t)intMap[rhsName]);
      } else if (lhsType == UINT64_TYPE && rhsType == UINT32_TYPE) {
        status = compare(opcode, uint64Map[lhsName], uintMap[rhsName]);
      } else if (lhsType == INTEGER_LITERAL && rhsType == UINT32_TYPE) {
        status = compare(opcode, lhs["value"].asUInt(), uintMap[rhsName]);
      } else if (lhsType == INTEGER_LITERAL && rhsType == INT32_TYPE) {
        status = compare(opcode, lhs["value"].asInt(), intMap[rhsName]);
      } else if (lhsType == FLOAT_TYPE && rhsType == FLOAT_TYPE) {
        status = compare(opcode, floatMap[lhsName], floatMap[rhsName]);
      } else if (lhsType == STRING16_TYPE || lhsType == STRING8_TYPE ||
                 lhsType == CSTRING_TYPE) {
        string lhsValue, rhsValue;
        if (lhs.isMember("value")) {
          lhsValue = lhs["value"].asString();
        } else {
          lhsValue = stringMap[lhsName];
        }

        if (rhs.isMember("value")) {
          rhsValue = rhs["value"].asString();
        } else {
          rhsValue = stringMap[rhsName];
        }
        if (opcode == "<") {
          status = lhsValue < rhsValue;
        } else if (opcode == "<=") {
          status = lhsValue <= rhsValue;
        } else if (opcode == "==" || opcode == "operator==") {
          status = lhsValue == rhsValue;
        } else if (opcode == ">=") {
          status = lhsValue >= rhsValue;
        } else if (opcode == ">") {
          status = lhsValue > rhsValue;
        } else if (opcode == "!=" || opcode == "operator!=") {
          status = lhsValue != rhsValue;
        } else {
          FUZZER_LOGE("Unexpected opcode %s meeted when comparing two string "
                      "variables.",
                      opcode.c_str());
          exit(0);
        }
      } else {
        FUZZER_LOGE("This kind of constraint is not supported now: lhsType "
                    "%s, rhsType %s",
                    lhs["type"].asCString(), rhs["type"].asCString());
        exit(0);
      }
    }
  }
  // TODO: Maybe it is a good choice to record the constraint status whether it
  // is known or unknown.

  // cache constraint status.

  // if (!this->random) {
  FUZZER_LOGD("Cache %s status = %d.", constraint["name"].asCString(), status);
  parcelReaderWriter->constraintStatus[constraintName] = status;
  // } else {
  //   FUZZER_LOGD("Constraint %s's status is unknown, we won't record it.",
  //               constraint["name"].asCString());
  // }
  return status;
}

bool ConstraintChecker::checkConstraintSet(vector<Json::Value> constraintSet) {
  bool satisfied = true;
  spaceNum += 2;
  FUZZER_LOGD("The size of constraint set is %lu.", constraintSet.size());
  FUZZER_LOGD("Start checking the variable's constraint set.");
  for (uint32_t i = 0; i < constraintSet.size(); ++i) {
    if (check(constraintSet[i]) == 0) {
      satisfied = false;
      break;
    }
  }
  FUZZER_LOGD("Finish checking the variable's constraint set.");
  spaceNum -= 2;
  return satisfied;
}

template <typename T1, typename T2>
bool ConstraintChecker::compare(string opcode, const T1 &left,
                                const T2 &right) {
  if (opcode == "<") {
    return left < right;
  } else if (opcode == "<=") {
    return left <= right;
  } else if (opcode == "==" || opcode == "operator==") {
    return left == right;
  } else if (opcode == ">=") {
    return left >= right;
  } else if (opcode == ">") {
    return left > right;
  } else if (opcode == "!=" || opcode == "operator!=") {
    return left != right;
  } else if (opcode == "&") {
    return (uint64_t)left & (uint64_t)right;
  } else {
    FUZZER_LOGE("unexpected opcode %s meeted when comparing two variables.",
                opcode.c_str());
    exit(0);
  }
}

bool ConstraintChecker::checkNot(Json::Value constraint) {
  FUZZER_LOGD("Start checking !variable.");
  Json::Value variable = constraint["lhs"];
  if (!parcelReaderWriter->getVarCachedStatus(variable)) {
    FUZZER_LOGD("Variable %s hasn't been cached.",
                variable["name"].asString().c_str());
    if (IntType<int>::nOutOf(10 * constraint["probability"].asFloat(), 10)) {
      return true;
    } else {
      return false;
    }
  }
  string varName = variable["name"].asString();
  VarType varType = varTypeMap[variable["type"].asString()];
  if (varType == STRING8_TYPE || varType == STRING16_TYPE ||
      varType == CSTRING_TYPE) {
    string value = parcelReaderWriter->stringMap[varName];
    if (value != "NULL") {
      return false;
    } else {
      return true;
    }
  } else if (varType == INT32_TYPE) {
    int32_t value = parcelReaderWriter->intMap[varName];
    return !value;
  } else if (varType == UINT32_TYPE) {
    uint32_t value = parcelReaderWriter->uintMap[varName];
    return !value;
  } else if (varType == UINT64_TYPE) {
    uint64_t value = parcelReaderWriter->uint64Map[varName];
    return !value;
  } else if (varType == INT64_TYPE) {
    uint64_t value = parcelReaderWriter->int64Map[varName];
    return !value;
  } else if (varType == BOOL_TYPE) {
    bool value = parcelReaderWriter->boolMap[varName];
    return !value;
  } else if (variable["type"].asString().find("android::GraphicBuffer") !=
             string::npos) {
    // remain to be improved.
    return true;
  } else {
    FUZZER_LOGE("Unexpected variable type %s when checking not constraint.",
                variable["type"].asCString());
    exit(0);
  }
}

bool ConstraintChecker::checkSelf(Json::Value constraint) {
  Json::Value variable = constraint["lhs"];
  FUZZER_LOGD("Start checking variable (No opcode. When variable is not zero, "
              "the constraint is satisfied).");
  if (!parcelReaderWriter->getVarCachedStatus(variable)) {
    FUZZER_LOGD("Variable %s hasn't been cached.",
                variable["name"].asString().c_str());
    if (IntType<int>::nOutOf(10 * constraint["probability"].asFloat(), 10)) {
      return true;
    } else {
      return false;
    }
  }
  string varName = variable["name"].asString();
  VarType varType = varTypeMap[variable["type"].asString()];
  if (varType == STRING8_TYPE || varType == STRING16_TYPE ||
      varType == CSTRING_TYPE) {
    string value = parcelReaderWriter->stringMap[varName];
    if (value != "NULL") {
      return true;
    } else {
      return false;
    }
  } else if (varType == INT32_TYPE) {
    int32_t value = parcelReaderWriter->intMap[varName];
    return value;
  } else if (varType == UINT32_TYPE) {
    uint32_t value = parcelReaderWriter->uintMap[varName];
    return value;
  } else if (varType == UINT64_TYPE) {
    uint64_t value = parcelReaderWriter->uint64Map[varName];
    return value;
  } else if (varType == INT64_TYPE) {
    uint64_t value = parcelReaderWriter->int64Map[varName];
    return value;
  } else if (varType == BOOL_TYPE) {
    bool value = parcelReaderWriter->boolMap[varName];
    return value;
  } else if (variable["type"].asString().find("android::GraphicBuffer") !=
             string::npos) {
    // remain to be improved.
    return true;
  } else {
    FUZZER_LOGE("Unexpected variable type %s when checking self.",
                variable["type"].asCString());
    exit(0);
  }
}

int32_t ConstraintChecker::getSpecialSelfConstraintIdx(
    vector<Json::Value> constraintSet) {
  vector<int> tmp;
  for (int32_t i = 0; i < (int32_t)constraintSet.size(); ++i) {
    string opcode = constraintSet[i]["opcode"].asString();
    if (opcode.find("==") != string::npos || opcode == "!" || opcode == "") {
      FUZZER_LOGD("Find special constraint %s in constraint %s.",
                  constraintSet[i]["opcode"].asCString(),
                  constraintSet[i]["name"].asCString());
      tmp.push_back(i);
    }
  }
  if (tmp.size() > 0) {
    uint32_t idx = randomUInt64(0, tmp.size() - 1);
    return tmp[idx];
  } else {
    return -1;
  }
}

bool ConstraintChecker::shouldGenerateValueFromSpecialSelfConstraint(
    const Json::Value &constraint) {
  string opcode = constraint["opcode"].asString();
  FUZZER_LOGD("Decide whether we should generate value according to opcode(%s) "
              "(if exists).",
              opcode.c_str());
  // check whether constraint's lhs and rhs are cached or not.
  // TODO: should think again
  if (opcode.find("==") != string::npos) {
    if (parcelReaderWriter->getVarCachedStatus(constraint["lhs"]) &&
        parcelReaderWriter->getVarCachedStatus(constraint["rhs"])) {
      return check(constraint);
    }
  } else {
    if (parcelReaderWriter->getVarCachedStatus(constraint["lhs"])) {
      return check(constraint);
    }
  }

  float prob = constraint["probability"].asFloat();
  float r = randomFloat(0, 1);
  if (r > prob) {
    FUZZER_LOGD(
        "We won't generate the value according to opcode(%s) as %f > %f.",
        opcode.c_str(), r, prob);
    return false;
  } else {
    FUZZER_LOGD(
        "We will generate the value according to opcode(%s) as %f <= %f.",
        opcode.c_str(), r, prob);
    return true;
  }
}

bool ConstraintChecker::getValueFromSpecialSelfConstraint(
    vector<Json::Value> constraintSet, int32_t &value) {
  int32_t specialIdx = getSpecialSelfConstraintIdx(constraintSet);
  if (specialIdx != -1) {
    if (shouldGenerateValueFromSpecialSelfConstraint(
            constraintSet[specialIdx])) {
      Json::Value constraint = constraintSet[specialIdx];
      string opcode = constraint["opcode"].asString();
      if (opcode.find("==") != string::npos) {
        value = constraint["rhs"]["value"].asInt();
      } else if (opcode == "") {
        value = 0;
      } else if (opcode == "!") {
        value = 0;
      } else {
        FUZZER_LOGE("Invalid opcode %s in getValueFromSpecialSelfConstraint.",
                    opcode.c_str());
        exit(0);
      }
      return true;
    }
  }
  return false;
}

bool ConstraintChecker::getValueFromSpecialSelfConstraint(
    vector<Json::Value> constraintSet, uint32_t &value) {
  int32_t specialIdx = getSpecialSelfConstraintIdx(constraintSet);
  if (specialIdx != -1) {
    if (shouldGenerateValueFromSpecialSelfConstraint(
            constraintSet[specialIdx])) {
      Json::Value constraint = constraintSet[specialIdx];
      string opcode = constraint["opcode"].asString();
      if (opcode.find("==") != string::npos) {
        value = constraint["rhs"]["value"].asUInt();
      } else if (opcode == "") {
        value = 0;
      } else if (opcode == "!") {
        value = 0;
      } else {
        FUZZER_LOGE("Invalid opcode %s in getValueFromSpecialSelfConstraint.",
                    opcode.c_str());
        exit(0);
      }
      return true;
    }
  }
  return false;
}

bool ConstraintChecker::getValueFromSpecialSelfConstraint(
    vector<Json::Value> constraintSet, int64_t &value) {
  int32_t specialIdx = getSpecialSelfConstraintIdx(constraintSet);
  if (specialIdx != -1) {
    if (shouldGenerateValueFromSpecialSelfConstraint(
            constraintSet[specialIdx])) {
      Json::Value constraint = constraintSet[specialIdx];
      string opcode = constraint["opcode"].asString();
      if (opcode.find("==") != string::npos) {
        value = constraint["rhs"]["value"].asInt64();
      } else if (opcode == "") {
        value = 0;
      } else if (opcode == "!") {
        value = 0;
      } else {
        FUZZER_LOGE("Invalid opcode %s in getValueFromSpecialSelfConstraint.",
                    opcode.c_str());
        exit(0);
      }
      return true;
    }
  }
  return false;
}

bool ConstraintChecker::getValueFromSpecialSelfConstraint(
    vector<Json::Value> constraintSet, unsigned long &value) {
  int32_t specialIdx = getSpecialSelfConstraintIdx(constraintSet);
  if (specialIdx != -1) {
    if (shouldGenerateValueFromSpecialSelfConstraint(
            constraintSet[specialIdx])) {
      Json::Value constraint = constraintSet[specialIdx];
      string opcode = constraint["opcode"].asString();
      if (opcode.find("==") != string::npos) {
        value = constraint["rhs"]["value"].asUInt64();
      } else if (opcode == "") {
        value = 0;
      } else if (opcode == "!") {
        value = 0;
      } else {
        FUZZER_LOGE("Invalid opcode %s in getValueFromSpecialSelfConstraint.",
                    opcode.c_str());
        exit(0);
      }
      return true;
    }
  }
  return false;
}

bool ConstraintChecker::getValueFromSpecialSelfConstraint(
    vector<Json::Value> constraintSet, string &value) {
  int32_t specialIdx = getSpecialSelfConstraintIdx(constraintSet);
  if (specialIdx != -1) {
    if (shouldGenerateValueFromSpecialSelfConstraint(
            constraintSet[specialIdx])) {
      Json::Value constraint = constraintSet[specialIdx];
      string opcode = constraint["opcode"].asString();
      if (opcode.find("==") != string::npos) {
        value = constraint["rhs"]["value"].asString();
      } else if (opcode == "") {
        value = "";
      } else if (opcode == "!") {
        value = "";
      } else {
        FUZZER_LOGE("Invalid opcode %s in getValueFromSpecialSelfConstraint.",
                    opcode.c_str());
        exit(0);
      }
      return true;
    }
  }
  return false;
}

bool ConstraintChecker::getValueFromSpecialSelfConstraint(
    vector<Json::Value> constraintSet, float &value) {
  int32_t specialIdx = getSpecialSelfConstraintIdx(constraintSet);
  if (specialIdx != -1) {
    if (shouldGenerateValueFromSpecialSelfConstraint(
            constraintSet[specialIdx])) {
      Json::Value constraint = constraintSet[specialIdx];
      string opcode = constraint["opcode"].asString();
      if (opcode.find("==") != string::npos) {
        value = constraint["rhs"]["value"].asFloat();
      } else if (opcode == "") {
        value = 0;
      } else if (opcode == "!") {
        value = 0;
      } else {
        FUZZER_LOGE("Invalid opcode %s in getValueFromSpecialSelfConstraint.",
                    opcode.c_str());
        exit(0);
      }
      return true;
    }
  }
  return false;
}

bool ConstraintChecker::getValueFromSpecialSelfConstraint(
    vector<Json::Value> constraintSet, double &value) {
  int32_t specialIdx = getSpecialSelfConstraintIdx(constraintSet);
  if (specialIdx != -1) {
    if (shouldGenerateValueFromSpecialSelfConstraint(
            constraintSet[specialIdx])) {
      Json::Value constraint = constraintSet[specialIdx];
      string opcode = constraint["opcode"].asString();
      if (opcode.find("==") != string::npos) {
        value = constraint["rhs"]["value"].asDouble();
      } else if (opcode == "") {
        value = 0;
      } else if (opcode == "!") {
        value = 0;
      } else {
        FUZZER_LOGE("Invalid opcode %s in getValueFromSpecialSelfConstraint.",
                    opcode.c_str());
        exit(0);
      }
      return true;
    }
  }
  return false;
}

bool ConstraintChecker::getValueFromSpecialSelfConstraint(
    vector<Json::Value> constraintSet, bool &value) {
  int32_t specialIdx = getSpecialSelfConstraintIdx(constraintSet);
  if (specialIdx != -1) {
    if (shouldGenerateValueFromSpecialSelfConstraint(
            constraintSet[specialIdx])) {
      Json::Value constraint = constraintSet[specialIdx];
      string opcode = constraint["opcode"].asString();
      if (opcode.find("==") != string::npos) {
        value = constraint["rhs"]["value"].asBool();
      } else if (opcode == "") {
        value = 0;
      } else if (opcode == "!") {
        value = 0;
      } else {
        FUZZER_LOGE("Invalid opcode %s in getValueFromSpecialSelfConstraint.",
                    opcode.c_str());
        exit(0);
      }
      return true;
    }
  }
  return false;
}

uint32_t ConstraintChecker::getTryCount(vector<Json::Value> constraintSet) {
  float prob = 0.5;
  if (constraintSet.size() > 0) {
    prob = constraintSet[0]["probability"].asFloat();
  }
  return prob * 100;
}
