#include "client/client_connection.hpp"
bool ClientConnection::ConnectionClose() {
  VLOG(2) << "ClientConnection::ConnectionClose";
  delete connection_;
  connection_ = NULL;
}
bool ClientConnection::Connect() {
  if (connection_ && connection_->IsConnected()) {
    LOG(WARNING) << "Connect but IsConnected";
    return true;
  }
  Disconnect();
  if (out_threadpool_  == NULL) threadpool_.Start();
  if (out_io_service_pool_ == NULL) io_service_pool_.Start();
  boost::asio::ip::tcp::resolver::query query(server_, port_);
  boost::asio::ip::tcp::resolver resolver(GetIOService());
  boost::asio::ip::tcp::resolver::iterator endpoint_iterator(
      resolver.resolve(query));
  boost::asio::ip::tcp::resolver::iterator end;
  // Try each endpoint until we successfully establish a connection.
  boost::system::error_code error = boost::asio::error::host_not_found;
  boost::asio::ip::tcp::socket *socket =
    new boost::asio::ip::tcp::socket(GetIOService());
  while (error && endpoint_iterator != end) {
    socket->close();
    socket->connect(*endpoint_iterator++, error);
  }
  if (error) {
    delete socket;
    LOG(WARNING) << ":fail to connect, error:"  << error.message();
    return false;
  }
  connection_ = connection_template_.Clone();
  connection_->set_socket(socket);
  connection_->push_close_handler(boost::bind(&ClientConnection::ConnectionClose, this));
  if (out_threadpool_) {
    connection_->set_executor(out_threadpool_);
  } else {
    connection_->set_executor(&threadpool_);
  }
  boost::shared_ptr<ConnectionStatus> status(new ConnectionStatus);
  connection_->set_connection_status(status);
  return true;
}
