#include <iostream>
#include <string>
#include "server.hpp"
#include "protobuf_connection.hpp"
#include <pthread.h>
#include <signal.h>
#include <gflags/gflags.h>
#include <boost/thread.hpp>
DEFINE_string(address, "","The address");
DEFINE_string(port, "","The port");
DEFINE_int32(num_threads, 1,"The thread size");

int main(int argc, char* argv[]) {
  try
  {
    // Check command line arguments.
    if (argc != 5)
    {
      std::cerr << "Usage: http_server <address> <port> <threads> <doc_root>\n";
      std::cerr << "  For IPv4, try:\n";
      std::cerr << "    receiver 0.0.0.0 80 1 .\n";
      std::cerr << "  For IPv6, try:\n";
      std::cerr << "    receiver 0::0 80 1 .\n";
      return 1;
    }

    // Block all signals for background thread.
    sigset_t new_mask;
    sigfillset(&new_mask);
    sigset_t old_mask;
    pthread_sigmask(SIG_BLOCK, &new_mask, &old_mask);

    // Run server in background thread.
    size_t num_threads = lexical_cast<std::size_t>(argv[3]);
    ConnectionPtr protobuf_connection(new ProtobufConnection);
    Server s(FLAGS_address,
             FLAGS_port,
             FLAGS_num_threads,
             protobuf_connection);
    boost::thread t(boost::bind(&Server::Run, &s));

    // Restore previous signals.
    pthread_sigmask(SIG_SETMASK, &old_mask, 0);

    // Wait for signal indicating time to shut down.
    sigset_t wait_mask;
    sigemptyset(&wait_mask);
    sigaddset(&wait_mask, SIGINT);
    sigaddset(&wait_mask, SIGQUIT);
    sigaddset(&wait_mask, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &wait_mask, 0);
    int sig = 0;
    sigwait(&wait_mask, &sig);

    // Stop the server.
    s.Stop();
    t.join();
  }
  catch (std::exception& e)
  {
    std::cerr << "exception: " << e.what() << "\n";
  }
  return 0;
}
