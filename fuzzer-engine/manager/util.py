from pwn import log
import os
from time import sleep
import subprocess


def exeute_cmd(cmdline):
    log.debug("cmd: %s" % cmdline)
    cmdline = cmdline
    try:
        # p = os.popen(cmdline)
        # reply = p.read()
        reply = subprocess.check_output(
            cmdline, stderr=subprocess.STDOUT, shell=True, executable="/bin/bash").decode('utf-8')

        # p = subprocess.Popen(cmdline, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, shell=True)
        # reply = p.stdout.read().decode()
    except Exception as e:
        reply = e.output.decode('utf-8', errors='ignore')
    log.debug("reply: %s" % reply)
    sleep(1)
    return reply
