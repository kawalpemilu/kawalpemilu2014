#ifndef SIMPLE_HTTP_
#define SIMPLE_HTTP_

#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <string>

namespace simple_http {

  using std::map;
  using std::string;
  using std::unique_ptr;
  using std::ostringstream;

  class Request {
   public:
    map<string, string> headers;
    string url;
    string body;

    void clear() {
      headers.clear();
      url = body = "";
    }
  };

  class ResponseImpl;

  // The Response class can be used for asynchronous processing.
  class Response {
   public:
    enum class Code {
      OK,
      NOT_FOUND,
      SERVER_ERROR,
    };

    Response(ResponseImpl*);
    ~Response();

    // Response body. Fill this before calling send() or flush().
    ostringstream& body();

    // Enable HTTP Cache for the specified seconds (default not specified).
    // An optional last modified header may be specified to improve HTTP caching.
    void set_max_age(int seconds, int last_modified = 0);

    // Prints warning to the console if the send() method is called later than
    // the specified milliseconds after the handler is called (default = 500 ms).
    void set_max_runtime_warning(int milliseconds);

    // Sends the response to the client with the specified code.
    // No more appends to body allowed after calling send().
    void send(Code code = Code::OK);

   private:
    // Not owned by this class.
    ResponseImpl* impl;
  };

  class VarzImpl;

  // Statistis for monitoring.
  class Varz {
   public:
    Varz();
    ~Varz();
    unsigned long long get(string key);
    void set(string key, unsigned long long value);
    void inc(string key, unsigned long long value = 1);
    void latency(string key, int us);
    void print_to(ostringstream &ss);

   private:
    unique_ptr<VarzImpl> impl;
  };

  typedef std::function<void(Request&, Response&)> Handler;

  class ServerImpl;

  class Server {
   public:
    Server();
    ~Server();

    // Non-copy-able.
    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    // Handles http requests where the URL matches the specified prefix.
    void get(string prefix, Handler);

    // Starts the http server at the specified address and port.
    void listen(string address, int port);

    // Server statistics.
    Varz* varz();

   private:
    unique_ptr<ServerImpl> impl;
  };

  // Global logging to standard error.
  struct Log {
    static int max_level;
    static void severe(const char *fmt, ... );
    static void warn(const char *fmt, ... );
    static void info(const char *fmt, ... );
  };


  class ClientImpl;

  class Client {
   public:

    Client(const char *addr, int port = 80);
    ~Client();

    void request(const char *url, const string &body,
      std::function<void(const string&)> response_callback);

    void close();

   private:
    unique_ptr<ClientImpl> impl;
  };

}

#endif
