# Case 1 - default branch in setParameter

location: frameworks/av/media/libmediaplayerservice/MediaPlayerService.cpp 

before modifying 

```cpp
status_t MediaPlayerService::Client::setParameter(int key, const Parcel &request) {
    ALOGV("[%d] setParameter(%d)", mConnId, key);
    switch (key) {
    case KEY_PARAMETER_AUDIO_ATTRIBUTES:
    {
        Mutex::Autolock l(mLock);
        return setAudioAttributes_l(request);
    }
    default:
        sp<MediaPlayerBase> p = getPlayer();
        if (p == 0) { return UNKNOWN_ERROR; }
        return p->setParameter(key, request);
    }
}
```

after modifying
```cpp
status_t MediaPlayerService::Client::setParameter(int key,
                                                  const Parcel &request) {
  ALOGV("[%d] setParameter(%d)", mConnId, key);
  switch (key) {
  case KEY_PARAMETER_AUDIO_ATTRIBUTES: {
    Mutex::Autolock l(mLock);
    return setAudioAttributes_l(request);
  }
  default: {
    sp<MediaPlayerBase> p = getPlayer();
    if (p == 0) {
      return UNKNOWN_ERROR;
    }
    return p->setParameter(key, request);
  }
  }
}
```

# Case 2 - readFromParcel and writeToParcel of MediaAnalyticsItem
location: frameworks/av/media/libmediametrics/MediaAnalyticsItem.cpp

modify
- wrap case statement with `{` and `}` in readFromParcel and writeToParcel
- swap the order of `MediaAnalyticsItem::kTypeRate` case and `MediaAnalyticsItem::kTypeCString` case in `writeToParcel` function. 

Here we only give the modification of `writeToParcel` function.

before modifying
```cpp
for (int i = 0 ; i < count; i++ ) {
        Prop *prop = &mProps[i];
        data->writeCString(prop->mName);
        data->writeInt32(prop->mType);
        switch (prop->mType) {
            case MediaAnalyticsItem::kTypeInt32:
                    data->writeInt32(prop->u.int32Value);
                    break;
            case MediaAnalyticsItem::kTypeInt64:
                    data->writeInt64(prop->u.int64Value);
                    break;
            case MediaAnalyticsItem::kTypeDouble:
                    data->writeDouble(prop->u.doubleValue);
                    break;
            case MediaAnalyticsItem::kTypeRate:
                    data->writeInt64(prop->u.rate.count);
                    data->writeInt64(prop->u.rate.duration);
                    break;
            case MediaAnalyticsItem::kTypeCString:
                    data->writeCString(prop->u.CStringValue);
                    break;
            default:
                    ALOGE("found bad Prop type: %d, idx %d, name %s",
                            prop->mType, i, prop->mName);
                    break;
        }
}
```

after modifying
```cpp
for (int i = 0; i < count; i++) {
Prop *prop = &mProps[i];
data->writeCString(prop->mName);
data->writeInt32(prop->mType);
switch (prop->mType) {
case MediaAnalyticsItem::kTypeInt32: {
    data->writeInt32(prop->u.int32Value);
    break;
}
case MediaAnalyticsItem::kTypeInt64: {
    data->writeInt64(prop->u.int64Value);
    break;
}
case MediaAnalyticsItem::kTypeDouble: {
    data->writeDouble(prop->u.doubleValue);
    break;
}
case MediaAnalyticsItem::kTypeCString: {
    data->writeCString(prop->u.CStringValue);
    break;
}
case MediaAnalyticsItem::kTypeRate: {
    data->writeInt64(prop->u.rate.count);
    data->writeInt64(prop->u.rate.duration);
    break;
}
default: {
    ALOGE("found bad Prop type: %d, idx %d, name %s", prop->mType, i,
        prop->mName);
    break;
}
}
}
```

# Case 3 - BnProducerListener::onTransact

location:frameworks/native/libs/gui/IProducerListener.cpp 

before modifying

```cpp
switch (code) {
    case ON_BUFFER_RELEASED:
        CHECK_INTERFACE(IProducerListener, data, reply);
        onBufferReleased();
        return NO_ERROR;
    case NEEDS_RELEASE_NOTIFY:
        CHECK_INTERFACE(IProducerListener, data, reply);
        reply->writeBool(needsReleaseNotify());
        return NO_ERROR;
}
```
after modifying
```cpp
switch (code) {
    case ON_BUFFER_RELEASED: {
        CHECK_INTERFACE(IProducerListener, data, reply);
        onBufferReleased();
        return NO_ERROR;
    }
    case NEEDS_RELEASE_NOTIFY: {
        CHECK_INTERFACE(IProducerListener, data, reply);
        reply->writeBool(needsReleaseNotify());
        return NO_ERROR;
    }
}
```


# Case 4 - setParameter of class android::MediaPlayer2

location: frameworks/av/media/libmediaplayer2/mediaplayer2.cpp

before modifying

```cpp
    switch (key) {
    case MEDIA2_KEY_PARAMETER_AUDIO_ATTRIBUTES:
        // save the marshalled audio attributes
        if (mAudioAttributesParcel != NULL) {
            delete mAudioAttributesParcel;
        }
        mAudioAttributesParcel = new Parcel();
        mAudioAttributesParcel->appendFrom(&request, 0, request.dataSize());
        status = setAudioAttributes_l(request);
        if (status != OK) {
            return status;
        }
        break;
    default:
        ALOGV_IF(mPlayer == NULL, "setParameter: no active player");
        break;
    }
```
after modifying

```cpp
  switch (key) {
  case MEDIA2_KEY_PARAMETER_AUDIO_ATTRIBUTES: {
    // save the marshalled audio attributes
    if (mAudioAttributesParcel != NULL) {
      delete mAudioAttributesParcel;
    }
    mAudioAttributesParcel = new Parcel();
    mAudioAttributesParcel->appendFrom(&request, 0, request.dataSize());
    status = setAudioAttributes_l(request);
    if (status != OK) {
      return status;
    }
    break;
  }
  default: {
    ALOGV_IF(mPlayer == NULL, "setParameter: no active player");
    break;
  }
  }
```

# Case 5 - SurfaceFlinger

The transaction process in SurfaceFlinger.cpp is very special compared with the transaction process in other interfaces. So we manually modify it and provide the corresponding modified version. 

Before extracting the rough interface model, you need to overwrite the original one as follows.

```shell
# backup
cp /path/to/aosp/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp /path/to/aosp/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp.bak
# overwrite
cp /path/to/fans/interface-model-extractor/pre-process/misc/SurfaceFlinger.cpp /path/to/aosp/frameworks/native/services/surfaceflinger/SurfaceFlinger.cpp
```