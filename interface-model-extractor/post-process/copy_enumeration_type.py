import shutil
import os
import json

cfg = json.load(open("../../fans.cfg"))

fans_dir = cfg["fans_dir"]

rough_enumeration_data_dir = os.path.join(
    cfg["rough_interface_related_data_dir"], "enumeration")
interface_model_extractor_dir = os.path.join(
    fans_dir, cfg["interface_model_extractor_dir"])
precise_enumeration_dir = os.path.join(interface_model_extractor_dir,"model","enumeration")
used_types_file = os.path.join(interface_model_extractor_dir,"used_types.txt")
used_types = open(used_types_file).read().split("\n")

special_enumeration_types = ["audio_source_t","enum android::video_source","enum android::output_format","enum android::audio_encoder","enum android::video_encoder","enum android_pixel_format_t"]
# http://androidxref.com/9.0.0_r3/xref/frameworks/av/media/libaudioclient/aidl/android/media/IAudioRecord.aidl#27
special_enumeration_types.append("enum android::AudioSystem::sync_event_t")
# http://androidxref.com/9.0.0_r3/xref/frameworks/native/include/gui/ISurfaceComposer.h#63
# http://androidxref.com/9.0.0_r3/xref/frameworks/native/libs/gui/ISurfaceComposer.cpp#597
special_enumeration_types.append("enum android::ISurfaceComposer::(anonymous at frameworks.native.libs.gui.include.gui.ISurfaceComposer.h:61:5)")
special_enumeration_types.append("enum (anonymous at hardware.libhardware.include.hardware.hwcomposer_defs.h:277:1)")
special_enumeration_types.append("enum effect_command_e")
special_enumeration_types.append("enum android::(anonymous at frameworks.av.media.libmedia.include.media.mediametadataretriever.h:35:1)")
# audio_devices_t
special_enumeration_types.append("enum (anonymous at system.media.audio.include.system.audio-base.h:289:1)")
special_enumeration_types.append("enum android::hardware::camera2::params::OutputConfiguration::SurfaceType")
special_enumeration_types.append("enum android::MetaDataBase::Type")
def copy_enumeration_type():
    for filename in os.listdir(rough_enumeration_data_dir):
        if not os.path.isdir(filename):
            if filename in used_types or filename in special_enumeration_types:
                absolute_path = os.path.join(rough_enumeration_data_dir, str(filename))
                shutil.copy(absolute_path, precise_enumeration_dir)

if __name__=="__main__":
    copy_enumeration_type()