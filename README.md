# FANS: Fuzzing Android Native System Services
FANS is a fuzzing tool for fuzzing Android native system services. It contains four components: interface collector, interface model extractor, dependency inferer, and fuzzer engine.

For more details, please refer to our [USENIX Security'20 paper](https://www.usenix.org/conference/usenixsecurity20/presentation/liu).

You could follow the following steps to setup FANS. In the following, we use Pixel 2 XL to illustrate the instructions.

## Prepare Host
Please prepare a server with

- at least 1T disk (preferably SSD) as the following reasons
  - We should separate the AOSP projects with/without ASan enabled.
  - We need to save the logs.
  - etc.
- many cores as compiling AOSP is time-consuming. The more cores, the better.

We suggest using FANS on Ubuntu. We tested it on Ubuntu 18.04.

## Prepare Android Environment

Please refer to [AOSP](https://source.android.com/) for
- how to download AOSP source code
- how to compile AOSP for the target mobile phone with the target version (e.g., Android 9.0.0_r46 for Pixel 2 XL)
- how to compile AOSP with ASan enabled
- how to flash devices

Suppose we have
- downloaded AOSP source code to `/path/to/aosp`.
- checkout to the target version, e.g., Android 9.0.0_r46.
- downloaded the proprietary binaries of the target mobile phone to `/path/to/aosp` according to the following URLs
  -  https://source.android.com/setup/start/build-numbers#source-code-tags-and-builds
  -  https://developers.google.com/android/drivers

Before building, we'd better modify some options in `/path/to/aosp/build/core/main.mk` to make fuzzing more convenient.

- ro.adb.secure=0, which will disable adb authentication. Otherwise, every time we reflash the phone, we need to click the screen manually to trust the host. Disabling adb authentication will help us reflash the mobile automatically as we will reflash the mobile phone through adb.

- persist.sys.disable_rescue=1, which will disable rescue party. For more details, please see https://source.android.com/devices/tech/debug/rescue-party. This will improve fuzzing efficiency.

```
# line 273
## before modifying
ifneq (,$(user_variant))
  # Target is secure in user builds.
  ADDITIONAL_DEFAULT_PROPERTIES += ro.secure=1
  ADDITIONAL_DEFAULT_PROPERTIES += security.perf_harden=1

  ifeq ($(user_variant),user)
    ADDITIONAL_DEFAULT_PROPERTIES += ro.adb.secure=1
  endif
## after modifying
ifneq (,$(user_variant))
  # Target is secure in user builds.
  ADDITIONAL_DEFAULT_PROPERTIES += ro.secure=1
  ADDITIONAL_DEFAULT_PROPERTIES += security.perf_harden=1

  ADDITIONAL_DEFAULT_PROPERTIES += ro.adb.secure=0
  ADDITIONAL_DEFAULT_PROPERTIES += persist.sys.disable_rescue=1

  #ifeq ($(user_variant),user)
  #  ADDITIONAL_DEFAULT_PROPERTIES += ro.adb.secure=1
  #endif
```

Note, when flashing the image, you should use the correct `adb` and `fastboot` version corresponding to the Android version. So please install Android SDK according to the version of the target phone. For instance, we are testing Android 9.0.0_r46, so we install the Android SDK for Android 9.0. After installing the SDK, please create the following symbolic links

```bash
sudo ln -s /path/to/sdk/platform-tools/adb /usr/bin/fastboot
sudo ln -s /path/to/sdk/platform-tools/adb /usr/bin/adb
```

Here are some helpful instructions for flashing a device with ASan enabled.

```bash
############################# Flash factory image       #############################
# Before flashing the manually build image, 
# you should flash the mobile phone with the corresponding factory image.
# please refer to the offical website for flashing factory image.

############################# Flash AOSP image without ASan #############################

# we need to compile aosp in a bash environment
bash

cd /path/to/aosp
# prepare environment
source build/envsetup.sh
# select the target version.
# 50 corresponding to the aosp_taimen-userdebug
# you can use lunch to see the allowed choices.
lunch 50

# compile AOSP and save the compile commands
# replace the N_PROCS with the number you want, 
# e.g., make -j15 showcommands 2>&1 >cmd.txt
make -j [N_PROCS] showcommands 2>&1 >cmd.txt

## here, you should run your commands to flash the image.

############################# Flash AOSP image with ASan #############################

cd ..
# copy the entire project to another place.
cp /path/to/aosp /path/to/aosp_asan
cd /path/to/aosp_asan
source build/envsetup.sh
lunch 50

# compile the entire AOSP with ASan enabled
# replace the N_PROCS with the number you want, 
# e.g., SANITIZE_TARGET=address make -j15
SANITIZE_TARGET=address make -j [N_PROCS]

## here, you should run your commands to flash the image with ASan enabled.
```

## Config FANS
Then we need to create a config file `fans.cfg` for FANS. You could utilize the template `fans.template.cfg` to set up your config. In detail, we need to config the following options of FANS.
- `fans_dir`, FANS directory.
- `aosp_dir`, AOSP directory.
- `aosp_sanitizer_dir`, AOSP with ASan enabled directory.
- `aosp_compilation_cmd_file`, the location of the AOSP compilation cmd file.
- `lunch_command`, the lunch command, e.g., `lunch 50` for aosp_taimen-userdebug.
- `aosp_clang_location`, the location of clang used to compile AOSP, relative to `aosp_dir`, e.g., `prebuilts/clang/host/linux-x86/clang-4691093/bin/clang++.real` for Android 9.0.0_r46.
- `manually_build_clang_location`, the location of clang manually built. For details, please refer to [pre-process](interface-model-extractor/pre-process/readme.md) of the interface model extractor.
- `clang_plugin_option`, the additional options appended to the compilation cmd to load the clang plugin.
- `service_related_file_collector_workdir`, the work dir of the service-related file collector. Keep as default.
- `service_related_filepath_storage_location`, store files related to service. Keep as default.
- `misc_parcel_related_function_storage_location`, store misc functions that have a parcel parameter, e.g., setSchedPolicy(data). Keep as default.
- `special_parcelable_function_storage_location`, store special functions of special parcelable structures. Keep as default.
- `aosp_compilation_cc1_cmd_file`, store cc1 cmd. Keep as default.
- `already_preprocessed_files_storage_location`, store already preprocessed files. Keep as default.
- `rough_interface_related_data_dir`, store the data extracted during the pre-processing. This directory locates in the root dir of aosp. Its name is `data`.
- `already_parsed_interfaces_storage_location`, store already parsed interfaces during the post process. Keep as default.
- `interface_model_extractor_tmp_dir`, the tmp dir used by interface model extractor. Keep as default.
- `interface_model_extractor_dir`, interface model extractor work dir. Keep as default.
- `interface_dependency_dir`, interface dependency dir. Keep as default.

## Collect Interface and Related Files
Please see [Service Related File Collector](service-related-file-collector/readme.md).

## Extract Interface Model
Please see [Interface Model Extractor](interface-model-extractor/readme.md).

## Infer Dependency
Please see [Dependency Inferer](dependency-inferer/readme.md).

## Start Fuzzing

Please see [Fuzzer Engine](fuzzer-engine/readme.md).

## Results
`workdir` contains the following results, including
- service-related files information, located in `workdir/service-related-file`.
- interface model, located in `workdir/interface-model-extractor/model`.
- simplified interface dependency, located in `workdir/interface-dependency`.

For details, you can refer to the `workdir`.

As for the fuzzing results, you can refer to [Fuzzer Manager](fuzzer-engine/manager/readme.md).

If you find bugs by running FANS, please let us know by sending a PR.

## TODO

See [TODO](TODO.md).

## Disclaimer

I am not sure what will happen to your device when using FANS. So good luck!

## Contact

Baozheng Liu (uromise@gmail.com)