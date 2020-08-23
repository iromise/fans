#include <fuzzer/interface/IGraphicBufferProducer.h>
#include <fuzzer/types/types.h>
sp<IBinder> generateIGraphicBufferProducer() {
  sp<IServiceManager> sm = defaultServiceManager();
  sp<IBinder> MeidaPlayerService = sm->checkService(String16("media.player"));
  if (MeidaPlayerService == NULL) {
    return NULL;
  }

  // get IMediaPlayerService
  sp<IMediaPlayerService> iMPService =
      IMediaPlayerService::asInterface(MeidaPlayerService);
  //   ALOGI("Get iMPService instance, 0x%08lx\n", (unsigned
  //   long)iMPService.get());
  sp<IMediaRecorder> recorder = iMPService->createMediaRecorder(
      String16(StringType::generatePackageName().c_str()));
  if (recorder == NULL) {
    return NULL;
  }
  //   ALOGI("Get recorder instance, 0x%08lx\n", (unsigned long)recorder.get());

  const char *fileName = "/data/fuzzer/test.txt";
  int fd = open(fileName, O_RDWR | O_CREAT, 0744);
  recorder->setVideoSource(2);
  recorder->setOutputFile(fd);
  IntType<int32_t> of("of", "");
  recorder->setOutputFormat(of.generate());
  recorder->init();
  recorder->prepare();
  recorder->start();

  // get IGraphicBufferProducer
  sp<IGraphicBufferProducer> producer = recorder->querySurfaceMediaSource();
  return IInterface::asBinder(producer);
}