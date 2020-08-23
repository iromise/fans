#include <fuzzer/interface/interface.h>

#include <fuzzer/executor.h>
#include <fuzzer/generator.h>

#include <fuzzer/test.h>
#include <fuzzer/types/types.h>
#include <fuzzer/utils/java_vm.h>
#include <fuzzer/utils/log.h>
#include <stdio.h>
#include <unistd.h>

char *targetInterface = NULL;
char *targetTransaction = NULL;

void init(char *svcDir, char *structDir) {
  loadServiceInfo(svcDir, targetInterface, targetTransaction);
  loadStructureInfo(structDir);
  loadUnionInfo((char *)UNION_INFO_DIR);
  initEnumInfo((char *)ENUMERATION_INFO_DIR);
  loadFunctionInfo((char *)FUNCTION_INFO_DIR);

  initPackageNameList();
  initPermissionNameList();
  initMediaUrlList();

  initFDPool();
  initVarTypeMap();

  initCodecInfo();
  init_jvm(&vm, &env);
}
void startFuzzing() {
  FUZZER_LOGI("Start fuzzing...");
  Generator gen;
  Executor executor;
  status_t ret;
  // string stop = "Y";
  while (true) {
    FUZZER_LOGI(
        "-------------------------------------------------------------------");
    Transaction tx = gen.generateTx();
    ret = executor.run(tx);
    // sleep(1);
    // if (stop == "Y") {
    //   cin >> stop;
    // }
    FUZZER_LOGI(
        "-------------------------------------------------------------------");
  }
}

int main(int argc, char *argv[]) {

  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  const char *optstring = "";
  static struct option long_options[] = {
      {"log_level", required_argument, NULL, 'l'},
      {"interface", required_argument, NULL, 'i'},
      {"transaction", required_argument, NULL, 't'},
      {"help", no_argument, NULL, 'h'},
      {0, 0, 0, 0}};
  int opt;
  int option_index = 0;
  while ((opt = getopt_long(argc, argv, optstring, long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 'l': {
      printf("Set log level : %s.\n", optarg);
      if (!strncmp(optarg, "info", 4)) {
        logLevel = INFO_LEVEL;
      } else if (!strncmp(optarg, "debug", 5)) {
        logLevel = DEBUG_LEVEL;
      } else if (!strncmp(optarg, "error", 5)) {
        logLevel = ERROR_LEVEL;
      } else {
        FUZZER_LOGI("Unknown log option %s.", optarg);
        exit(0);
      }
      break;
    }
    case 'i': {
      printf("Set target interface : %s.\n", optarg);
      targetInterface = optarg;
      break;
    }
    case 't': {
      printf("Set target transaction: %s.\n", optarg);
      targetTransaction = optarg;
      break;
    }
    case '?':
    case 'h': {
      char *help =
          (char *)"Usage: ./native_service_fuzzer [OPTION]\n"
                  "\n"
                  "  --log_level       specify the log level of fuzzer\n"
                  "  --interface       specify the target interface to "
                  "fuzz\n"
                  "  --transaction     specify the target transaction to "
                  "fuzz\n"
                  "  --help            help manual\n";
      printf("%s", help);
      exit(0);
    }
    default:
      abort();
    }
  }
  if (targetInterface && targetTransaction) {
    FUZZER_LOGE(
        "Can not specify interface and transaction options at the same time.");
    exit(0);
  }
  init((char *)FUZZER_PATH "model/service/",
       (char *)FUZZER_PATH "model/structure/");

  // testIsCryptoSchemeSupported();
  // testIsCryptoSchemeSupportedForMimeType();
  // testOpenSeesion();
  // testCloseSession();
  startFuzzing();
  return 0;
}
