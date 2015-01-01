#include <stdio.h>

#include "simple_http.h"
#include "uv.h"

using namespace std;
using namespace simple_http;

int main(int argc, char *argv[]) {
  Client c { "127.0.0.1", 8000 };

  if (argc != 2) {
    fprintf(stderr, "Usage: ./test_add [url]\n");
    return 0;
  }

  c.request(argv[1], "", [&c] (const string &res) {
    printf("result = %s\n", res.c_str());
    c.close();
  });

  uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}
