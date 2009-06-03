// http://code.google.com/p/server1/
//
// You can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// Author: xiliu.tang@gmail.com (Xiliu Tang)

#ifndef NET2_PROTOBUF_CONNECTION_HPP_
#define NET2_PROTOBUF_CONNECTION_HPP_
#include "base/base.hpp"
#include "server/connection.hpp"
#include <server/meta.pb.h>
#include <boost/function.hpp>
#include <boost/thread/condition.hpp>
#include <boost/thread/mutex.hpp>
#include <glog/logging.h>
#include <protobuf/message.h>
#include <protobuf/descriptor.h>
#include <protobuf/service.h>
#include "thread/notifier.hpp"
class ProtobufConnection;
class RpcController : virtual public google::protobuf::RpcController {
 public:
  RpcController() : notifier_(new Notifier("RPCController")) {
  }
  void Reset() {
    failed_.clear();
    notifier_.reset(new Notifier("RPCController"));
  }
  void SetFailed(const string &failed) {
    failed_ = failed;
  }
  bool Failed() const {
    return !failed_.empty();
  }
  string ErrorText() const {
    return failed_;
  }
  void StartCancel() {
  }
  bool IsCanceled() const {
    return false;
  }
  void NotifyOnCancel(google::protobuf::Closure *callback) {
  }
  bool Wait() {
    return notifier_->Wait();
  }
  bool Wait(int timeout_ms) {
    return notifier_->Wait(timeout_ms);
  }
  void Notify() {
    notifier_->Notify();
  }
 private:
  string failed_;
  bool responsed_;
  boost::shared_ptr<Notifier> notifier_;
};

class FullDualChannel : virtual public RpcController,
  virtual public google::protobuf::RpcChannel {
 public:
  typedef boost::signals2::signal<void()> CloseSignal;
  virtual bool RegisterService(google::protobuf::Service *service) = 0;
  virtual void CallMethod(const google::protobuf::MethodDescriptor *method,
                          google::protobuf::RpcController *controller,
                          const google::protobuf::Message *request,
                          google::protobuf::Message *response,
                          google::protobuf::Closure *done) = 0;
  virtual CloseSignal *close_signal() = 0;
  virtual bool IsConnected() = 0;
  virtual void Disconnect() = 0;
  virtual const string Name() = 0;
};

class ProtobufDecoder {
 public:
  /// Construct ready to parse the request method.
  ProtobufDecoder() : state_(Start) {
  };

  /// Parse some data. The boost::tribool return value is true when a complete request
  /// has been parsed, false if the data is invalid, indeterminate when more
  /// data is required. The InputIterator return value indicates how much of the
  /// input has been consumed.
  template <typename InputIterator>
  boost::tuple<boost::tribool, InputIterator> Decode(
      InputIterator begin, InputIterator end) {
    VLOG(2) << "Decode size: " << (end - begin);
    while (begin != end) {
      boost::tribool result = Consume(*begin++);
      if (result || !result) {
        return boost::make_tuple(result, begin);
      }
    }
    boost::tribool result = boost::indeterminate;
    return boost::make_tuple(result, begin);
  }

  const ProtobufLineFormat::MetaData &meta() const {
    return meta_;
  }
private:
  /// Handle the next character of input.
  boost::tribool Consume(char input);

  /// The current state of the parser.
  enum State {
    Start,
    Length,
    Content,
    End
  } state_;

  string length_store_;
  int length_;
  Buffer<char> content_;
  ProtobufLineFormat::MetaData meta_;
};

class ProtobufConnection : virtual public ConnectionImpl<ProtobufDecoder>, virtual public FullDualChannel {
 private:
   typedef hash_map<uint64, boost::function2<void, boost::shared_ptr<const ProtobufDecoder>,
          ProtobufConnection*> > HandlerTable;
 public:
  ProtobufConnection() : ConnectionImpl<ProtobufDecoder>(),
      handler_table_(new HandlerTable) {
    VLOG(2) << "New protobuf connection" << this;
  }
  CloseSignal *close_signal() {
    return Connection::close_signal();
  }
  virtual bool IsConnected() {
    return Connection::IsConnected();
  }
  virtual void Disconnect() {
    return Connection::Close();
  }
  virtual const string Name() {
    return name();
  }

  ~ProtobufConnection();

  ProtobufConnection* Clone();

  // Non thread safe.
  bool RegisterService(google::protobuf::Service *service);
  // Thread safe.
  void CallMethod(const google::protobuf::MethodDescriptor *method,
                  google::protobuf::RpcController *controller,
                  const google::protobuf::Message *request,
                  google::protobuf::Message *response,
                  google::protobuf::Closure *done);
 private:
  void ReleaseResponseTable();
  virtual void Cleanup();
  virtual void Handle(boost::shared_ptr<const ProtobufDecoder> decoder);
  boost::shared_ptr<HandlerTable> handler_table_;
  // The response handler table is per connection.
  HandlerTable response_handler_table_;
  boost::mutex response_handler_table_mutex_;
};
#endif  // NET2_PROTOBUF_CONNECTION_HPP_
