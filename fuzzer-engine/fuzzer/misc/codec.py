codecinfo="""OMX.google.aac.decoder
audio/mp4a-latm
-------
OMX.google.amrnb.decoder
audio/3gpp
-------
OMX.google.amrwb.decoder
audio/amr-wb
-------
OMX.google.flac.decoder
audio/flac
-------
OMX.google.g711.alaw.decoder
audio/g711-alaw
-------
OMX.google.g711.mlaw.decoder
audio/g711-mlaw
-------
OMX.google.gsm.decoder
audio/gsm
-------
OMX.google.mp3.decoder
audio/mpeg
-------
OMX.google.opus.decoder
audio/opus
-------
OMX.google.raw.decoder
audio/raw
-------
OMX.google.vorbis.decoder
audio/vorbis
-------
OMX.google.aac.encoder
audio/mp4a-latm
-------
OMX.google.amrnb.encoder
audio/3gpp
-------
OMX.google.amrwb.encoder
audio/amr-wb
-------
OMX.google.flac.encoder
audio/flac
-------
OMX.qcom.video.decoder.avc
video/avc
-------
OMX.qcom.video.decoder.avc.secure
video/avc
-------
OMX.google.h264.decoder
video/avc
-------
OMX.qcom.video.decoder.h263
video/3gpp
-------
OMX.google.h263.decoder
video/3gpp
-------
OMX.qcom.video.decoder.hevc
video/hevc
-------
OMX.qcom.video.decoder.hevc.secure
video/hevc
-------
OMX.google.hevc.decoder
video/hevc
-------
OMX.qcom.video.decoder.mpeg4
video/mp4v-es
-------
OMX.google.mpeg4.decoder
video/mp4v-es
-------
OMX.qcom.video.decoder.vp8
video/x-vnd.on2.vp8
-------
OMX.google.vp8.decoder
video/x-vnd.on2.vp8
-------
OMX.qcom.video.decoder.vp9
video/x-vnd.on2.vp9
-------
OMX.qcom.video.decoder.vp9.secure
video/x-vnd.on2.vp9
-------
OMX.google.vp9.decoder
video/x-vnd.on2.vp9
-------
OMX.qcom.video.encoder.avc
video/avc
-------
OMX.google.h264.encoder
video/avc
-------
OMX.qcom.video.encoder.h263
video/3gpp
-------
OMX.google.h263.encoder
video/3gpp
-------
OMX.qcom.video.encoder.hevc
video/hevc
-------
OMX.qcom.video.encoder.mpeg4
video/mp4v-es
-------
OMX.google.mpeg4.encoder
video/mp4v-es
-------
OMX.qcom.video.encoder.vp8
video/x-vnd.on2.vp8
-------
OMX.google.vp8.encoder
video/x-vnd.on2.vp8
-------
OMX.google.vp9.encoder
video/x-vnd.on2.vp9
-------
"""
codecinfo =codecinfo.split("-------\n")[:-1]
codecname=[]
mime = []
for item in codecinfo:
    item = item.split("\n")

    codecname.append(item[0])
    mime.append(item[1])
for i in codecname:
    print(i+",")
print("")
for i in mime:
    print(i+",")