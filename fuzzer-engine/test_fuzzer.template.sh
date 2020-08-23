adb -s $1 root
sleep 3
adb -s $1 shell setenforce 0
if [ "$2" = "push" ]; then
  adb -s $1 shell rm -rf /data/fuzzer/
  adb -s $1 shell mkdir /data/fuzzer/
  adb -s $1 push ../workdir/interface-model-extractor/model /data/fuzzer/
  adb -s $1 push ../seed/ /data/fuzzer
  adb -s $1 push /path/to/aosp_asan/out/target/product/taimen/symbols/system/bin/native_service_fuzzer /data/fuzzer
fi
adb -s $1 shell killall native_service_fuzzer
# adb -s $1 shell "./data/fuzzer/native_service_fuzzer --log_level=info --interface=IDrm"
# adb -s $1 shell "./data/fuzzer/native_service_fuzzer --log_level=info --transaction=IDrm::13-13"