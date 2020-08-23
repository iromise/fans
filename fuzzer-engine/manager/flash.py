from pwn import process, log, context
from time import sleep
import json
from util import exeute_cmd

context.log_level = "info"


class Flasher:
    def __init__(self, serial, cfg):
        self.serial = serial
        self.cfg = cfg

    def flash_factory(self):
        cmd = [
            "cd %s" % self.cfg["original_image_path"],
            "source build/envsetup.sh",
            self.cfg["lunch_command"],
            "cd %s" % self.cfg["factory_image_path"],
            "/usr/bin/adb -s " + self.serial + " " + "reboot bootloader",
            "sleep 5",
            "/usr/bin/fastboot -s %s flash bootloader bootloader-taimen-tmz20r.img"
            % self.serial,
            "/usr/bin/fastboot -s %s reboot-bootloader" % self.serial,
            "sleep 5",
            "/usr/bin/fastboot -s %s flash radio radio-taimen-g8998-00008-1902121845.img"
            % self.serial,
            "/usr/bin/fastboot -s %s reboot-bootloader" % self.serial,
            "sleep 5",
            "/usr/bin/fastboot -s %s -w update image-taimen-pq3a.190801.002.zip"
            % self.serial,
        ]
        cmd = "\n".join(cmd)
        result = exeute_cmd(cmd)
        log.info(result)
        log.success("finish flashing factory image.")
        sleep(30)

    def flash_manually_build(self):
        cmd = [
            "cd %s" % self.cfg["original_image_path"],
            "source build/envsetup.sh", self.cfg["lunch_command"],
            "/usr/bin/adb -s %s reboot bootloader" % self.serial,
            "sleep 5",
            "/usr/bin/fastboot -s %s flashing unlock" % self.serial,
            "/usr/bin/fastboot -s %s flashall -w" % self.serial
        ]
        cmd = "\n".join(cmd)
        result = exeute_cmd(cmd)
        log.info(result)
        log.success("finish flashing manually build image.")
        sleep(30)

    def check_asan(self):
        sh = process("/bin/bash")
        cmd = "/usr/bin/adb -s %s shell 'su 0 ls /data| grep asan|wc -l'" % self.serial
        sh.sendline(cmd)
        sleep(1)
        number = sh.readuntil('\n', drop=True)

        log.info("Asan status of device %s : %s." % (self.serial, str(number)))
        if number == b"1":
            return True
        else:
            return False

        sh.close()

    def flash_santizier_image(self):
        log.info("start flashing sanitizier image.")
        cmd = [
            "cd %s\n" % self.cfg["sanitizer_image_path"],
            "source build/envsetup.sh", self.cfg["lunch_command"],
            "/usr/bin/adb -s %s reboot bootloader" % self.serial,
            "sleep 5",
            "/usr/bin/fastboot -s %s flashing unlock" % self.serial,
            "/usr/bin/fastboot -s %s flash userdata" % self.serial,
            "/usr/bin/fastboot -s %s flashall" % self.serial
        ]
        cmd = "\n".join(cmd)
        result = exeute_cmd(cmd)
        log.info(result)
        # wait until boot completed
        log.success("finish flashing sanitizier image.")
        sleep(30)

    def wipe_userdata_and_cache(self):
        log.info("start wiping userdata and cache.")
        cmd = [
            "/usr/bin/adb -s %s reboot bootloader" % self.serial,
            "sleep 5",
            "/usr/bin/fastboot -s %s boot %s" % (self.serial, self.cfg["twrp_img_path"]),
            "sleep 25",
            "/usr/bin/adb -s %s shell recovery --wipe_data" % self.serial,
        ]
        cmd = "\n".join(cmd)
        result = exeute_cmd(cmd)
        log.info(result)
        if "I:AB_OTA_UPDATER := true" in result:
            log.success("wiping userdata and cache succeed.")
        else:
            log.warning("wiping userdata and cache failed.")
        # wait until boot completed
        sleep(30)


def flash_all(flash_cfg,device_cfg):
    for serial in device_cfg:
        flasher = Flasher(serial, flash_cfg)
        # flasher.flash_factory()
        flasher.flash_manually_build()
        flasher.flash_santizier_image()

def check_all_asan_status(flash_cfg,device_cfg):
    for serial in device_cfg:
        flasher = Flasher(serial, flash_cfg)
        flasher.check_asan()

if __name__ == "__main__":
    flash_cfg = json.load(open("flash.cfg"))
    device_cfg = json.load(open("device.cfg"))
    # check_all_asan_status(flash_cfg,device_cfg)
    # XXXXXXXXXXXXXX is the serial of device to be flashed.
    # flasher = Flasher("XXXXXXXXXXXXXX", flash_cfg)
    # flasher.flash_factory()
    # flasher.flash_manually_build()
    # flasher.flash_santizier_image()
    # flasher.wipe_userdata_and_cache()
    flash_all(flash_cfg,device_cfg)
    
