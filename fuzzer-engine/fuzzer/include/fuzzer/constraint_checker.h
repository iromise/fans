#ifndef CONSTRAINT_CHECKER_H
#define CONSTRAINT_CHECKER_H
#include <fuzzer/parcel_reader_writer.h>
#include <fuzzer/transaction.h>
#include <fuzzer/utils/log.h>
#include <json/json.h>
#include <string>
using namespace std;
class ConstraintChecker {
  ParcelReaderWriter *parcelReaderWriter;

public:
  ConstraintChecker(ParcelReaderWriter *parcelReaderWriter);
  bool check(const string &constraintName);
  bool check(const Json::Value &constraint);
  bool checkNot(Json::Value constraint);
  bool checkSelf(Json::Value constraint);

  template <typename T1, typename T2>
  bool compare(string opcode, const T1 &left, const T2 &right);
  bool checkConstraintSet(vector<Json::Value> constraintSet);

  int32_t getSpecialSelfConstraintIdx(vector<Json::Value> constraintSet);
  bool
  shouldGenerateValueFromSpecialSelfConstraint(const Json::Value &constraint);
  // template <typename T>
  // bool getValueFromSpecialSelfConstraint(vector<Json::Value> constraintSet, T
  // &value);
  bool getValueFromSpecialSelfConstraint(vector<Json::Value> constraintSet,
                                         int32_t &value);
  bool getValueFromSpecialSelfConstraint(vector<Json::Value> constraintSet,
                                         int64_t &value);
  bool getValueFromSpecialSelfConstraint(vector<Json::Value> constraintSet,
                                         unsigned long &value);
  bool getValueFromSpecialSelfConstraint(vector<Json::Value> constraintSet,
                                         uint32_t &value);
  bool getValueFromSpecialSelfConstraint(vector<Json::Value> constraintSet,
                                         float &value);
  bool getValueFromSpecialSelfConstraint(vector<Json::Value> constraintSet,
                                         double &value);
  bool getValueFromSpecialSelfConstraint(vector<Json::Value> constraintSet,
                                         string &value);
  bool getValueFromSpecialSelfConstraint(vector<Json::Value> constraintSet,
                                         bool &value);
  uint32_t getTryCount(vector<Json::Value> constraintSet);

  bool random = false;
};

#endif // CONSTRAINT_CHECKER_H