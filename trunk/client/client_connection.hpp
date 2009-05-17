#ifndef CLIENT_CONNECTION_HPP
#define CLIENT_CONNECTION_HPP
#include "server/protobuf_connection.hpp"
#include "server/io_service_pool.hpp"
class ClientConnection : public FullDualChannel {
 public:
  ClientConnection(const string &server, const string &port)
    : connection_(NULL), io_service_pool_("ClientIOService", 1),
      threadpool_("ClientThreadPool", kClientThreadPoolSize), server_(server), port_(port), out_threadpool_(NULL), out_io_service_pool_(NULL) {
      VLOG(2) << "Constructor client connection";
    connection_template_.set_name(server + "::" + port + "::Client");
  }
  virtual bool RegisterService(google::protobuf::Service *service) {
    return connection_template_.RegisterService(service);
  }
  virtual void CallMethod(const google::protobuf::MethodDescriptor *method,
                          google::protobuf::RpcController *controller,
                          const google::protobuf::Message *request,
                          google::protobuf::Message *response,
                          google::protobuf::Closure *done) {
    if (connection_) {
      connection_->CallMethod(method, controller, request, response, done);
    } else {
      LOG(WARNING) << "Callmethod but connection is null";
    }
  }

  void set_name(const string &name) {
    connection_template_.set_name(name);
  }
  const string name() {
    return connection_template_.name();
  }
  void set_io_service_pool(IOServicePool *io_service_pool) {
    out_io_service_pool_ = io_service_pool;
  }

  void set_threadpool(ThreadPool *out_threadpool) {
    out_threadpool_ = out_threadpool;
  }
  bool IsConnected() {
    return connection_ && connection_->IsConnected();
  }
  bool Connect();
  void Disconnect() {
    if (out_threadpool_ == NULL) {
      threadpool_.Stop();
    }
    if (connection_) {
      connection_->Close();
    }
    if (out_io_service_pool_ == NULL) {
      io_service_pool_.Stop();
    }
  }
  ~ClientConnection() {
    VLOG(2) << "~ClientConnection";
  }
 private:
  boost::asio::io_service &GetIOService() {
    if (out_io_service_pool_) {
      return out_io_service_pool_->get_io_service();
    }
    return io_service_pool_.get_io_service();
  }

  static const int kClientThreadPoolSize = 1;
  bool ConnectionClose();
  ProtobufConnection *connection_;
  ProtobufConnection connection_template_;
  IOServicePool io_service_pool_;
  ThreadPool threadpool_;
  ThreadPool *out_threadpool_;
  IOServicePool *out_io_service_pool_;
  string server_, port_;
};

class RpcController : public google::protobuf::RpcController {
 public:
  void Reset() {
    failed_.clear();
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
 private:
  string failed_;
};
#endif  // CLIENT_CONNECTION_HPP
