#include <fuzzer/dependency_solver.h>
#include <fuzzer/executor.h>
#include <fuzzer/service.h>
#include <fuzzer/utils/random.h>
DependencySolver::DependencySolver(Parcel *targetParcel,
                                   DependencyType dependencyType,
                                   const Json::Value &dependencySet)
    : targetParcel(targetParcel), dependencyType(dependencyType) {
  uint32_t possIdx = (uint32_t)randomUInt64(0, dependencySet.size() - 1);
  dependency = dependencySet[possIdx];
}
bool DependencySolver::canUseDependency() {
  string txName = dependency["name"].asString();
  if (usedTxs.size() >= 10 && dependencyType != BINDER_DEPENDENCY) {
    FUZZER_LOGD("There are too many Txs used when solving dependency.");
    for (auto tx : usedTxs) {
      FUZZER_LOGD("usedTxs: %s", tx.first.c_str());
    }
    return false;
  } else if (usedTxs.find(txName) == usedTxs.end()) {
    FUZZER_LOGD("Tx %s hasn't been used.", txName.c_str());
    if (dependency["type"].asString() == "Structure") {
      // use the structure dependency(a variable depends on another variable in
      // a structure) with a low probability.
      return !randomUInt64(0, 9);
    } else {
      return true;
    }

  } else {
    FUZZER_LOGD(
        "Tx %s has been used, so we can not utilize it to generate other "
        "variables.",
        txName.c_str());
    return false;
  }
}
void DependencySolver::solveStructDependency() {
  string structName = dependency["name"].asString();
  FUZZER_LOGI("Dependent structure name: %s.", structName.c_str());
  if (parcelableStructureReplyInfo.count(structName) == 0) {
    FUZZER_LOGE("Structure %s does not exist.", structName.c_str());
    exit(0);
  }

  Json::Value structure = parcelableStructureReplyInfo[structName];

  // get the tx that this structure depends.
  Json::Value txDependency = structure["dependency"];
  Parcel tmp;
  DependencySolver solver(&tmp, COMMON_DEPENDENCY, txDependency);
  // read target structure to tmp parcel.
  solver.solve();
  tmp.setDataPosition(0);

  // read target structure item.
  uint32_t possIdx = dependency["possIdx"].asUInt();

  Json::Value &info = structure["possibility"][possIdx];
  Json::Value &variable = structure["variable"];
  Json::Value &loop = structure["loop"];
  Json::Value &constraint = structure["constraint"];

  ParcelReaderWriter parcelReaderWriter(info["reply"], variable, loop,
                                        constraint);
  parcelReaderWriter.initParcelableStructRead(&tmp);
  parcelReaderWriter.prepareDependencyRead(
      info["reply"], dependency["varIdx"].asUInt(), targetParcel);
  parcelReaderWriter.start();

  return;
}
void DependencySolver::solveTxDependency() {

  string txName = dependency["name"].asString();
  FUZZER_LOGI("Dependent transaction name: %s.", txName.c_str());
  if (svcInfo.count(txName) == 0) {
    FUZZER_LOGE("Transaction %s does not exist.", txName.c_str());
    exit(0);
  }

  Json::Value dependencyTxInfo = svcInfo[txName];
  uint32_t possIdx = dependency["possIdx"].asUInt();
  Transaction dependencyTx(txName, dependencyTxInfo, possIdx);
  ParcelReaderWriter parcelReaderWriter(
      dependencyTx.info["data"], dependencyTx.variable, dependencyTx.loop,
      dependencyTx.constraint);
  parcelReaderWriter.initTxWrite(&dependencyTx);
  parcelReaderWriter.start();

  // run selected tx
  Executor executor;
  dependencyTx.flags = 0; // sync call as this is a dependency.
  executor.run(dependencyTx);

  // read target
  // we should still use the parcelReaderWriter as constraint status is recorded
  // in it
  parcelReaderWriter.initTxRead(&dependencyTx);
  parcelReaderWriter.prepareDependencyRead(
      dependencyTx.info["reply"], dependency["varIdx"].asUInt(), targetParcel);
  parcelReaderWriter.start();
  if (dependencyType == BINDER_DEPENDENCY) {
    binder = parcelReaderWriter.binder;
  }
  return;
}

void DependencySolver::solve() {
  FUZZER_LOGI("Generate through dependency.");
  spaceNum += 2;
  string type = dependency["type"].asString();
  if (type == "Transaction") {
    solveTxDependency();
  } else if (type == "Structure") {
    solveStructDependency();
  } else {
    FUZZER_LOGE("Unexpected depdendency type %s.", type.c_str());
    exit(0);
  }
  spaceNum -= 2;
}