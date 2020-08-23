# Fuzzer Engine
Fuzzer engine consists of two parts.

- fuzzer, the actual fuzzer to fuzz the Android native system services.
- manager, it manages to fuzz Android native system services automatically to some extent.
  - push fuzzer and data to smartphone
  - sync crash logs, e.g., logcat logs, tombstone logs
  - flash smartphone 
  - etc.

## Prepare Data
Before fuzzing, we should prepare data used by the fuzzer, including
- interface model, locates in `workdir/interface-model-extractor/model`
- various seeds, located in `seed`, e.g.,
  - files
    - media files, located in `seed/files/media`. You can add media files according to your demand, e.g., mp3, mp4. 
    - apk files, located in `seed/files/apk`. You can add apk files according to your demand. Here we give a simple file, which is from https://www.appsapk.com/compass/.
    - misc files, located in `seed/files/misc`. You can add misc files according to your demand.
  - media URLs, located in `seed/media_url_list.txt`. You can add URLs according to your demand.
  - package name list, located in `seed/package_list.txt`. You can use `adb shell pm list packages |cut -f 2 -d :` to get the packages installed on your device.
  - permission list, locates in `seed/permission_list.txt`. You could use `adb shell pm list permissions -g |sed  "s/  permission://g" | grep -v "All Permissions:\|group:\|ungrouped:\|^$" > permission_list.txt` to collect the permissions provided by your device.


## Build Fuzzer

Before building the fuzzer, please modify the option `include_dirs` in `fuzzer-engine/fuzzer/Android.bp`. You need to change `"out/target/product/taimen/obj/STATIC_LIBRARIES/libwificond_ipc_intermediates/aidl-generated/include"` to the corresponding directory of your target mobile phone. This directory is a relative directory related to the AOSP directory. Besides, it is generated when compiling the AOSP.

Here we provide a handful template `setup.template.sh` to build the fuzzer. You can create `setup.sh` according to your environment. Then you can build the fuzzer with ASan enabled as follows. 
```bash
# create a symbolic link inside the AOSP_ASan dir.
cd /path/to/aosp_asan
ln -s /path/to/fans/fuzzer-engine/setup.sh .
# build
bash
source setup.sh
```

## Test Fuzzer
Currently, fuzzer supports several options as follows
```bash
$ ./native_service_fuzzer --help
Usage: ./native_service_fuzzer [OPTION]

  --log_level       specify the log level of fuzzer
  --interface       specify the target interface to fuzz
  --transaction     specify the target transaction to fuzz
  --help            help manual
```

For example, after pushing fuzzer and related data to the `/data/fuzzer` dir of the device, we can use the following commands to test `IDrm` interface and `IDrm::13-13` transaction correspondingly. Note that we can not specify the interface option and transaction option at the same time.

```bash
adb shell "./data/fuzzer/native_service_fuzzer --log_level=debug --interface=IDrm"
adb shell "./data/fuzzer/native_service_fuzzer --log_level=debug --transaction=IDrm::13-13"
```

Here we also provide a handful template `test_fuzzer.template.sh` to test the fuzzer to check if it works as expected. You can create `test_fuzzer.sh` according to your environment. In the script, 

- `$1` should be the device serial
- If you want to push data and fuzzer to the device, you should set `$2` to "push".

Here is an example of how to use `test_fuzzer.sh`.
```bash
# push data and test fuzzer.
# XXXXXXXXXXXXXX should be the serial number of your device
sh test_fuzzer.sh XXXXXXXXXXXXXX push
```

Besides, if you find some problems with fuzzer, you can also use `debug_fuzzer.sh` to debug fuzzer.

## Run Fuzzer Manager
Please refer to the [Fuzzer Manager](manager/readme.md).