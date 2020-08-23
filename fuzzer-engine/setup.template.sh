source build/envsetup.sh
lunch 50 # for taimen
rm -rf frameworks/native/cmds/native-service-fuzzer
cp -r /path/to/fans/fuzzer-engine/fuzzer  frameworks/native/cmds/native-service-fuzzer
cd frameworks/native/cmds/native-service-fuzzer
mm