#include <fuzzer/interface/IGraphicBufferConsumer.h>

sp<IBinder> generateIGraphicBufferConsumer() {

  sp<BufferQueueCore> core(new BufferQueueCore());
  LOG_ALWAYS_FATAL_IF(core == NULL,
                      "BufferQueue: failed to create BufferQueueCore");

  sp<IGraphicBufferConsumer> consumer(new BufferQueueConsumer(core));
  LOG_ALWAYS_FATAL_IF(consumer == NULL,
                      "BufferQueue: failed to create BufferQueueConsumer");
  return IInterface::asBinder(consumer);
}