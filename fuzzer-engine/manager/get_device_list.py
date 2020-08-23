from pwn import process, log, context
import json
def get_device_list():
    bash = process("/bin/bash")
    bash.sendline("/usr/bin/adb devices")
    bash.recvuntil("List of devices attached\n")
    reply = bash.recv().strip().split(b"\n")
    serials = dict()
    cnt = 0
    for line in reply:
        fields = line.split()
        serial = fields[0]
        cnt += 1
        serials[serial.decode()] = cnt
    print(serials)
    data = json.dumps(serials, sort_keys=True, indent=4)
    open("device.cfg", "w").write(data)


if __name__ == "__main__":
    get_device_list()