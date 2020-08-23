#include <fuzzer/interface/IStreamSource.h>
// struct MyStreamSource : public BnStreamSource {
//   // Object assumes ownership of fd.
//   explicit MyStreamSource(int fd);

//   virtual void setListener(const sp<IStreamListener> &listener);
//   virtual void setBuffers(const Vector<sp<IMemory>> &buffers);

//   virtual void onBufferAvailable(size_t index);

// protected:
//   virtual ~MyStreamSource();

// private:
//   int mFd;
//   off64_t mFileSize;
//   uint64_t mNumPacketsSent;

//   sp<IStreamListener> mListener;
//   Vector<sp<IMemory>> mBuffers;

//   DISALLOW_EVIL_CONSTRUCTORS(MyStreamSource);
// };

// MyStreamSource::MyStreamSource(int fd)
//     : mFd(fd), mFileSize(0), mNumPacketsSent(0) {
//   CHECK_GE(fd, 0);

//   mFileSize = lseek64(fd, 0, SEEK_END);
//   lseek64(fd, 0, SEEK_SET);
// }

// MyStreamSource::~MyStreamSource() {
//   close(mFd);
//   mFd = -1;
// }

// void MyStreamSource::setListener(const sp<IStreamListener> &listener) {
//   mListener = listener;
// }

// void MyStreamSource::setBuffers(const Vector<sp<IMemory>> &buffers) {
//   mBuffers = buffers;
// }

// void MyStreamSource::onBufferAvailable(size_t index) {
//   CHECK_LT(index, mBuffers.size());

// #if 0
//     if (mNumPacketsSent >= 20000) {
//         ALOGI("signalling discontinuity now");

//         off64_t offset = 0;
//         CHECK((offset % 188) == 0);

//         lseek(mFd, offset, SEEK_SET);

//         sp<AMessage> extra = new AMessage;
//         extra->setInt32(IStreamListener::kKeyFormatChange, 0);

//         mListener->issueCommand(
//                 IStreamListener::DISCONTINUITY, false /* synchronous */,
//                 extra);

//         mNumPacketsSent = 0;
//     }
// #endif

//   sp<IMemory> mem = mBuffers.itemAt(index);

//   ssize_t n = read(mFd, mem->pointer(), mem->size());
//   if (n <= 0) {
//     mListener->issueCommand(IStreamListener::EOS, false /* synchronous */);
//   } else {
//     mListener->queueBuffer(index, n);

//     mNumPacketsSent += n / 188;
//   }
// }
// ////////////////////////////////////////////////////////////////////////////////

// struct MyConvertingStreamSource : public BnStreamSource {
//   explicit MyConvertingStreamSource(const char *filename);

//   virtual void setListener(const sp<IStreamListener> &listener);
//   virtual void setBuffers(const Vector<sp<IMemory>> &buffers);

//   virtual void onBufferAvailable(size_t index);

// protected:
//   virtual ~MyConvertingStreamSource();

// private:
//   Mutex mLock;
//   Condition mCondition;

//   sp<IStreamListener> mListener;
//   Vector<sp<IMemory>> mBuffers;

//   sp<MPEG2TSWriter> mWriter;

//   ssize_t mCurrentBufferIndex;
//   size_t mCurrentBufferOffset;

//   List<size_t> mBufferQueue;

//   static ssize_t WriteDataWrapper(void *me, const void *data, size_t size);
//   ssize_t writeData(const void *data, size_t size);

//   DISALLOW_EVIL_CONSTRUCTORS(MyConvertingStreamSource);
// };

// ////////////////////////////////////////////////////////////////////////////////

// MyConvertingStreamSource::MyConvertingStreamSource(const char *filename)
//     : mCurrentBufferIndex(-1), mCurrentBufferOffset(0) {
//   sp<DataSource> dataSource =
//       DataSourceFactory::CreateFromURI(NULL /* httpService */, filename);

//   CHECK(dataSource != NULL);

//   sp<IMediaExtractor> extractor = MediaExtractorFactory::Create(dataSource);
//   CHECK(extractor != NULL);

//   mWriter =
//       new MPEG2TSWriter(this, &MyConvertingStreamSource::WriteDataWrapper);

//   size_t numTracks = extractor->countTracks();
//   for (size_t i = 0; i < numTracks; ++i) {
//     const sp<MetaData> &meta = extractor->getTrackMetaData(i);

//     const char *mime;
//     CHECK(meta->findCString(kKeyMIMEType, &mime));

//     if (strncasecmp("video/", mime, 6) && strncasecmp("audio/", mime, 6)) {
//       continue;
//     }

//     sp<MediaSource> track =
//         CreateMediaSourceFromIMediaSource(extractor->getTrack(i));
//     if (track == nullptr) {
//       fprintf(stderr, "skip NULL track %zu, total tracks %zu\n", i,
//       numTracks); continue;
//     }
//     CHECK_EQ(mWriter->addSource(track), (status_t)OK);
//   }

//   CHECK_EQ(mWriter->start(), (status_t)OK);
// }

// MyConvertingStreamSource::~MyConvertingStreamSource() {}

// void MyConvertingStreamSource::setListener(
//     const sp<IStreamListener> &listener) {
//   mListener = listener;
// }

// void MyConvertingStreamSource::setBuffers(const Vector<sp<IMemory>> &buffers)
// {
//   mBuffers = buffers;
// }

// ssize_t MyConvertingStreamSource::WriteDataWrapper(void *me, const void
// *data,
//                                                    size_t size) {
//   return static_cast<MyConvertingStreamSource *>(me)->writeData(data, size);
// }

// ssize_t MyConvertingStreamSource::writeData(const void *data, size_t size) {
//   size_t totalWritten = 0;

//   while (size > 0) {
//     Mutex::Autolock autoLock(mLock);

//     if (mCurrentBufferIndex < 0) {
//       while (mBufferQueue.empty()) {
//         mCondition.wait(mLock);
//       }

//       mCurrentBufferIndex = *mBufferQueue.begin();
//       mCurrentBufferOffset = 0;

//       mBufferQueue.erase(mBufferQueue.begin());
//     }

//     sp<IMemory> mem = mBuffers.itemAt(mCurrentBufferIndex);

//     size_t copy = size;
//     if (copy + mCurrentBufferOffset > mem->size()) {
//       copy = mem->size() - mCurrentBufferOffset;
//     }

//     memcpy((uint8_t *)mem->pointer() + mCurrentBufferOffset, data, copy);
//     mCurrentBufferOffset += copy;

//     if (mCurrentBufferOffset == mem->size()) {
//       mListener->queueBuffer(mCurrentBufferIndex, mCurrentBufferOffset);
//       mCurrentBufferIndex = -1;
//     }

//     data = (const uint8_t *)data + copy;
//     size -= copy;

//     totalWritten += copy;
//   }

//   return (ssize_t)totalWritten;
// }

// void MyConvertingStreamSource::onBufferAvailable(size_t index) {
//   Mutex::Autolock autoLock(mLock);

//   mBufferQueue.push_back(index);
//   mCondition.signal();

//   if (mWriter->reachedEOS()) {
//     if (mCurrentBufferIndex >= 0) {
//       mListener->queueBuffer(mCurrentBufferIndex, mCurrentBufferOffset);
//       mCurrentBufferIndex = -1;
//     }

//     mListener->issueCommand(IStreamListener::EOS, false /* synchronous */);
//   }
// }

sp<IBinder> generateIStreamSource() {
  // sp<IStreamSource> source;
  // // source = new MyStreamSource(FDType::generateRandomFD());
  // sp<CallbackProtector> callbackProtector = NULL;
  // IAndroidBufferQueue *androidBufferQueue = NULL;
  // StreamPlayer *player = NULL;
  // source = new android::StreamSourceAppProxy(androidBufferQueue,
  //                                            callbackProtector, player);
  // return IInterface::asBinder(source);
  return NULL;
}
