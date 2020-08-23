#ifndef DEPENDENCY_SOLVER_H
#define DEPENDENCY_SOLVER_H

#include <binder/Parcel.h>
#include <fuzzer/transaction.h>
#include <json/json.h>
typedef enum {
  COMMON_DEPENDENCY,
  BINDER_DEPENDENCY,
  STRUCTURE_ITEM_DEPENDENCY
} DependencyType;

class DependencySolver {
  Json::Value dependency;
  Parcel *targetParcel;
  DependencyType dependencyType;

public:
  DependencySolver(Parcel *targetParcel, DependencyType dependencyType,
                   const Json::Value &dependencySet);

  bool canUseDependency();
  void solveStructDependency();
  void solveTxDependency();
  void solve();
  sp<IBinder> binder;
};

#endif // DEPENDENCY_SOLVER_H