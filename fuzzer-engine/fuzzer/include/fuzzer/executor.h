#ifndef EXECUTOR_H
#define EXECUTOR_H

#include <fuzzer/transaction.h>
#include <fuzzer/types/types.h>

#include <binder/IServiceManager.h>
#include <binder/Parcel.h>
#include <binder/ProcessState.h>
#include <binder/TextOutput.h>
#include <fstream>
#include <getopt.h>
#include <json/json.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

class Executor {
public:
  status_t run(Transaction &tx);
};

#endif // EXECUTOR_H