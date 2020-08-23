#include <assert.h>
#include <binder/Parcel.h>
#include <fuzzer/dependency_solver.h>
#include <fuzzer/types/blob_type.h>
#include <fuzzer/types/file_descriptor_type.h>

void BlobType::generate() {
  int32_t blobType;

  if (blobSize <= 16 * 1024) {
    blobType = 0;
  } else {
    blobType = randomUInt64(1, 2);
  }
  parcelReaderWriter->data->writeInt32(blobType);
  if (blobType == 0) {
    // http://androidxref.com/9.0.0_r3/xref/frameworks/native/libs/binder/Parcel.cpp#2269
    FUZZER_LOGD("Generating blob inplace.");

    FUZZER_LOGD("Blob size is %lu.", blobSize);
    vector<int8_t> tmp;

    for (uint32_t i = 0; i < blobSize && i < 16 * 1024; ++i) {
      tmp.push_back((int8_t)randomUInt64(0, 255));
    }
    parcelReaderWriter->data->write(tmp.data(), blobSize);
  } else {
    FUZZER_LOGD("Generating FileDescriptor in blob.");
    // TODO: fd means that size is larger than BLOB_INPLACE_LIMIT=16*1024
    FDType fdType("in", "int");
    int32_t fd = fdType.generate();
    parcelReaderWriter->data->writeFileDescriptor(fd);
  }
}

void BlobType::read() {
  Parcel::ReadableBlob blob;
  parcelReaderWriter->reply->readBlob(blobSize, &blob);
  if (parcelReaderWriter->isDependency) {
    if (blobSize > 16 * 1024) {
      FUZZER_LOGE("Blob size is larger than 16*1024, it is not implemented.");
      exit(0);
    } else {
      parcelReaderWriter->targetParcel->write(blob.data(), blobSize);
    }
  }
  blob.release();
}
