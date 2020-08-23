
#include <fuzzer/constraint_checker.h>
#include <fuzzer/parcel_reader.h>
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/parcel_writer.h>
#include <fuzzer/types/types.h>

void ParcelReaderWriter::initWrite(Parcel *data) {
  operation = WRITE_DATA;
  this->data = data;
  this->reply = NULL;
}
void ParcelReaderWriter::initRead(Parcel *reply) {
  operation = READ_REPLY;
  this->data = NULL;
  this->reply = reply;
}

void ParcelReaderWriter::initTxWrite(Transaction *tx) {
  this->interfaceToken = tx->interfaceToken;
  initWrite(&tx->data);
}
void ParcelReaderWriter::initTxRead(Transaction *tx) { initRead(&tx->reply); }

void ParcelReaderWriter::initLightFlattenableStructureWrite(Parcel *data) {
  initWrite(data);
}
void ParcelReaderWriter::initLightFlattenableStructureRead(Parcel *reply) {
  initRead(reply);
}

void ParcelReaderWriter::initFlattenableStructureWrite(
    Parcel *data, uint32_t flattenableLevel) {
  initWrite(data);
  this->flattenableLevel = flattenableLevel;
}

void ParcelReaderWriter::initParcelableStructWrite(Parcel *data) {
  initWrite(data);
}
void ParcelReaderWriter::initParcelableStructRead(Parcel *reply) {
  initRead(reply);
}

void ParcelReaderWriter::initRawStructWrite(Parcel *data,
                                            vector<uint8_t> *rawData) {
  initWrite(data);
  this->rawData = rawData;
  isRaw = true;
}
void ParcelReaderWriter::initRawStructRead(Parcel *reply) { initRead(reply); }

void ParcelReaderWriter::initFunctionWrite(Parcel *data) { initWrite(data); }
void ParcelReaderWriter::initFunctionRead(Parcel *reply) { initRead(reply); }

void ParcelReaderWriter::setInterfaceToken(string interfaceToken) {
  this->interfaceToken = interfaceToken;
}

void ParcelReaderWriter::prepareDependencyRead(const Json::Value &parcel,
                                               uint32_t dependIdx,
                                               Parcel *targetParcel) {
  this->parcel = parcel;
  this->dependIdx = dependIdx;
  this->targetParcel = targetParcel;
}

void ParcelReaderWriter::setParcel(const Json::Value &parcel) {
  this->parcel = parcel;
}

bool ParcelReaderWriter::getIsRaw() { return isRaw; }

void ParcelReaderWriter::start() {
  uint32_t idx = 0;
  uint32_t cnt = 0;
  if (operation == WRITE_DATA) {
    FUZZER_LOGD("Start writing Data Parcel.");
  } else {
    FUZZER_LOGD("start reading Reply Parcel.");
  }
  spaceNum += 2;
  while (idx < parcel.size()) {
    FUZZER_LOGI("Current item idx: %u", idx);
    spaceNum += 2;
    // update dependency status.
    this->isDependency = isDependencyItem(idx);

    // When dealing with dependency, we can only read item before dependIdx.
    if (this->isDependency) {
      if (operation != READ_REPLY) {
        FUZZER_LOGE(
            "The operation should be READ_REPLY when dealing with dependency.");
        exit(0);
      }
      // 1. when dependIdx==UINT32_MAX, it means that all of the item in the
      // parcel should be read this is designed to support structure, function
      // type variable.
      // 2. when dependIdx<UINT32_MAX, it means that we should only read the
      // item at the dependIdx.
      if (this->dependIdx != UINT32_MAX && idx > this->dependIdx) {
        // TODO: maybe we should consider a better way to handle this situation.
        FUZZER_LOGI("Dependent item has already been read, we do not need to "
                    "handle this item.");
        spaceNum -= 2;
        goto out;
      }
    }

    cnt = readWriteFrom(idx);
    idx += cnt;
    spaceNum -= 2;
  }
out:
  spaceNum -= 2;
  if (operation == WRITE_DATA) {
    FUZZER_LOGD("Finish writing data parcel.");
  } else {
    FUZZER_LOGD("Finish reading reply parcel.");
  }
}

uint32_t ParcelReaderWriter::readWriteFrom(unsigned int begin) {
  string varName = parcel[begin].asString();
  int32_t loopIdx = getValidLoopIdxOf(varName);
  if (loopIdx == -1) {
    // This means this variable is not inside valid loop currently.
    // We can generate it directly.
    string varName = parcel[begin].asString();
    uint32_t varIdx = getVarIdx(varName);
    readWriteItem(varIdx);
    return 1;
  } else {
    uint32_t end = begin + 1;
    FUZZER_LOGD(
        "Find all the following variables which are in the same loop with %s.",
        varName.c_str());
    while (end < parcel.size() &&
           loopIdx == getValidLoopIdxOf(parcel[end].asString())) {
      end += 1;
    }
    FUZZER_LOGD("Var %s - Var %s are in the same loop.",
                parcel[begin].asCString(), parcel[end - 1].asCString());
    FUZZER_LOGD("Mark loop %d is visited.", loopIdx);
    loopVisit[loopIdx] = true;
    uint32_t success = readWriteRange(begin, end, loopIdx);
    FUZZER_LOGD("Recover the status of loop %d as there migh exist nest loop.",
                loopIdx);
    // e.g.
    // for i in loop1:
    //  read(x)
    //  for j in loop2:
    //    read(y)
    // x:loop1
    // y:loop1-loop2
    loopVisit[loopIdx] = false;
    if (success) {
      return end - begin;
    } else {
      return -1;
    }
  }
}

void ParcelReaderWriter::setLoopCounter(uint32_t loopIdx,
                                        uint32_t counterValue) {
  string counterName = loop[loopIdx]["counter"]["name"].asString();
  VarType counterType = varTypeMap[loop[loopIdx]["counter"]["type"].asString()];

  if (counterType == INT32_TYPE) {
    storeValue(counterName, (int)counterValue);
  } else if (counterType == UINT32_TYPE) {
    storeValue(counterName, counterValue);
  } else if (counterType == UINT64_TYPE) {
    storeValue(counterName, (uint64_t)counterValue);
  } else {
    FUZZER_LOGE("Unexpected counter type %s meeted in setLoopCounter.",
                loop[loopIdx]["counter"]["type"].asCString());
    exit(0);
  }
}

int32_t ParcelReaderWriter::readWriteRange(uint32_t begin, uint32_t end,
                                           int32_t loopIdx) {
  FUZZER_LOGD("Init the counter of the loop %s.",
              loop[loopIdx]["name"].asCString());
  string counterName = loop[loopIdx]["counter"]["name"].asString();
  uint32_t counterValue = loop[loopIdx]["counter"]["value"].asUInt();
  setLoopCounter(loopIdx, counterValue);

  string constraint = loop[loopIdx]["constraint"].asString();
  Json::Value inc = loop[loopIdx]["inc"];
  string incOpcode = inc["opcode"].asString();
  string incVarName = inc["name"].asString();

  // be careful with those big loop, such as finish value is -1.
  uint32_t count = 0;
  FUZZER_LOGD("Start generating item in loop %s.",
              loop[loopIdx]["name"].asCString());
  ConstraintChecker checker(this);
  while (checker.check(constraint)) {
    FUZZER_LOGD("counter %d", counterValue)
    for (uint32_t i = begin; i < end; ++i) {
      FUZZER_LOGD("begin-current-end, %d-%d-%d", begin, i, end);
      uint32_t success = readWriteFrom(i);
      if (success) {
        continue;
      } else {
        return -1;
      }
    }
    // update counter.
    if (incOpcode == "++") {
      counterValue += 1;
      setLoopCounter(loopIdx, counterValue);
    } else {
      FUZZER_LOGE("Unexpected inc opcode.");
      exit(0);
    }
    count += 1;
    // this perhaps means that the loop is very big
    // but, in fact, there won't be so large loop?
    // TODO: how to handle it better?
    if (count > 65537) {
      break;
    }
  }
  return 1;
}

bool endsWith(const string &a, const string &b) {
  if (b.size() > a.size())
    return false;
  return std::equal(a.begin() + a.size() - b.size(), a.end(), b.begin());
}

void ParcelReaderWriter::resetConstraints(string constraint) {
  this->constraintStatus.erase(constraint);
  if (endsWith(constraint, "_lhs") || endsWith(constraint, "_rhs")) {
    string prefix = constraint.substr(0, constraint.size() - 4);
    FUZZER_LOGD("Reset constraint %s.", prefix.c_str());
    resetConstraints(prefix);
  }
}
// clear related constraint....
void ParcelReaderWriter::resetSelfConstraints(string varName) {
  uint32_t varIdx = getVarIdx(varName);
  Json::Value selfConstraint = variable[varIdx]["self_constraint"];
  for (uint32_t i = 0; i < selfConstraint.size(); ++i) {
    FUZZER_LOGD(
        "Reset variable %s's self constraint %s and related constraint(s).",
        varName.c_str(), selfConstraint[i].asCString());
    resetConstraints(selfConstraint[i].asString());
    // this->constraintStatus.erase(selfConstraint[i].asString());
  }
}

bool ParcelReaderWriter::checkUnderConstraint(uint32_t varIdx) {
  FUZZER_LOGD("Check whether constraints under other variable are satisfied.");
  spaceNum += 2;
  const Json::Value &underConstraint = variable[varIdx]["under_constraint"];
  bool flag = true;
  for (uint32_t i = 0; i < underConstraint.size(); ++i) {
    string consName = underConstraint[i]["name"].asString();

    ConstraintChecker checker(this);
    bool result = checker.check(consName);
    // if (checker.random) {
    //   FUZZER_LOGD("%s is not ready, we treat it as satisfied.",
    //               consName.c_str());
    //   continue;
    // }
    bool expected = underConstraint[i]["status"].asBool();
    if (expected != result) {
      flag = false;
    }
  }
  spaceNum -= 2;
  if (flag) {
    FUZZER_LOGD("Under constraint is satisfied.");
  } else {
    FUZZER_LOGD("Under constraint is not satisfied.");
  }
  return flag;
}

vector<Json::Value>
ParcelReaderWriter::selectOneSelfConstraint(const Json::Value &var) {
  vector<Json::Value> result;
  uint32_t consLen = var["self_constraint"].size();
  if (consLen == 0)
    return result;
  uint32_t consIdx = (uint32_t)randomUInt64(0, consLen - 1);
  string constraintName = var["self_constraint"][consIdx].asString();
  uint32_t constraintIdx = getConstraintIdx(constraintName);
  result.push_back(
      getTargetConstraint(constraint[constraintIdx], constraintName));
  return result;
}

Json::Value ParcelReaderWriter::getTargetConstraint(Json::Value constraint,
                                                    string constraintName) {
  Json::Value result;
  string opcode = constraint["opcode"].asString();
  if (opcode == "&&" || opcode == "||") {
    Json::Value lhs = constraint["lhs"];
    Json::Value rhs = constraint["rhs"];

    Json::Value tmp = getTargetConstraint(lhs, constraintName);
    if (tmp.isMember("name")) {
      result = tmp;
    }
    tmp = getTargetConstraint(rhs, constraintName);
    if (tmp.isMember("name")) {
      result = tmp;
    }
  } else {
    string name = constraint["name"].asString();
    if (name == constraintName || name == constraintName) {
      result = constraint;
    }
  }
  return result;
}

bool ParcelReaderWriter::isDependencyItem(uint32_t idx) {
  if (idx == this->dependIdx || this->isDependency) {
    return true;
  } else {
    return false;
  }
}

void ParcelReaderWriter::readWriteItem(uint32_t varIdx) {
  if (checkUnderConstraint(varIdx)) {
    if (operation == WRITE_DATA) {
      ParcelWriter parcelWriter(this, variable[varIdx]);
      parcelWriter.write();
    } else {
      ParcelReader parcelReader(this, variable[varIdx]);
      parcelReader.read();
    }
  }
}

void ParcelReaderWriter::eraseStructStatus(string prefix) {

  for (map<string, bool>::iterator iter = isVarCached.begin();
       iter != isVarCached.end();) {
    if (iter->first.rfind(prefix, 0) == 0) {
      FUZZER_LOGD("Erasing struct member %s.", iter->first.c_str());
      isVarCached.erase(iter++);
    } else {
      ++iter;
    }
  }

  for (map<string, bool>::iterator iter = this->constraintStatus.begin();
       iter != this->constraintStatus.end();) {

    if (iter->first.rfind(prefix, 0) == 0) {
      FUZZER_LOGD("Erasing struct related constraint status %s.",
                  iter->first.c_str());
      this->constraintStatus.erase(iter++);
    } else {
      ++iter;
    }
  }
}

uint32_t ParcelReaderWriter::getVarIdx(string varName) {
  for (uint32_t i = 0; i < variable.size(); ++i) {
    if (varName == variable[i]["name"].asString()) {
      return i;
    }
  }
  Json::StyledWriter writer;
  const std::string variabeTable = writer.write(variable);
  FUZZER_LOGE("Var %s do not exist in variable symbol table\n%s.",
              varName.c_str(), variabeTable.c_str());
  exit(0);
}
bool ParcelReaderWriter::getVarCachedStatus(const Json::Value &variable) {
  string varType = variable["type"].asString();
  if (varType == "IntegerLiteral")
    return true;
  if (variable.isMember("value"))
    return true;
  string varName = variable["name"].asString();
  if (isVarCached.count(varName)) {
    return isVarCached[varName];
  } else {
    return false;
  }
}

int64_t ParcelReaderWriter::getVarValue(string varName) {
  uint32_t idx = getVarIdx(varName);
  Json::Value var = variable[idx];
  return getVarValue(var);
}

int64_t ParcelReaderWriter::getVarValue(const Json::Value &var) {
  string varName = var["name"].asString();
  VarType varType = getVarTypeEnum(var["type"].asString());
  if (isVarCached[varName]) {
    FUZZER_LOGD("Variable %s is cached.", varName.c_str());
    if (varType == INT32_TYPE) {
      return intMap[varName];
    } else if (varType == UINT32_TYPE) {
      return uintMap[varName];
    } else if (varType == INT64_TYPE) {
      return int64Map[varName];
    } else if (varType == UINT64_TYPE) {
      return uint64Map[varName];
    } else if (varType == INTEGER_LITERAL) {
      return var["value"].asInt();
    } else if (varType == BOOL_TYPE) {
      return boolMap[varName];
    } else {
      FUZZER_LOGD("Unexpected type %s meeted when geting var value.",
                  var["type"].asCString());
      exit(0);
    }
  } else {
    if (varType == INTEGER_LITERAL) {
      return var["value"].asInt();
    } else if (varType == BOOL_TYPE && var.isMember("value")) {
      return var["value"].asBool();
    } else {
      FUZZER_LOGD("Variable %s is not cached, we return default value -1.",
                  var["name"].asCString());
      return -1;
    }
  }
}
int64_t ParcelReaderWriter::getVarSize(const Json::Value &var) {
  FUZZER_LOGD("Get size of variable %s.", var["name"].asCString());
  int64_t value;
  if (var["size"].isString()) {
    FUZZER_LOGD("The size of variable is determined by variable %s.",
                var["size"].asCString());
    string sizeName = var["size"].asString();
    value = getVarValue(sizeName);
  } else {
    value = var["size"].asInt64();
  }
  FUZZER_LOGD("The size of variable %s is %ld.", var["name"].asCString(),
              value);
  return value;
}

int32_t ParcelReaderWriter::getValidLoopIdxOf(string varName) {
  FUZZER_LOGD("Get valid loop idx of variable %s.", varName.c_str());
  uint32_t idx = getVarIdx(varName);
  for (int i = 0; i < (int32_t)variable[idx]["loop"].size(); ++i) {
    // get loopIdx in loop json info
    for (int j = 0; j < (int)loop.size(); ++j) {
      if (variable[idx]["loop"][i].asString() == loop[j]["name"].asString()) {
        FUZZER_LOGD("%s is in loop %s.", varName.c_str(),
                    variable[idx]["loop"][i].asCString());
        // check if this loop has already been visited..
        if (loopVisit.count(j) == 0 || loopVisit[j] == false) {
          FUZZER_LOGD("loop %s is valid.",
                      variable[idx]["loop"][i].asCString());
          return j;
        } else {
          FUZZER_LOGD("loop %s has already been visited.",
                      variable[idx]["loop"][i].asCString());
        }
      }
    }
  }
  FUZZER_LOGD("There is no valid loop idx of variable %s.", varName.c_str());
  return -1;
}

int32_t ParcelReaderWriter::getCounterLoopIdx(string counterName) {
  for (uint32_t i = 0; i < loop.size(); ++i) {
    if (loop[i]["counter"]["name"].asString() == counterName) {
      return i;
    }
  }
  return -1;
}

uint32_t ParcelReaderWriter::getConstraintIdx(string constraintName) {
  for (uint32_t i = 0; i < constraint.size(); ++i) {
    if (constraintName.rfind(constraint[i]["name"].asString(), 0) == 0) {
      return i;
    }
  }
  FUZZER_LOGE("Constraint %s do not exists in variable.",
              constraintName.c_str());
  exit(0);
}

void ParcelReaderWriter::storeValue(string varName, uint32_t value) {
  isVarCached[varName] = 1;
  uintMap[varName] = value;
  resetSelfConstraints(varName);
}
void ParcelReaderWriter::storeValue(string varName, int32_t value) {
  isVarCached[varName] = 1;
  intMap[varName] = value;
  resetSelfConstraints(varName);
}

void ParcelReaderWriter::storeValue(string varName, int64_t value) {
  isVarCached[varName] = 1;
  int64Map[varName] = value;
  resetSelfConstraints(varName);
}
void ParcelReaderWriter::storeValue(string varName, uint64_t value) {
  isVarCached[varName] = 1;
  uint64Map[varName] = value;
  resetSelfConstraints(varName);
}

void ParcelReaderWriter::storeValue(string varName, float value) {
  isVarCached[varName] = 1;
  floatMap[varName] = value;
  resetSelfConstraints(varName);
}

void ParcelReaderWriter::storeValue(string varName, double value) {
  isVarCached[varName] = 1;
  doubleMap[varName] = value;
  resetSelfConstraints(varName);
}

void ParcelReaderWriter::storeValue(string varName, bool value) {
  isVarCached[varName] = 1;
  boolMap[varName] = value;
  resetSelfConstraints(varName);
}

void ParcelReaderWriter::storeValue(string varName, string value) {
  isVarCached[varName] = 1;
  stringMap[varName] = value;
  resetSelfConstraints(varName);
}