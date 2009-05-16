#include "client/client_connection.hpp"
#include "server/server.hpp"
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include "proto/hello.pb.h"
#include "boost/thread.hpp"
#include "thread/pcqueue.hpp"
#include <sstream>

DEFINE_string(server, "localhost", "The test server");
DEFINE_string(port, "6789", "The test server");
DEFINE_int32(num_threads, 4, "The test server thread number");
DECLARE_bool(logtostderr);
DECLARE_int32(v);

class EchoService2Impl : public Hello::EchoService2 {
 public:
 public:
  EchoService2Impl(boost::shared_ptr<PCQueue<bool> > pcqueue) : called_(0), cnt_(0), pcqueue_(pcqueue) {
  }
  virtual void Echo1(google::protobuf::RpcController *controller,
                    const Hello::EchoRequest *request,
                    Hello::EchoResponse *response,
                    google::protobuf::Closure *done) {
    LOG(INFO) << "Echo ServiceImpl called";
    response->set_echoed(1);
    response->set_text("server->" + request->question());
    response->set_close(false);
    done->Run();
    EchoService2::Stub stub(
        dynamic_cast<google::protobuf::RpcChannel*>(controller));
    Hello::EchoRequest2  *request2 = new Hello::EchoRequest2;
    Hello::EchoResponse2 *response2 = new Hello::EchoResponse2;
    request2->set_question("server question" + boost::lexical_cast<string>(i_++));
    RpcController controller2;
    google::protobuf::Closure *done2 = google::protobuf::NewCallback(
        this, &EchoService2Impl::CallEcho2Done, request2, response2);
    stub.Echo2(&controller2,
               request2,
               response2,
               done2);
    connection_ = dynamic_cast<Connection*>(controller);
    VLOG(2) << "CallEcho2 request: " << request2->question();
  }
  virtual void Echo2(google::protobuf::RpcController *controller,
                     const Hello::EchoRequest2 *request,
                    Hello::EchoResponse2 *response,
                    google::protobuf::Closure *done) {
    response->set_echoed(2);
    response->set_text("client->" + request->question());
    response->set_close(false);
    LOG(INFO) << "Echo2 called response with:" << response->text();
    done->Run();
  }
  void CallEcho2Done(Hello::EchoRequest2 *request,
                     Hello::EchoResponse2 *response) {
    if (!response->text().empty()) {
      CHECK_EQ("client->" + request->question(), response->text());
    }
    ++called_;
    VLOG(2) << "CallEcho2 response:" << response->text();
    delete response;
    delete request;
    pcqueue_->Push(called_ < FLAGS_num_threads);
  }
  int called() const {
    return called_;
  }
 private:
  boost::shared_ptr<PCQueue<bool> > pcqueue_;
  Connection *connection_;
  int cnt_;
  int called_;
  int i_;
};

class ListenTest : public testing::Test {
 public:
  ListenTest() {
  }

  void SetUp() {
    VLOG(2) << "New server connection";
    server_connection_.reset(new ProtobufConnection);
    server_connection_->set_name("Server");
    server_.reset(new Server(2, FLAGS_num_threads));
    VLOG(2) << "New client connection";
    client_connection_.reset(new ClientConnection(FLAGS_server, FLAGS_port));
    client_connection_->set_name("Client");
    client_stub_.reset(new Hello::EchoService2::Stub(client_connection_.get()));
    pcqueue_.reset(new PCQueue<bool>);
    echo_service_.reset(new EchoService2Impl(pcqueue_));
    VLOG(2) << "Register service";
    server_connection_->RegisterService(echo_service_.get());
    client_connection_->RegisterService(echo_service_.get());
    server_->Listen(FLAGS_server, FLAGS_port, server_connection_.get());
    CHECK(!client_connection_->IsConnected());
    CHECK(client_connection_->Connect());
  }

  void TearDown() {
    VLOG(2) << "Reset server connection";
    server_connection_.reset();
    server_->Stop();
    server_.reset();
    client_stub_.reset();
    VLOG(2) << "Reset client connection";
    client_connection_->Disconnect();
    client_connection_.reset();
    pcqueue_.reset();
    echo_service_.reset();
  }

  void ClientCallDone() {
    VLOG(2) << "ClientCallDone";
  }

  void ClientCallMultiThreadDone(
      boost::shared_ptr<RpcController> controller,
      boost::shared_ptr<Hello::EchoRequest> request,
      boost::shared_ptr<Hello::EchoResponse> response) {
    VLOG(2) << "CallEcho response: " << response->text();
    VLOG(2) << "ClientCallMultiThreadDone called";
    CHECK_EQ("server->" + request->question(), response->text());
  }

  void ClientThreadRun() {
    static int i = 0;
    boost::shared_ptr<Hello::EchoRequest> request(new Hello::EchoRequest);
    boost::shared_ptr<Hello::EchoResponse> response(new Hello::EchoResponse);
    request->set_question("client question" + boost::lexical_cast<string>(i++));
    boost::shared_ptr<RpcController> controller(new RpcController);
    client_stub_->Echo1(controller.get(),
                       request.get(),
                       response.get(),
                       NewClosure(boost::bind(
                           &ListenTest::ClientCallMultiThreadDone,
                           this, controller, request, response)));
    VLOG(2) << "CallEcho request: " << request->question();
  }
 protected:
  boost::scoped_ptr<ProtobufConnection> server_connection_;
  boost::scoped_ptr<Server> server_;
  boost::scoped_ptr<ClientConnection> client_connection_;
  RpcController listener_controller_;
  boost::scoped_ptr<Hello::EchoService2::Stub> client_stub_;
  boost::shared_ptr<PCQueue<bool> > pcqueue_;
  boost::scoped_ptr<EchoService2Impl> echo_service_;
};
/*
TEST_F(ListenTest, Test1) {
  Hello::EchoRequest request;
  Hello::EchoResponse response;
  request.set_question("client question");
  RpcController controller;
  client_stub_->Echo1(&controller,
                      &request,
                      &response,
                      NewClosure(boost::bind(
                          &ListenTest::ClientCallDone, this)));
  VLOG(2) << response.text() << " wait for server terminate";
  shared_ptr<boost::thread> t(new boost::thread(
      boost::bind(&boost::asio::io_service::run, client_io_service_)));
  pcqueue_->Pop();
  client_io_service_->stop();
  CHECK_EQ("server->" + request.question(), response.text());
  CHECK_EQ(echo_service_->called(), 1);
}
*/

TEST_F(ListenTest, MultiThreadTest1) {
  CHECK(client_connection_->Connect());
  // Create a pool of threads to run all of the io_services.
  vector<boost::shared_ptr<boost::thread> > threads;
  for (size_t i = 0; i < FLAGS_num_threads; ++i) {
    ClientThreadRun();
  }
  int cnt = 0;
  while (pcqueue_->Pop()) {
    ++cnt;
    VLOG(2) << "Get on pop" << cnt;
    if (cnt == FLAGS_num_threads) {
      break;
    }
  }
  client_connection_->Disconnect();
  VLOG(2) << "Close client connection";
  CHECK_EQ(echo_service_->called(), FLAGS_num_threads);
}
int main(int argc, char **argv) {
  FLAGS_v = 4;
  FLAGS_logtostderr = true;
  google::ParseCommandLineFlags(&argc, &argv, true);
  google::InitGoogleLogging(argv[0]);
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
