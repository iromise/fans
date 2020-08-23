from xml.etree.ElementTree import parse

doc = parse("audio_effects.xml")
root = doc.getroot()
print(root)
print(root.find("libraries"))
uuid = []
for item in root.findall("effects/effectProxy"):
    uuid.append(item.attrib["uuid"])
    libsw = item.find("libsw")
    uuid.append(libsw.attrib["uuid"])
    libhw = item.find("libhw")
    uuid.append(libhw.attrib["uuid"])
for item in root.findall("effects/effect"):
    uuid.append(item.attrib["uuid"])
for item in uuid:
    print('"%s",' % item)
print(len(uuid))