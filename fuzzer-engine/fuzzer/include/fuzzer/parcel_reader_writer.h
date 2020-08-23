#ifndef PARCEL_READER_WRITER_H
#define PARCEL_READER_WRITER_H
#include <binder/Parcel.h>
#include <fuzzer/transaction.h>
#include <fuzzer/utils/log.h>
#include <json/json.h>
typedef enum { READ_REPLY, WRITE_DATA } ParcelReaderWriterOperation;
class ParcelReaderWriter {
public:
  ParcelReaderWriter(Json::Value &parcel, const Json::Value &variable,
                     const Json::Value &loop, const Json::Value &constraint)
      : parcel(parcel), variable(variable), loop(loop), constraint(constraint) {
  }
  ~ParcelReaderWriter() {}

  void start();
  uint32_t readWriteFrom(unsigned int begin);
  int32_t readWriteRange(uint32_t begin, uint32_t end, int32_t loopIdx);
  void readWriteItem(uint32_t varIdx);

  uint32_t getVarIdx(string varName);
  string getVarName(unsigned int idx);

  int32_t getValidLoopIdxOf(string varName);
  int32_t getCounterLoopIdx(string counterName);
  void setLoopCounter(uint32_t loopIdx, uint32_t counterValue);

  uint32_t getConstraintIdx(string constrantName);
  bool checkUnderConstraint(uint32_t varIdx);
  void resetConstraints(string constraint);
  void resetSelfConstraints(string varName);
  vector<Json::Value> selectOneSelfConstraint(const Json::Value &var);
  Json::Value getTargetConstraint(Json::Value constraint,
                                  string constraintName);

  void initWrite(Parcel *data);
  void initRead(Parcel *reply);

  void initTxWrite(Transaction *tx);
  void initTxRead(Transaction *tx);

  void initParcelableStructWrite(Parcel *data);
  void initParcelableStructRead(Parcel *reply);

  void initFlattenableStructureWrite(Parcel *data, uint32_t flattenableLevel);

  void initLightFlattenableStructureWrite(Parcel *data);
  void initLightFlattenableStructureRead(Parcel *data);

  void initRawStructWrite(Parcel *data, vector<uint8_t> *rawData);
  void initRawStructRead(Parcel *reply);

  void initFunctionWrite(Parcel *data);
  void initFunctionRead(Parcel *reply);

  void prepareDependencyRead(const Json::Value &parcel, uint32_t dependIdx,
                             Parcel *targetParcel);

  void setInterfaceToken(string interfaceToken);
  void setParcel(const Json::Value &parcel);
  bool getIsRaw();
  bool isDependencyItem(uint32_t idx);

  void storeValue(string varName, int32_t value);
  void storeValue(string varName, uint32_t value);
  void storeValue(string varName, int64_t value);
  void storeValue(string varName, uint64_t value);
  void storeValue(string varName, float value);
  void storeValue(string varName, double value);
  void storeValue(string varName, bool value);
  void storeValue(string varName, string value);

  bool getVarCachedStatus(const Json::Value &variable);
  int64_t getVarValue(string varName);
  int64_t getVarValue(const Json::Value &var);
  int64_t getVarSize(const Json::Value &variable);
  void eraseStructStatus(string prefix);

  ParcelReaderWriterOperation operation;

  Parcel *data = NULL;
  Parcel *reply = NULL;
  Parcel *targetParcel = NULL;

  string interfaceToken;
  sp<IBinder> binder;
  int exceptionCode;
  uint32_t fdCount = 0; // how many File Descriptors should be written or read?
  uint32_t flattenableLevel = 0; // the nested flattenable level
  bool isDependency = false;
  uint32_t dependIdx = -1; // default value

  bool isRaw = false;
  vector<uint8_t> *rawData;

  map<string, bool> isVarCached;

  map<string, int32_t> intMap;
  map<string, uint32_t> uintMap;
  map<string, int64_t> int64Map;
  map<string, uint64_t> uint64Map;
  map<string, float> floatMap;
  map<string, double> doubleMap;
  map<string, size_t> sizetMap;
  map<string, status_t> statusMap;
  map<string, bool> boolMap;
  map<string, string> stringMap;

  map<string, bool> constraintStatus;
  Json::Value &parcel;
  const Json::Value &variable;
  const Json::Value &loop;
  const Json::Value &constraint;

private:
  // Auxiliary variable
  // check if loop has already visited.
  map<uint32_t, bool> loopVisit;
};
#endif // PARCEL_READER_WRITER_H