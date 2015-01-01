#include "http_parser.h"
#include "simple_http.h"
#include "uv.h"

#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <queue>

namespace simple_http {

using std::chrono::duration_cast;
using std::chrono::microseconds;
using std::chrono::high_resolution_clock;
using std::chrono::time_point;
using std::function;
using std::max;
using std::min;
using std::pair;
using std::queue;
using std::vector;

// Utility to produce a histogram of request latencies.
class LatencyHistogram {
  int buckets[31];

 public:
  LatencyHistogram() {
    for (int i = 0; i < 31; i++) buckets[i] = 0;
  }
  ~LatencyHistogram() {}

  void add(int us) {
    if (us < 0) Log::severe("Adding negative runtime: %d us", us);
    else buckets[31 - __builtin_clz(max(us, 1))]++;
  }
  void print(ostringstream &ss) {
    ss << "[";
    for (int i = 0; i < 30; i++) ss << buckets[i] << ",";
    ss << buckets[30] << "]";
  }
};

class VarzImpl {
 public:

  VarzImpl() {}
  ~VarzImpl() {}

  unsigned long long get(string key) { return varz[key]; }
  void set(string key, unsigned long long value) { varz[key] = value; }
  void inc(string key, unsigned long long value) { varz[key] += value; }
  void latency(string key, int us) {
    if (!varz_hist.count(key)) {
      varz_hist[key] = unique_ptr<LatencyHistogram>(new LatencyHistogram());
    }
    varz_hist[key]->add(us);
  }
  void print_to(ostringstream &ss) {
    ss << "{\n";
    bool first = true;
    for (auto &it : varz) {
      if (first) first = false; else ss << ",\n";
      ss << "\"" << it.first << "\":" << it.second;
    }
    for (auto &it : varz_hist) {
      if (first) first = false; else ss << ",\n";
      ss << "\"" << it.first << "\":";
      it.second->print(ss);
    }
    ss << "\n}\n";
  }

 private:
  map<string, unique_ptr<LatencyHistogram>> varz_hist;
  map<string, unsigned long long> varz;
};


class ServerImpl {
 public:
  ServerImpl();
  void get(string path, Handler handler);
  void listen(string address, int port);

  vector<pair<string, Handler>> handlers;
  Varz varz;
};


// HttpParser's state.
enum class HttpParserState {
  READING_URL,
  READING_HEADER_FIELD,
  READING_HEADER_VALUE,
  CLOSED,
};

constexpr int MAX_BUFFER_LEN = 64 * 1024;

class HttpParser {
 public:
  HttpParser();
  ~HttpParser();

  // Start reading from the stream, and calls on_message_complete callback when a complete http request is received.
  // The callback may be called multiple times (i.e., http pipelining) until the stream is closed.
  void start(
    uv_stream_t *stream,
    enum http_parser_type type,
    function<void(Request&)> on_message_complete,
    function<void()> on_close);

 // private:
  int append_url(const char *p, size_t len);
  int append_header_field(const char *p, size_t len);
  int append_header_value(const char *p, size_t len);
  int append_body(const char *p, size_t len);

  bool parse(const char *buf, ssize_t nread);

  void reset();                         // Prepare the HttpParser for the next request.
  void build_request();                 // Make the request ready for consumption.
  void close();

  uv_stream_t* tcp;                     // Not owned, passed in through start(), used for close().
  http_parser_settings parser_settings; // Built-in implementation of parsing http requests.
  char buffer[MAX_BUFFER_LEN];          // Reading buffer.
  ostringstream temp_hf_;               // Header field.
  ostringstream temp_hv_;               // Header value.
  ostringstream url_;                   // Request URL.
  ostringstream body_;                  // Request body.
  http_parser parser;                   // HTTP parser to parse the client http requests.
  Request request;                      // Previous Calls to parse() are used to build this Request.
  function<void(Request&)> msg_cb; // Callback on message complete.
  function<void()> close_cb;       // Callback on close.
  HttpParserState state;                // The state of the current parsing request.
};


// One Connection instance per client.
// HTTP pipelining is supported.
class Connection {
 public:
  Connection(ServerImpl*);
  ~Connection();

  ServerImpl *server;       // The server that created this connection object.
  ResponseImpl* create_response(string prefix);      // Returns a detached object for async response.
  void flush_responses();
  bool disposeable();
  void cleanup();           // Flush all pending responses and destroy this connection if no longer used.

  queue<ResponseImpl*> responses;
  uv_tcp_t handle;          // TCP connection handle to the client browser.
  HttpParser the_parser;    // The parser for the TCP stream handle.
};


class ResponseImpl {
 public:
  ostringstream body;
  int max_age_s;
  int max_runtime_ms;
  int last_modified;

  ResponseImpl(Connection *con, string req_url);
  ~ResponseImpl();

  // In a pipelined response, this send request will be queued if it's not the head.
  void send(Response::Code code);

  // Send the body to client then asynchronously call "cb".
  void flush(uv_write_cb cb);

  Connection* connection() { return c; }
  int get_state() { return state; }
  void finish() {
    assert(state == 2);
    state = 3;
    c->cleanup();
  }

 private:
  Connection *c; // Not owned.
  string url;
  time_point<high_resolution_clock> start_time;
  uv_buf_t send_buffer;
  int state; // 0 = initialized, 1 = after send(), 2 = after flush(), 3 = finished
  Response::Code code;
  uv_write_t write_req;
};


Varz::Varz(): impl(unique_ptr<VarzImpl>(new VarzImpl())) {}
Varz::~Varz() {}
unsigned long long Varz::get(string key) { return impl->get(key); }
void Varz::set(string key, unsigned long long value) { impl->set(key, value); }
void Varz::inc(string key, unsigned long long value) { impl->inc(key, value); }
void Varz::latency(string key, int us) { impl->latency(key, us); }
void Varz::print_to(ostringstream &ss) { impl->print_to(ss); }



Server::Server(): impl(unique_ptr<ServerImpl>(new ServerImpl())) {}
Server::~Server() {}
void Server::get(string prefix, Handler handler) { impl->get(prefix, handler); }
void Server::listen(string address, int port) { impl->listen(address, port); }
Varz* Server::varz() { return &impl->varz; }


Response::Response(ResponseImpl *r): impl(r) {}
Response::~Response() {}
void Response::set_max_age(int seconds, int last_modified) {
  assert(impl);
  impl->max_age_s = seconds;
  impl->last_modified = last_modified;
}
void Response::set_max_runtime_warning(int milliseconds) {
  assert(impl);
  impl->max_runtime_ms = milliseconds;
}
void Response::send(Code code) {
  assert(impl);
  impl->send(code);
  impl = nullptr;
}
ostringstream& Response::body() { assert(impl); return impl->body; }



ServerImpl::ServerImpl() {
  get("/varz", [&](Request& req, Response& res) {
    varz.print_to(res.body());
    res.send();
  });
}

static bool is_prefix_of(const string &prefix, const string &str) {
  auto res = std::mismatch(prefix.begin(), prefix.end(), str.begin());
  return res.first == prefix.end();
}

void ServerImpl::get(string path, Handler handler) {
  for (auto &it : handlers) {
    if (is_prefix_of(it.first, path)) {
      Log::severe("Path '%s' cannot be the prefix of path '%s'", it.first.c_str(), path.c_str());
      abort();
    }
  }
  handlers.push_back(make_pair(path, handler));
}

static void on_connect(uv_stream_t* server_handle, int status) {
  ServerImpl *server = static_cast<ServerImpl*>(server_handle->data);
  assert(server && !status);
  Connection* c = new Connection(server);
  c->server->varz.inc("server_connection_alloc");
  uv_tcp_init(uv_default_loop(), &c->handle);
  status = uv_accept(server_handle, (uv_stream_t*) &c->handle);
  assert(!status);

  c->the_parser.start((uv_stream_t*) &c->handle, HTTP_REQUEST,
    [c](Request &req) {
      // On message complete.
      Log::info("message complete con %p, the_parser = %p, url = %s", c, &c->the_parser, req.url.c_str());
      c->server->varz.inc("server_on_message_complete");
      for (auto &it : c->server->handlers) {
        if (is_prefix_of(it.first, req.url)) {
          // Handle the request.
          Response res { c->create_response(it.first) };
          it.second(req, res);
          return;
        }
      }
      // No handler for the request, send 404 error.
      Response res { c->create_response("/unknown") };
      res.body() << "Request not found for " << req.url;
      res.send(Response::Code::NOT_FOUND);

    }, [c]() {
      // On close.
      // Log::info("Connection closing %p", c);
      c->cleanup();
    });
}

void ServerImpl::listen(string address, int port) {
  signal(SIGPIPE, SIG_IGN);
  uv_tcp_t server;
  server.data = this;
  int status = uv_tcp_init(uv_default_loop(), &server);
  assert(!status);
  // struct sockaddr_in addr;
  // status = uv_ip4_addr(address.c_str(), port, &addr);
  // assert(!status);
  // status = uv_tcp_bind(&server, (const struct sockaddr*) &addr);
  status = uv_tcp_bind(&server, uv_ip4_addr(address.c_str(), port));
  assert(!status);
  uv_listen((uv_stream_t*)&server, 128, on_connect);
  Log::info("Listening on port %d", port);
  varz.set("server_start_time", time(NULL));
  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}



#define CRLF "\r\n"
#define CORS_HEADERS                                       \
  "Content-Type: application/json; charset=utf-8"     CRLF \
  "Access-Control-Allow-Origin: *"                    CRLF \
  "Access-Control-Allow-Methods: GET, POST, OPTIONS"  CRLF \
  "Access-Control-Allow-Headers: X-Requested-With"    CRLF

static void after_flush(uv_write_t* req, int status) {
  ResponseImpl* res = static_cast<ResponseImpl*>(req->data);
  assert(res);
  res->finish();
}

ResponseImpl::ResponseImpl(Connection *con, string req_url):
  max_age_s(0),
  max_runtime_ms(500),
  last_modified(0),
  c(con),
  url(req_url),
  start_time(high_resolution_clock::now()),
  send_buffer({nullptr, 0}),
  state(0) {}

ResponseImpl::~ResponseImpl() {
  if (send_buffer.base) {
    c->server->varz.inc("server_send_buffer_dealloc");
    delete[] send_buffer.base;
  }
}

void ResponseImpl::send(Response::Code code) {
  assert(c);          // send() can only be called exactly once.
  this->state = 1;    // after send().
  this->code = code;
  // Log::info("RESPONSE send con = %p, code = %d", c, code);
  c->cleanup();
}

void ResponseImpl::flush(uv_write_cb cb) {
  assert(state == 1);
  state = 2; // after flush().
  assert(c);
  assert(c->the_parser.state != HttpParserState::CLOSED);
  assert(!c->disposeable());
  c->server->varz.inc("server_response_send");

  ostringstream ss; 
  switch (code) {
    case Response::Code::OK: ss << "HTTP/1.1 200 OK" CRLF CORS_HEADERS; break;
    case Response::Code::NOT_FOUND: ss << "HTTP/1.1 400 URL Request Error" CRLF CORS_HEADERS; break;
    case Response::Code::SERVER_ERROR: ss << "HTTP/1.1 500 Internal Server Error" CRLF CORS_HEADERS; break;
    default: Log::severe("unknown code %d", code); assert(0); break;
  }
  string body_str = body.str();
  ss << "Content-Length: " << body_str.length() << "\r\n";
  if (max_age_s > 0) {
    ss << "Cache-Control: public,max-age=" << max_age_s << "\r\n";
    if (last_modified > 0) {
      time_t t = last_modified;
      char buffer[80];
      strftime(buffer, 80, "%a, %d %b %Y %H:%M:%S GMT", gmtime(&t));
      ss << "Last-Modified: " << buffer << std::endl;
      // ss << "Last-Modified: " << std::put_time(gmtime(&t), "%a, %d %b %Y %H:%M:%S GMT") << std::endl;
    }
  }
  ss << "\r\n" << body_str << "\r\n";

  string s = ss.str();
  send_buffer.base = new char[send_buffer.len = s.length()];
  c->server->varz.inc("server_send_buffer_alloc");
  memcpy(send_buffer.base, s.data(), send_buffer.len);
  c->server->varz.inc("server_sent_bytes", send_buffer.len);

  auto dur = duration_cast<microseconds>(high_resolution_clock::now() - start_time).count();
  c->server->varz.latency("server_response", dur);
  c->server->varz.latency(url, dur);
  if (dur * 1e-3 >= max_runtime_ms) {
    Log::warn("runtime = %6.3lf, prefix = %s", dur * 1e-6, url.c_str());
  }

  write_req.data = this;
  int error = uv_write((uv_write_t*) &write_req, (uv_stream_t*) &c->handle, &send_buffer, 1, cb);
  if (error) Log::severe("Could not write %d for request %s", error, url.c_str());
}



Connection::Connection(ServerImpl *s): server(s) {
  handle.data = this;
  // Log::warn("Connection created %p", this);
}

Connection::~Connection() {
  // Log::warn("Connection destroyed %p", this);
}

ResponseImpl* Connection::create_response(string prefix) {
  ResponseImpl* res = new ResponseImpl(this, prefix);
  server->varz.inc("server_response_impl_alloc");
  responses.push(res);
  return res;
}

void Connection::cleanup() {
  flush_responses();
  if (disposeable()) {
    server->varz.inc("server_connection_dealloc");
    // Log::warn("Connection DELETE: %p", this);
    delete this;
  }
}

void Connection::flush_responses() {
  while (!responses.empty()) {
    ResponseImpl* res = responses.front();
    if (res->get_state() == 0) return; // Not yet responded.
    if (res->get_state() == 1 && the_parser.state != HttpParserState::CLOSED) return res->flush(after_flush);
    if (res->get_state() == 2) return; // Not yet written.
    responses.pop();
    server->varz.inc("server_response_impl_dealloc");
    delete res;
  }
}

bool Connection::disposeable() {
  return the_parser.state == HttpParserState::CLOSED && responses.empty();
}


/***** Logger *****/

#define LOG_FMT_STDERR(prefix)        \
  va_list args; va_start(args, fmt);  \
  fprintf(stderr, prefix);            \
  vfprintf(stderr, fmt, args);        \
  fprintf(stderr, "\n");              \
  va_end(args);

void Log::severe(const char *fmt, ... ) { LOG_FMT_STDERR("\e[00;31mSEVERE\e[00m ") }
void Log::warn(const char *fmt, ... ) { LOG_FMT_STDERR("\e[1;33mWARN\e[00m ") }
void Log::info(const char *fmt, ... ) { LOG_FMT_STDERR("\e[0;32mINFO\e[00m ") }

#undef LOG_FMT_STDERR



/***** Simple Parser *****/

static int on_url(http_parser* parser, const char* p, size_t len) {
  return static_cast<HttpParser*>(parser->data)->append_url(p, len);
}

static int on_header_field(http_parser* parser, const char* at, size_t len) {
  return static_cast<HttpParser*>(parser->data)->append_header_field(at, len);
}

static int on_header_value(http_parser* parser, const char* at, size_t len) {
  return static_cast<HttpParser*>(parser->data)->append_header_value(at, len);
}

static int on_headers_complete(http_parser* parser) {
  return static_cast<HttpParser*>(parser->data)->append_header_field("", 0);
}

static int on_body(http_parser* parser, const char* p, size_t len) {
  return static_cast<HttpParser*>(parser->data)->append_body(p, len);
}

static int on_message_complete(http_parser* parser) {
  HttpParser* c = static_cast<HttpParser*>(parser->data);
  if (c->state != HttpParserState::CLOSED) {
    c->build_request();
    c->request.body = c->body_.str();
    // Log::info("on_message_complete parser %p : %s", c, c->request.body.c_str());
    c->msg_cb(c->request);
    c->reset(); // Recycle the HttpParser and request object.
  }
  return 0;   // Continue parsing.
}

HttpParser::HttpParser() {
  memset(&parser_settings, 0, sizeof(http_parser_settings));
  parser_settings.on_url = on_url;
  parser_settings.on_header_field = on_header_field;
  parser_settings.on_header_value = on_header_value;
  parser_settings.on_headers_complete = on_headers_complete;
  parser_settings.on_body = on_body;
  parser_settings.on_message_complete = on_message_complete;

  parser.data = this;

  reset();
}

HttpParser::~HttpParser() {}

static uv_buf_t on_alloc_uv_0_10(uv_handle_t* handle, size_t suggested_size) {
  HttpParser* c = static_cast<HttpParser*>(handle->data);
  assert(suggested_size == MAX_BUFFER_LEN); // libuv dependent code.
  return uv_buf_init(c->buffer, MAX_BUFFER_LEN);
}

static void on_alloc(uv_handle_t* handle, size_t suggested_size, uv_buf_t *buf) {
  HttpParser* c = static_cast<HttpParser*>(handle->data);
  assert(suggested_size == MAX_BUFFER_LEN); // libuv dependent code.
  buf->base = c->buffer;
  buf->len = MAX_BUFFER_LEN;
}

static void on_close(uv_handle_t* handle) {
  HttpParser* c = static_cast<HttpParser*>(handle->data);
  assert(c && c->state != HttpParserState::CLOSED);
  c->state = HttpParserState::CLOSED;
  c->close_cb();
}

static void on_read_uv_0_10(uv_stream_t* tcp, ssize_t nread, uv_buf_t buf) {
  HttpParser* c = static_cast<HttpParser*>(tcp->data);
  assert(c);
  if (nread < 0 || !c->parse(buf.base, nread)) {
    assert(c->state != HttpParserState::CLOSED);
    c->close();
  }
}

static void on_read(uv_stream_t* tcp, ssize_t nread, const uv_buf_t *buf) {
  HttpParser* c = static_cast<HttpParser*>(tcp->data);
  assert(c);
  // Log::info("on_read parser %p, nread = %d", c, nread);
  // Log::info("%.*s", buf->len, buf->base);
  if (nread < 0 || !c->parse(buf->base, nread)) {
    assert(c->state != HttpParserState::CLOSED);
    c->close();
  }
}

void HttpParser::start(
    uv_stream_t *stream,
    enum http_parser_type type,
    function<void(Request&)> on_message_complete,
    function<void()> on_close_cb) {
  tcp = stream;
  stream->data = this;
  msg_cb = on_message_complete;
  close_cb = on_close_cb;
  http_parser_init(&parser, type);
  uv_read_start(stream, on_alloc_uv_0_10, on_read_uv_0_10);
}

void HttpParser::close() {
  uv_close((uv_handle_t*) tcp, on_close);
}

static void clear_ss(ostringstream &ss) { ss.clear(); ss.str(""); }

void HttpParser::reset() {
  state = HttpParserState::READING_URL;
  clear_ss(temp_hf_);
  clear_ss(temp_hv_);
  clear_ss(url_);
  clear_ss(body_);
  request.clear();
}

int HttpParser::append_url(const char *p, size_t len) {
  assert(state == HttpParserState::READING_URL);
  url_.write(p, len);
  return 0;
}

/* Converts a hex character to its integer value */
static char from_hex(char ch) {
  return isdigit(ch) ? ch - '0' : tolower(ch) - 'a' + 10;
}

/* Converts an integer value to its hex character*/
static char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

/* Returns a url-decoded version of str */
static char* url_decode(char *str, char *buf) {
  char *pstr = str, *pbuf = buf;
  while (*pstr) {
    if (*pstr == '%') {
      if (pstr[1] && pstr[2]) {
        *pbuf++ = from_hex(pstr[1]) << 4 | from_hex(pstr[2]);
        pstr += 2;
      }
    } else if (*pstr == '+') {
      *pbuf++ = ' ';
    } else {
      *pbuf++ = *pstr;
    }
    pstr++;
  }
  *pbuf = '\0';
  return buf;
}

void HttpParser::build_request() {
  if (state == HttpParserState::READING_URL) {
    request.url = url_.str();
    char *url = (char*) request.url.c_str();
    url_decode(url, url);
    state = HttpParserState::READING_HEADER_FIELD;
  }
  if (state == HttpParserState::READING_HEADER_VALUE) {
    request.headers[temp_hf_.str()] = temp_hv_.str();
    clear_ss(temp_hf_);
    clear_ss(temp_hv_);
    state = HttpParserState::READING_HEADER_FIELD;
  }
}

int HttpParser::append_header_field(const char *p, size_t len) {
  build_request();
  temp_hf_.write(p, len);
  return 0;
}

int HttpParser::append_header_value(const char *p, size_t len) {
  assert(state != HttpParserState::READING_URL);
  if (state == HttpParserState::READING_HEADER_FIELD) {
    state = HttpParserState::READING_HEADER_VALUE;
    temp_hv_.write(p, len);
  }
  return 0;
}

int HttpParser::append_body(const char *p, size_t len) {
  // Log::info("body = %.*s", len, p);
  body_.write(p, len);
  return 0;
}

bool HttpParser::parse(const char *buf, ssize_t nread) {
  ssize_t parsed = http_parser_execute(&parser, &parser_settings, buf, nread);
  assert(parsed <= nread);
  return parsed == nread;
}


/***** Http Client *****/

enum class ClientState {
  DISCONNECTED,
  CONNECTING,
  CONNECTED,
  UNINITED,
  WAITING
};

class ClientImpl {
 public:
  ClientImpl(const char *h, int p);

  void request(const char *url, const string &body, function<void(const string&)> response_callback);
  void flush();
  void try_connect();
  void close();

  const string host;
  int port;
  int timeout;
  uv_connect_t connect_req;
  uv_timer_t connect_timer;
  uv_tcp_t handle;
  HttpParser the_parser;    // The parser for the TCP stream handle.

  queue<pair<string, string>> req_queue; // url, body.
  queue<function<void(const string&)>> cb_queue;
  ClientState connection_status;
  bool is_idle;
};

static void on_connect(uv_connect_t *req, int status);
static void try_connect_cb(uv_timer_t* handle, int status) {
  ClientImpl *c = (ClientImpl*) handle->data;
  Log::warn("Connecting to %s:%d", c->host.c_str(), c->port);
  c->connection_status = ClientState::CONNECTING;
  uv_tcp_init(uv_default_loop(), &c->handle);
  int r = uv_tcp_connect(&c->connect_req, &c->handle, uv_ip4_addr(c->host.c_str(), c->port), on_connect);
  if (r) {
    Log::severe("Failed connecting to %s:%d", c->host.c_str(), c->port);
    on_connect(&c->connect_req, r);
  }
}

static void on_connect(uv_connect_t *req, int status) {
  ClientImpl *c = (ClientImpl*) req->data;
  assert(&c->connect_req == req);
  if (status) {
    Log::severe("on_connect by %s:%d, queue = %lu/%lu\n",
      c->host.c_str(), c->port, c->req_queue.size(), c->cb_queue.size());
    c->connection_status = ClientState::DISCONNECTED;
    c->try_connect();
    c->timeout = min(c->timeout * 2, 8000);
    return;
  }

  c->timeout = 1000;
  Log::info("CONNECTED %s:%d, queue=%d/%d, readable=%d, writable=%d, status=%d, is_idle=%d",
    c->host.c_str(), c->port, c->req_queue.size(), c->cb_queue.size(),
    uv_is_readable(req->handle), uv_is_writable(req->handle), status, c->is_idle);
  c->is_idle = true;
  c->connection_status = ClientState::CONNECTED;

  assert(uv_is_readable(req->handle));
  assert(uv_is_writable(req->handle));
  assert(!uv_is_closing((uv_handle_t*) req->handle));

  // Log::info("CB SIZE = %d", c->cb_queue.size());
  c->the_parser.start((uv_stream_t*) &c->handle, HTTP_RESPONSE,
    [c](Request &req) {
      // On message complete.
      assert(!c->cb_queue.empty());
      c->cb_queue.front()(req.body);
      c->cb_queue.pop();
      c->req_queue.pop();
      c->is_idle = true;
      c->flush();
      // Log::info("complete qsize = %d/%d, %d", 
      //   c->req_queue.size(), c->cb_queue.size(), c->connection_status == ClientState::CONNECTED);

    }, [c] () {
      // On close.
      if (c->connection_status == ClientState::CONNECTED) {
        Log::severe("Connection closed %s:%d, reconnecting", c->host.c_str(), c->port);
        c->connection_status = ClientState::DISCONNECTED;
        c->timeout = 1000;
        c->try_connect();
      } else {
        Log::info("Client DISCONNECTED");
      }
    });

  c->flush();
}

Client::Client(const char *host, int port): impl(unique_ptr<ClientImpl>(new ClientImpl(host, port))) {}
Client::~Client() {}

void Client::request(const char *url, const string &body,
    function<void(const string&)> response_callback) {
  impl->request(url, body, response_callback);
}

void Client::close() {
  impl->close();
}

ClientImpl::ClientImpl(const char *h, int p): host(h), port(p) {
  connect_req.data = this;
  connection_status = ClientState::UNINITED;
  connect_timer.data = this;
  timeout = 1000;
  is_idle = false;
}

void ClientImpl::request(const char *url, const string &body, function<void(const string&)> response_callback) {
  req_queue.push(make_pair(url, body));
  cb_queue.push(response_callback);
  // Log::info("request qsize = %d/%d, %d",
  //   req_queue.size(), cb_queue.size(), connection_status == ClientState::CONNECTED);
  if (connection_status == ClientState::CONNECTED) {
    flush();
  } else {
    try_connect();
  }
}

static void after_write(uv_write_t *req, int status) {
  // Log::info("after_write");
  assert(status == 0);
  free(req->data);
  free(req);
}

static void write_string(char *s, int length, uv_tcp_t* tcp) {
  // Log::info("writing: %.*s", length, s);
  uv_buf_t buf = uv_buf_init(s, length);
  uv_write_t *req = (uv_write_t*) malloc(sizeof(*req));
  req->data = malloc(length);
  memcpy((char*) req->data, s, length);
  if (uv_write(req, (uv_stream_t*)tcp, &buf, 1, after_write)) {
    Log::severe("uv_write failed");
    assert(0);
  }
}

void ClientImpl::flush() {
  if (connection_status == ClientState::CONNECTED) {
    if (is_idle && !req_queue.empty()) {
      char url[1024];
      assert(!cb_queue.empty());
      const char *path = req_queue.front().first.c_str();
      // Log::info("flush %s con=%d, qsize=%d", path, connection_status, cb_queue.size());
      if (req_queue.front().second.length()) {
        int length = req_queue.front().second.length();
        sprintf(url, "POST %s HTTP/1.1\r\nContent-Type: multipart/form-data\r\nContent-Length: %d\r\n\r\n", path, length);
        write_string(url, strlen(url), &handle);
        write_string((char*) req_queue.front().second.data(), length, &handle);
        sprintf(url + 1000, "\r\n");
        write_string(url + 1000, strlen(url + 1000), &handle);
      } else {
        sprintf(url, "GET %s HTTP/1.1\r\n\r\n", path);
        write_string(url, strlen(url), &handle);
      }
      is_idle = false;
    } else {
      // Log::info("Waiting for result of previous call");
    }
  } else if (!cb_queue.empty()) {
    Log::warn("Failed flush, not connected, cb_queue size = %d", cb_queue.size());
  }
}

void ClientImpl::try_connect() {
  switch (connection_status) {
    case ClientState::CONNECTED: Log::severe("Already connected"); assert(0); break;
    case ClientState::CONNECTING: Log::severe("Try double connect"); assert(0); break;
    case ClientState::WAITING: /* Log::info("Try connect: state WAITING"); */ break;
    case ClientState::DISCONNECTED: Log::info("Try connect: state DISCONNECTED");
      uv_timer_stop(&connect_timer);
    case ClientState::UNINITED: Log::info("Try connect: state UNINITED");
      uv_timer_init(uv_default_loop(), &connect_timer);
      uv_timer_start(&connect_timer, try_connect_cb, timeout, 0);
      connection_status = ClientState::WAITING;
      break;
  }
}

void ClientImpl::close() {
  switch (connection_status) {
    case ClientState::CONNECTED:
      connection_status = ClientState::DISCONNECTED;
      the_parser.close();
      break;
    case ClientState::CONNECTING: Log::severe("CONNECTING"); assert(0); break;
    case ClientState::WAITING: Log::info("WAITING"); assert(0); break;
    case ClientState::DISCONNECTED: Log::info("Already DISCONNECTED"); the_parser.close(); break;
    case ClientState::UNINITED: Log::info("UNINITED"); assert(0); break;
  }
}

};
