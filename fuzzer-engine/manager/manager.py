from pwn import process, log, context
from multiprocessing import Process
from time import sleep
import os
import json
import datetime
from flash import Flasher
from util import exeute_cmd
import traceback

context.log_level = "warning"


class Device:
    def __init__(self, serial, cfg: dict):
        self.serial = serial
        self.cfg = cfg

        self.host_fuzzer_path = cfg["host_fuzzer_path"]
        self.fuzzer_name = os.path.basename(self.host_fuzzer_path)

        self.host_model_dir = cfg["host_model_dir"]
        self.host_seed_dir = cfg["host_seed_dir"]
        self.device_work_dir = cfg["device_work_dir"]

        self.device_fuzzer_path = os.path.join(self.device_work_dir,
                                               self.fuzzer_name)

        self.max_instance_number = cfg["max_instance_number"]

        # count how many fuzzers ever run, as some may be died.
        self.accumulated_fuzzer_num = 0

        # how many logcat, as logcat may be died?
        self.accumulated_logcat_num = 0

        # how many dmesg, as dmesg may be died.
        self.accumulated_dmesg_num = 0
        self.dmesg_pid = -1

        # device dir
        self.host_log_dir = os.path.join(self.cfg["current_log_dir"],
                                       self.serial)
        if os.path.exists(self.host_log_dir):
            if os.path.isdir(self.host_log_dir):
                pass
            else:
                log.warning("The log %s dir must not be an existing file!" %
                            self.host_log_dir)
                exit(0)
        else:
            os.makedirs(self.host_log_dir)

        self.flash_count = 0
        self.flash_cfg = json.load(open("flash.cfg"))
        self.flasher = Flasher(self.serial, self.flash_cfg)

        # restart device script on device
        self.device_restart_device_script_path = os.path.join(self.device_work_dir, os.path.basename(
            self.cfg["restart_device_script_path"]))
        self.max_tombstone_count = self.cfg["max_tombstone_count"]

        self.device_manager_log_path = os.path.join(self.host_log_dir, "device_manager.log")

    def host_shell(self, cmdline):
        return exeute_cmd(cmdline)

    def adb(self, cmd):
        adb_prefix = '/usr/bin/adb -s ' + self.serial
        return exeute_cmd(adb_prefix + " " + cmd)

    def device_shell(self, cmdline):
        return self.adb("shell " + cmdline)

    def reboot(self):
        return self.adb("reboot")

    def root(self):
        data = self.adb("root")
        if "as root" not in data:
            log.warning("root %s Failed." % self.serial)
        else:
            log.success("root %s succeed." % self.serial)

    def logcat(self, *args):
        cmd = ["logcat"]
        cmd.extend(args)
        return self.adb(' '.join(cmd))

    def push(self, local, remote):
        cmd = " ".join(["push", local, remote])
        data = self.adb(cmd)
        if "pushed." in data:
            log.success("%s push %s succeed." % (self.serial, remote))
        else:
            log.warning("%s push %s Failed." % (self.serial, local))
            log.info("  retry push..")
            while self.get_device_state() != "device":
                sleep(10)
            self.reset()
            self.push(local, remote)

    def pull(self, remote, local):
        cmd = " ".join(["pull", remote, local])
        data = self.adb(cmd)
        if "pulled." not in data:
            log.warning("%s pull %s Failed." % (self.serial, remote))
        else:
            log.success("%s pull %s succeed." % (self.serial, remote))

    def sync_device_to_host(self, device_dir, host_dir):
        cmd = "adb-sync -s %s --reverse %s %s" % (self.serial, device_dir,
                                                  host_dir)
        result = exeute_cmd(cmd)
        if "Total: " not in result:
            log.warning("%s sync %s Failed." % (self.serial, device_dir))
        else:
            log.success("%s sync %s succeed." % (self.serial, device_dir))

    def mkdir_on_device(self, dir):
        cmdline = "mkdir -p %s" % dir
        self.device_shell(cmdline)

    def mkdir_on_host(self, dir):
        cmdline = "mkdir -p %s" % dir
        self.host_shell(cmdline)

    def rmdir_on_device(self, dir):
        cmdline = "rm -rf %s" % dir
        self.device_shell(cmdline)

    def rmdir_on_host(self, dir):
        cmdline = "rm -rf %s" % dir
        self.host_shell(cmdline)

    def kill_device_process(self, process_pid):
        self.device_shell("kill -9 " + process_pid)

    def kill_all_device_process(self, process_name):
        data = self.device_shell(
            "ps -ef|grep %s|awk '{print $2}'" % process_name)
        pids = data.strip().split("\n")
        for pid in pids:
            if pid.isdigit():
                self.kill_device_process(pid)

    def get_device_process_list(self, process_name):
        process_list = self.device_shell(
            "ps -ef|grep %s|grep -v grep| awk '{print $2}'" % process_name)
        process_list = process_list.strip()
        if process_list == "":
            return []
        else:
            return process_list.split("\n")
        return process_list

    def kill_host_process(self, pid):
        cmd = "kill -9 %s" % pid
        self.host_shell(cmd)

    def check_host_pid(self, pid):
        """ Check For the existence of a unix pid. """
        try:
            os.kill(pid, 0)
        except OSError:
            return False
        else:
            return True

    def get_host_process_list(self, process_name):
        cmd = "ps -ef|grep %s|grep -v grep| awk '{print $2}'" % process_name
        process_list = self.host_shell(cmd)
        process_list = process_list.strip()
        if process_list == "":
            return []
        else:
            return process_list.split("\n")

    def get_process_list(self, location, process_name):
        if location == "host":
            pids = self.get_host_process_list(process_name)
        elif location == "device":
            pids = self.get_device_process_list(process_name)
        else:
            log.warning("The location should be either host or device.")
            return None
        log.debug("pids: %s" % str(pids))
        return pids

    def kill_process(self, location, process_name):
        if location == "host":
            self.kill_host_process(process_name)
        elif location == "device":
            self.kill_device_process(process_name)
        else:
            log.warning("The location should be either host or device.")
            return None

    def check_process_status(self, process_name, required_process_num,
                             get_process_list, kill_process, run_process):
        pids = get_process_list(process_name)
        if pids is None:
            return
        current_process_num = len(pids)
        if current_process_num == required_process_num:
            pass
        elif current_process_num < required_process_num:
            # we need to restart some
            log.info(
                "The number of process %s is less than required. Started the delta.."
                % process_name)
            delta = required_process_num - current_process_num
            for i in range(delta):
                run_process()
        elif current_process_num > required_process_num:
            log.warning(
                "The number of process %s is more than required. kill all and restart.."
                % process_name)
            for pid in pids:
                kill_process(pid)
            for i in range(required_process_num):
                run_process()

    def reset_max_tombstone_count(self):
        # this is not a good solution
        # we still need to 
        current_count = int(self.device_shell(
            "getprop tombstoned.max_tombstone_count").strip(), 10)
        if current_count != self.max_tombstone_count:
            # reset tombstoned number
            self.kill_all_device_process("tombstoned")
            # set tombstoned property
            self.device_shell(
                "setprop tombstoned.max_tombstone_count %s" % str(self.max_tombstone_count))

    def in_fastboot(self):
        cmd = "/usr/bin/fastboot devices"
        data = self.host_shell(cmd)
        if self.serial in data:
            return True

    def fastboot_reboot(self):
        cmd = "/usr/bin/fastboot -s %s reboot" % self.serial
        result = self.host_shell(cmd)
        if "Finished. Total time:" in result:
            log.success("fastboot reboot succeed.")
            sleep(10)
        else:
            log.error("fastboot reboot failed.")

    def get_device_state(self):
        try:
            state = self.adb("get-state").strip()
            if state == "device":
                pass
            elif state == "recovery":
                pass
            elif "offline" in state:
                state = "offline"
            elif "not found" in state:
                if self.in_fastboot():
                    state = "fastboot"
                else:
                    state = "not found"
            else:
                state = "unknown"
            log.info("%s in %s state." % (self.serial, state))
        except Exception as e:
            log.warning(
                "Get Device %s State Error %s." % (self.serial, str(e)))
            state = "error"
        return state

    def reconnect(self):
        log.debug("reconnect %s" % self.serial)
        self.adb("reconnect")
        sleep(5)
        self.host_shell("/usr/bin/adb reconnect offline")

    def ensure_in_device_state(self):
        log.info("ensure device in device state")
        state = self.get_device_state()
        recovery_count = 0
        while state != "device":
            if state == "offline":
                while self.get_device_state() == "offline":
                    self.reconnect()
                    sleep(10)
            elif state == "fastboot":
                self.fastboot_reboot()
            elif state == "recovery":
                recovery_count += 1
                if recovery_count > 5:
                    log.info("  meet device recovery state before starting fuzzing")
                    log.info("  sync old logs.")
                    if self.flash_count==0:
                        self.init_log_dir()
                    self.sync_log()
                    self.flash_count+=1
                    self.accumulated_dmesg_num=0
                    self.accumulated_fuzzer_num=0
                    self.accumulated_logcat_num=0
                    # what should we do in recovery mode?
                    # here we might need to wipe data?
                    # this will make the device lose sanitizer state
                    self.flasher.wipe_userdata_and_cache()

                else:
                    # try reboot mobile
                    self.reboot()
            else:
                sleep(20)
                # maybe we should reconnect...
                self.reconnect()
            state = self.get_device_state()
        log.info("device is in device state")

    def check_asan_status(self):
        cmd = "/usr/bin/adb -s %s shell 'su 0 ls /data| grep asan|wc -l'" % self.serial
        result = self.host_shell(cmd).strip()
        log.info("ASAN status: %s." % str(result))
        if result == "1":
            return True
        elif result =="0":
            return False
        else:
            sleep(10)
            self.ensure_in_device_state()
            return self.check_asan_status()

    def ensure_in_sanitizer_state(self):
        log.info("ensure device in sanitizer state")
        if self.check_asan_status() is False:
            log.info("  device is not in sanitizer state, flashing...")
            while self.check_asan_status() is False:
                self.flasher.flash_santizier_image()
            log.info("  flashing success.")
            self.reset()
            self.prepare()
        log.info("device is in sanitizer state")


    def reset(self):
        self.ensure_in_device_state()
        self.ensure_in_sanitizer_state()

        # get root ability
        self.root()
        # disable selinux
        self.device_shell("setenforce 0")
        # disable Rescue Party, already set when flashing
        self.device_shell("setprop persist.sys.disable_rescue true")

        # self.reset_max_tombstone_count()

    def init_log_dir(self):
        log.info("init log dir on host and device.")
        
        log.info("  create root log dir on host and device.")
        
        # note that host current log dir is related with the flash_count
        self.host_current_log_dir = os.path.join(self.host_log_dir, str(self.flash_count))
        self.mkdir_on_host(self.host_current_log_dir)
        self.device_current_log_dir = os.path.join(self.device_work_dir,"log")
        self.mkdir_on_device(self.device_current_log_dir)

        log.info("  create fuzzer log dir on host and device")
        self.device_fuzzer_log_dir = os.path.join(self.device_current_log_dir, "fuzzer_log")
        self.mkdir_on_device(self.device_fuzzer_log_dir)
        self.host_fuzzer_log_dir = os.path.join(self.host_current_log_dir, "fuzzer_log")
        self.mkdir_on_host(self.host_fuzzer_log_dir)      

        log.info("  create logcat log dir on host and device")
        self.device_logcat_log_dir = os.path.join(self.device_current_log_dir, "logcat")
        self.mkdir_on_device(self.device_logcat_log_dir)
        self.host_logcat_log_dir = os.path.join(self.host_current_log_dir, "logcat")
        self.mkdir_on_host(self.host_logcat_log_dir)     

        log.info("  create tombstone log dir on host (device tombstone log dir already exists.")
        self.host_tombstone_log_dir = os.path.join(self.host_current_log_dir, "tombstones")
        self.mkdir_on_host(self.host_tombstone_log_dir)     

        log.info("  create anr log dir on host (device anr log dir already exists.")
        self.host_anr_log_dir = os.path.join(self.host_current_log_dir, "anr")
        self.mkdir_on_host(self.host_anr_log_dir)            


    def push_fuzzer_data(self):
        log.info("push fuzzer related info")
        self.rmdir_on_device(self.device_work_dir)
        self.mkdir_on_device(self.device_work_dir)
        log.info("  push interface model to device.")
        self.push(self.host_model_dir, self.device_work_dir)
        log.info("  push seed to device")
        self.push(self.host_seed_dir,self.device_work_dir)

        log.info("  push fuzzer to device")
        self.push(self.host_fuzzer_path, self.device_work_dir)
        # log.info("push adbd restart script")
        # self.push(self.cfg["adbd_restart_script_path"], self.device_work_dir)
        log.info("  push android restart script to device")
        self.push(self.cfg["restart_device_script_path"], self.device_work_dir)

    def prepare(self):
        log.info("Prepare something before fuzzer running..")

        log.info("  kill all remained fuzzers")
        self.kill_all_device_process(self.fuzzer_name)

        log.info("  push fuzzer related data")
        self.push_fuzzer_data()

        log.info("  clear old logcat log")
        self.logcat("-b", "all", "-c")

        log.info("  init log dir on host and device")
        self.init_log_dir()


    def run_one_logcat(self):
        self.accumulated_logcat_num += 1
        self.host_logcat_path = os.path.join(self.host_logcat_log_dir,
                                        "logcat%d.log" % self.accumulated_logcat_num)
        self.logcat("-b", "all", ">", self.host_logcat_path, "2>&1", "&")

    def run_restart_device_script(self):
        self.device_shell(
            "chmod +x %s" % self.device_restart_device_script_path)
        restart_device_cmd = "sh -T- %s" % self.device_restart_device_script_path
        self.device_shell(restart_device_cmd)

    def run_one_fuzzer(self):
        self.accumulated_fuzzer_num += 1
        # for different fuzzer's run log
        self.device_fuzzer_log_path = os.path.join(
            self.device_fuzzer_log_dir, "run%d.log" % self.accumulated_fuzzer_num)
        # run fuzzer and record log on device
        cmd = [
            "." + self.device_fuzzer_path,
            "--log_level=" + str(self.cfg["fuzzer_log_level"]), ">",
            self.device_fuzzer_log_path, "2>&1", "&"
        ]
        cmd = ' '.join(cmd)
        cmd = "'sh -c \"%s\"'" % cmd
        self.device_shell(cmd)

    def _run(self):
        log.info("start fuzzing fuzzer")

        log.info("  start logcat")
        self.run_one_logcat()

        log.info("  run fuzzer")
        for i in range(self.max_instance_number):
            self.run_one_fuzzer()

        self.run_restart_device_script()

    def run(self):
        context.log_file = self.device_manager_log_path
        context.log_level = "warning"
        self.reset()
        self.prepare()
        self._run()
        self.monitor()

    def check_fuzzer_status(self):
        fuzzer_process_name = "native_service_fuzzer"
        self.check_process_status(fuzzer_process_name,
                                  self.max_instance_number,
                                  self.get_device_process_list,
                                  self.kill_device_process, self.run_one_fuzzer)

    def check_logcat_status(self):
        logcat_process_name = "'/usr/bin/adb -s %s logcat -b all'" % self.serial
        self.check_process_status(logcat_process_name, 1,
                                  self.get_host_process_list,
                                  self.kill_host_process, self.run_one_logcat)

    def check_restart_device_script_status(self):
        restart_device_process_name = "'sh -T- %s'" % self.device_restart_device_script_path
        self.check_process_status(
            restart_device_process_name, 1, self.get_device_process_list,
            self.kill_device_process, self.run_restart_device_script)

    def sync_log(self):
        log.info("sync log")


        log.info("  sync tombstones(native crash)")
        self.sync_device_to_host("/data/tombstones/", self.host_tombstone_log_dir) 
        
        log.info("  sync anrs(Application Not Responding)")
        self.sync_device_to_host("/data/anr/", self.host_anr_log_dir)
        
        log.info("  sync fuzzer log")
        self.sync_device_to_host(self.device_fuzzer_log_dir+"/",self.host_fuzzer_log_dir)

    def monitor(self):
        while True:
            sleep(10)
            try:
                # make sure the state ..
                # keep the device root, selinux disabled, tombstone.
                # kill tombstoned might miss some crashes.
                self.reset()

                self.check_fuzzer_status()
                self.check_logcat_status()
                self.check_restart_device_script_status()

                self.sync_log()

            except Exception as e:
                print("device %s exception: %s." % (self.serial, str(e)))
                print("please see the corresponding log.")
                log.error("device %s exception: %s." % (self.serial, str(e)))
                log.error(traceback.format_exc())

black_serial_list = []


class NativeServiceFuzzerManager:
    devices = []

    def clean(self):
        # killall existing logcat....
        self.bash.sendline(
            "ps -ef|grep logcat|grep -v less| grep -v tail| awk '{print $2}'|xargs kill -9"
        )

    def __init__(self):
        self.bash = process("/bin/bash")

        # clean remained information
        self.clean()
        
        self.bash.close()

        self.fuzzer_cfg = json.load(open("fuzzer.cfg"))

        self.device_cfg = json.load(open("device.cfg"))

        # get current timestamp
        self.timestamp = datetime.datetime.now().strftime('%Y-%m-%d-%H-%M-%S')

        # create current log dir...
        self.fuzzer_cfg["current_log_dir"] = os.path.join(
            self.fuzzer_cfg["root_log_dir"], self.timestamp)
        os.makedirs(self.fuzzer_cfg["current_log_dir"])

        for serial in self.device_cfg:
            if serial in black_serial_list:
                continue
            self.devices.append(Device(serial, self.fuzzer_cfg))

    def start(self):

        for device in self.devices:
            p = Process(target=device.run)
            p.start()
        while True:
            sleep(10)
            log.warning(
                "===========================================================")
            for device in self.devices:
                state = device.get_device_state()
                if state != "device":
                    log.warning("ID: " +
                                str(self.device_cfg[device.serial])+", "+"Serial: "+device.serial+" in " +
                                str(state) + " status.")
            log.warning(
                "===========================================================")

manager = NativeServiceFuzzerManager()
manager.start()
