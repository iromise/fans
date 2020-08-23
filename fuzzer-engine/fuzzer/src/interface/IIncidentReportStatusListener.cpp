// #include <fuzzer/interface/IIncidentReportStatusListener.h>
// #include <fuzzer/types/types.h>

// class SimpleListener : public IIncidentReportStatusListener {
// public:
//   SimpleListener(){};
//   virtual ~SimpleListener(){};

//   virtual Status onReportStarted() { return Status::ok(); };
//   virtual Status onReportSectionStatus(int /*section*/, int /*status*/) {
//     return Status::ok();
//   };
//   virtual Status onReportFinished() { return Status::ok(); };
//   virtual Status onReportFailed() { return Status::ok(); };
// };

// sp<IBinder> generateIIncidentReportStatusListener() {
//   sp<SimpleListener> listener = new SimpleListener();
//   return IInterface::asBinder(listener);
// }
