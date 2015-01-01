#include <signal.h>
#include <unistd.h>

#include <cassert>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include "load.h"

using namespace std;

// If you know this, you can mess with all the data :)
const long long SECRET_CODE = 1234LL;

// Admins' Facebook ID (app scoped id)
const long long FHALIM = 123LL;
const long long AINUN = 124LL;
const long long KURKUR = 125LL;

// Application Singleton accessible globally.
static Server& app() {
  static Server s;
  return s;
}

static uv_work_t hash_computer_req;
static int hash_tps_index;
static vector<pair<int, int>> updated_scans;
static bool sync_images;

static int get_hash(char *fn) {
  static char buf[1 << 20];
  int hash = 0;
  FILE *img = fopen(fn, "rb");
  if (!img) return hash;

  // http://en.wikipedia.org/wiki/Jenkins_hash_function
  while (true) {
    int n = fread(buf, sizeof(char), 1 << 20, img);
    for (int j = 0; j < n; j++) {
      hash += buf[j];
      hash += (hash << 10);
      hash ^= (hash >> 6);
    }
    if (n != (1 << 20)) break;
  }
  hash += (hash << 3);
  hash ^= (hash >> 11);
  hash += (hash << 15);
  fclose(img);
  return hash;
}

static void compute_hash(uv_work_t *req) {
  static char fn[100];

  sleep(1);
  updated_scans.clear();
  for (int i = 0; i < 10000; i++) {
    Tps &t = tps[hash_tps_index];
    sprintf(fn, "/mnt/images/%07d%03d%02d.jpg", t.kel, t.tps_no, 4);
    // Log::info("file %s", fn);
    int hash = get_hash(fn);
    if (hash != t.scan_hash) updated_scans.push_back(make_pair(hash_tps_index, hash));
    hash_tps_index = (hash_tps_index + 1) % (TOTAL_TPS + 2);
  }
}

static void update_hash(uv_work_t* req, int status) {
  assert(!status);
  if (updated_scans.size()) {
    Log::info("Computing hash %d, updated = %lu", hash_tps_index, updated_scans.size());
  }
  for (int i = 0; i < (int) updated_scans.size(); i++) {
    // Log::info("%d = %d", updated_scans[i].first, updated_scans[i].second);
    Tps &t = tps[updated_scans[i].first];
    assert(t.scan_hash != updated_scans[i].second);
    if (!t.scan_hash) {
      for (int p = t.kel; p; p = parent_id[p]) {
        tree[p].cnt_available++;
      }
    }
    t.scan_hash = updated_scans[i].second;
  }
  if (sync_images) {
    uv_queue_work(uv_default_loop(), &hash_computer_req, compute_hash, update_hash);
  } else {
    Log::warn("Stopped syncing images");
  }
}

static bool advance_prefix(const char *&s, const char *prefix) {
  while (*prefix)
    if (*(s++) != *(prefix++))
      return false;
  return true;
}

static bool read_int(const char *&s, int &num) {
  if (!isdigit(*s)) return false;
  num = 0;
  while (isdigit(*s))
    num = num * 10 + (*(s++) - '0');
  return true;
}

static bool read_ll(const char *&s, long long &num) {
  if (!isdigit(*s)) return false;
  num = 0;
  while (isdigit(*s))
    num = num * 10 + (*(s++) - '0');
  return true;
}

static vector<int> read_ints(const char *&s) {
  vector<int> nums;
  for (int num; ; s++) {
    if (!read_int(s, num)) break;
    nums.push_back(num);
    if (*s != ',') break;
  }
  return nums;
}

static void ids2names_handler(Request& req, Response& res) {
  char* url = (char*) req.url.c_str();
  if (!advance_prefix((const char*&) url, "/api/ids2names/"))
    return res.send(Response::Code::NOT_FOUND);

  res.body() << "[";
  bool first = true;
  for (const char *p = strtok(url, ","); p; p = strtok(NULL, ",")) {
    if (first) first = false; else res.body() << ",";
    int id = atoi(p);
    if (parent_name.count(id)) {
      res.body() << "\"" << parent_name[id] << "\"";
    } else {
      res.body() << "\"Unknown\"";
    }
  }
  res.body() << "]";
  res.set_max_runtime_warning(2);
  res.send(Response::Code::OK);
}

static void children_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent;

  if (sscanf(url, "/api/children/%d", &parent) != 1 || !tree.count(parent))
    return res.send(Response::Code::NOT_FOUND);

  auto &v = tree[parent].children;
  res.body() << "[";
  bool first = true;
  for (auto child : v) {
    if (first) first = false; else res.body() << ",";
    auto &cn = tree[child];
    res.body() << "["
      << child << ","
      << "\""<< parent_name[child] << "\","
      << cn.cnt_prabowo << ","
      << cn.cnt_jokowi << ","
      << cn.cnt_sah << ","
      << cn.cnt_tidak_sah << ","
      << cn.cnt_de_errors << ","
      << cn.cnt_entered << ","
      << cn.cnt_available << ","
      << cn.cnt_tps << ","
      << cn.cnt_siluman << ","
      << cn.cnt_h3_errors << ","
      << cn.cnt_h4_errors << ","
      << cn.cnt_j1 << ","
      << cn.cnt_j2 << ","
      << cn.cnt_j3 << ","
      << cn.cnt_j4 << ","
      << cn.cnt_entered3 << ","
      << cn.cnt_de3_errors << ","
      << cn.cnt_g1 << ","
      << cn.cnt_g2 << ","
      << cn.cnt_g3 << ","
      << cn.cnt_g4 << ","
      << cn.last_modified << ","
      << cn.cnt_a1 << ","
      << cn.cnt_a2 << ","
      << cn.cnt_a3 << ","
      << cn.cnt_a4 << ","
      << cn.cnt_b1 << ","
      << cn.cnt_b2 << ","
      << cn.cnt_b3 << ","
      << cn.cnt_b4 << ","
      << cn.cnt_j5 << ","
      << cn.cnt_j6 << ","
      << cn.cnt_g6 << ","
      << cn.cnt_j7 << ","
      << cn.cnt_ii1 << ","
      << cn.cnt_ii2 << ","
      << cn.cnt_ii3 << ","
      << cn.cnt_ii4 << "]";
  }
  res.body() << "]";

  res.set_max_runtime_warning(2);
  // res.set_max_age(60, 1399804033); // Sun May 11 2014 03:27:08 GMT-0700 (PDT)
  res.send(Response::Code::OK);
}

static void tps_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent, token = 0;

  if (sscanf(url, "/api/tps/%d/%d", &parent, &token) == 2) {
    if (token != 872836527 || !tree.count(parent)) {
      return res.send(Response::Code::NOT_FOUND);
    }
  } else if (sscanf(url, "/api/tps/%d", &parent) == 1) {
     if (!tree.count(parent)) {
       return res.send(Response::Code::NOT_FOUND);
     }
  } else {
    return res.send(Response::Code::NOT_FOUND);
  }

  auto &v = tree[parent].tps_index;
  res.body() << "[";
  bool first = true;
  for (auto idx : v) {
    if (first) first = false; else res.body() << ",";
    Tps &t = tps[idx];
    Hal3 &h = hal3[idx];
    Crowd &c = crowd[idx];
    LastModified &m = modified[idx];
    string comment = "\"\"";
    if (token && c.comment_id) {
      int cid = (int) c.comment_id - 1;
      char *s = kesalahan + cid * KESALAHAN_CHAR_LENGTH;
      int len = min((int) strlen(s), KESALAHAN_CHAR_LENGTH);
      for (int i = 0; i < len; i++) if (s[i] == '\t') s[i] = ' ';
      comment = "\"" + string(s, len) + "\"";
    }
    res.body() << "["
      << t.tps_no << ","
      << t.max_records << ","
      << t.prabowo << ","
      << t.jokowi << ","
      << t.sah << ","
      << t.tidak_sah << ","
      << (t.scan_hash ? "true" : "false") << ","
      << "\"" << (token? c.fb_de_reported : (c.fb_de_reported ? 1 : 0)) << "\","
      << "\"" << (token? c.fb_enter : (c.fb_enter ? 1 : 0)) << "\","
      << comment << ","
      << h.a1 << ","
      << h.a2 << ","
      << h.a3 << ","
      << h.a4 << ","
      << h.b1 << ","
      << h.b2 << ","
      << h.b3 << ","
      << h.b4 << ","
      << h.ii1 << ","
      << h.ii2 << ","
      << h.ii3 << ","
      << h.ii4 << ","
      << "\"" << (token? h.fb_enter : (h.fb_enter ? 1 : 0)) << "\","
      << "\"" << (token? h.fb_de3_reported : (h.fb_de3_reported ? 1 : 0)) << "\","
      << "\"" << (token? h.fb_h3_reported : (h.fb_h3_reported ? 1 : 0)) << "\","
      << "\"" << (token? c.fb_h4_reported : (c.fb_h4_reported ? 1 : 0)) << "\","
      << m.hal3_enter << "]";
  }
  res.body() << "]";

  res.set_max_runtime_warning(2);
  // res.set_max_age(60, 1399804033); // Sun May 11 2014 03:27:08 GMT-0700 (PDT)
  res.send(Response::Code::OK);
}

static int get_tps_idx(int parent, int tps_no) {
  int idx = -1;
  auto &v = tree[parent].tps_index;
  for (int i : v) {
    Tps &t = tps[i];
    if (t.tps_no == tps_no) {
      if (idx != -1) {
        Log::severe("duplicate tps no for %d %d: %d %d", parent, tps_no, idx, i);
      }
      idx = i;
    }
  }
  return idx;
}

static void report_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent, tps_no;
  long long fbid, token;

  if (sscanf(url, "/api/report/%d/%d/%lld/%lld", &parent, &tps_no, &fbid, &token) != 4 || token != SECRET_CODE) {
    res.body() << "{\"error\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  if (!tree.count(parent)) {
    res.body() << "{\"error\": \"wrong parent\"}";
    return res.send(Response::Code::OK);
  }

  int idx = get_tps_idx(parent, tps_no);
  if (idx == -1) {
    res.body() << "{\"error\": \"wrong tps\"}";
    return res.send(Response::Code::OK);
  }
  Crowd &c = crowd[idx];
  int delta = 0;
  if (c.fb_de_reported) {
    if (c.fb_de_reported == fbid) {
      c.fb_de_reported = 0;
      res.body() << "{\"status\": \"fixed\"}";
      delta = -1;
    } else {
      res.body() << "{\"error\": \"not owner\"}";
    }
  } else {
    delta = 1;
    c.fb_de_reported = fbid;
    res.body() << "{\"status\": \"reported\"}";
  }
  while (parent) {
    tree[parent].cnt_de_errors += delta;
    assert(parent_id.count(parent));
    parent = parent_id[parent];
  }
  res.set_max_runtime_warning(2);
  res.send(Response::Code::OK);
}

static void report3_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent, tps_no;
  long long fbid, token;

  if (sscanf(url, "/api/report3/%d/%d/%lld/%lld", &parent, &tps_no, &fbid, &token) != 4 || token != SECRET_CODE) {
    res.body() << "{\"error\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  if (!tree.count(parent)) {
    res.body() << "{\"error\": \"wrong parent\"}";
    return res.send(Response::Code::OK);
  }

  int idx = get_tps_idx(parent, tps_no);
  if (idx == -1) {
    res.body() << "{\"error\": \"wrong tps\"}";
    return res.send(Response::Code::OK);
  }
  Hal3 &c = hal3[idx];
  int delta = 0;
  if (c.fb_de3_reported) {
    if (c.fb_de3_reported == fbid || whitelist_edit.count(fbid)) {
      c.fb_de3_reported = 0;
      res.body() << "{\"status\": \"fixed\"}";
      delta = -1;
    } else {
      res.body() << "{\"error\": \"not owner\"}";
    }
  } else {
    delta = 1;
    c.fb_de3_reported = fbid;
    res.body() << "{\"status\": \"reported\"}";
  }
  while (parent) {
    tree[parent].cnt_de3_errors += delta;
    assert(parent_id.count(parent));
    parent = parent_id[parent];
  }
  res.set_max_runtime_warning(2);
  res.send(Response::Code::OK);
}

static void report_h3_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent, tps_no;
  long long fbid, token;

  if (sscanf(url, "/api/report_h3/%d/%d/%lld/%lld", &parent, &tps_no, &fbid, &token) != 4 || token != SECRET_CODE) {
    res.body() << "{\"error\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  if (!tree.count(parent)) {
    res.body() << "{\"error\": \"wrong parent\"}";
    return res.send(Response::Code::OK);
  }

  int idx = get_tps_idx(parent, tps_no);
  if (idx == -1) {
    res.body() << "{\"error\": \"wrong tps\"}";
    return res.send(Response::Code::OK);
  }
  Hal3 &c = hal3[idx];
  int delta = 0;
  if (c.fb_h3_reported) {
    if (c.fb_h3_reported == fbid || whitelist_edit.count(fbid)) {
      c.fb_h3_reported = 0;
      res.body() << "{\"status\": \"fixed\"}";
      delta = -1;
    } else {
      res.body() << "{\"error\": \"not owner\"}";
    }
  } else {
    delta = 1;
    c.fb_h3_reported = fbid;
    res.body() << "{\"status\": \"reported\"}";
  }
  while (parent) {
    tree[parent].cnt_h3_errors += delta;
    assert(parent_id.count(parent));
    parent = parent_id[parent];
  }
  res.set_max_runtime_warning(2);
  res.send(Response::Code::OK);
}

static void report_h4_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent, tps_no;
  long long fbid, token;

  if (sscanf(url, "/api/report_h4/%d/%d/%lld/%lld", &parent, &tps_no, &fbid, &token) != 4 || token != SECRET_CODE) {
    res.body() << "{\"error\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  if (!tree.count(parent)) {
    res.body() << "{\"error\": \"wrong parent\"}";
    return res.send(Response::Code::OK);
  }

  int idx = get_tps_idx(parent, tps_no);
  if (idx == -1) {
    res.body() << "{\"error\": \"wrong tps\"}";
    return res.send(Response::Code::OK);
  }
  Crowd &c = crowd[idx];
  int delta = 0;
  if (c.fb_h4_reported) {
    if (c.fb_h4_reported == fbid || whitelist_edit.count(fbid)) {
      c.fb_h4_reported = 0;
      res.body() << "{\"status\": \"fixed\"}";
      delta = -1;
    } else {
      res.body() << "{\"error\": \"not owner\"}";
    }
  } else {
    delta = 1;
    c.fb_h4_reported = fbid;
    res.body() << "{\"status\": \"reported\"}";
  }
  while (parent) {
    tree[parent].cnt_h4_errors += delta;
    assert(parent_id.count(parent));
    parent = parent_id[parent];
  }
  res.set_max_runtime_warning(2);
  res.send(Response::Code::OK);
}

static void report_comment_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent, tps_no, cid;
  long long fbid, token;

  if (sscanf(url, "/api/report_comment/%d/%d/%d/%lld/%lld", &parent, &tps_no, &cid, &fbid, &token) != 5 || token != SECRET_CODE) {
    res.body() << "{\"error\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  if (!tree.count(parent)) {
    res.body() << "{\"error\": \"wrong parent\"}";
    return res.send(Response::Code::OK);
  }

  if (cid < 0 || cid + 1 >= KESALAHAN_SIZE) {
    res.body() << "{\"error\": \"full\"}";
    return res.send(Response::Code::OK);
  }

  int idx = get_tps_idx(parent, tps_no);
  if (idx == -1) {
    res.body() << "{\"error\": \"wrong tps\"}";
    return res.send(Response::Code::OK);
  }
  Crowd &c = crowd[idx];
  if (!c.fb_de_reported) {
    while (parent) {
      tree[parent].cnt_de_errors += 1;
      assert(parent_id.count(parent));
      parent = parent_id[parent];
    }
    c.fb_de_reported = fbid;
  }

  c.comment_id = cid + 1;
  char *s = kesalahan + cid * KESALAHAN_CHAR_LENGTH;
  strncpy(s, req.body.c_str(), KESALAHAN_CHAR_LENGTH);
  for (int i = 0; i < KESALAHAN_CHAR_LENGTH; i++) {
    if (s[i] == '\\' || s[i] == '"' || s[i] == '\'') s[i] = ' ';
  }
  Log::info("cmt[%d] = %s", cid, req.body.c_str());
  res.body() << "{\"status\": \"reported\"}";

  res.set_max_runtime_warning(2);
  res.send(Response::Code::OK);
}

static void update_verified_tps(int idx, int prabowo, int jokowi, int valid, int invalid, long long fbid, Response &res, int psts, int pprabowo, int pjokowi) {
  if (idx < 0 || idx >= (TOTAL_TPS + 2)) {
    res.body() << "{\"received\": \"wrong idx\"}";
    return res.send(Response::Code::OK);
  }

  if (prabowo < 0 || prabowo > 999) {
    res.body() << "{\"received\": \"wrong prabowo\"}";
    return res.send(Response::Code::OK);
  }

  if (jokowi < 0 || jokowi > 999) {
    res.body() << "{\"received\": \"wrong jokowi\"}";
    return res.send(Response::Code::OK);
  }

  if (valid < 0 || valid > 999) {
    res.body() << "{\"received\": \"wrong valid\"}";
    return res.send(Response::Code::OK);
  }

  if (invalid < 0 || invalid > 999) {
    res.body() << "{\"received\": \"wrong invalid\"}";
    return res.send(Response::Code::OK);
  }

  Crowd &c = crowd[idx];
  if (c.fb_enter) {
    res.body() << "{\"received\": \"exists\"}";
    return res.send(Response::Code::OK);
  }


  Hal3 &h = hal3[idx];
  int pb5 = h.b1 + h.b2 + h.b3 + h.b4;
  int pii4 = h.ii4;

  assert(!c.fb_enter);
  c.fb_enter = fbid;

  Tps &t = tps[idx];

  t.prabowo = prabowo;
  t.jokowi = jokowi;
  t.sah = valid;
  t.tidak_sah = invalid;

  int sts = t.sah + t.tidak_sah;

  int pj1 = (pb5 != pii4) || (pb5 != psts);
  int  j1 = (pb5 != pii4) || (pb5 !=  sts);
  int delta1 = j1 - pj1;

  int pg1 = max(max(pb5, pii4), psts) - min(min(pb5, pii4), psts);
  int  g1 = max(max(pb5, pii4),  sts) - min(min(pb5, pii4),  sts);
  int deltag1 = g1 - pg1;


  int pj2 = pii4 != psts;
  int  j2 = pii4 !=  sts;
  int delta2 = j2 - pj2;

  int pg2 = max(pii4, psts) - min(pii4, psts);
  int  g2 = max(pii4,  sts) - min(pii4,  sts);
  int deltag2 = g2 - pg2;


  int pj5 = (!pprabowo && pjokowi) ? 1 : 0;
  int j5 = (!prabowo && jokowi) ? 1 : 0;
  int delta5 = j5 - pj5;

  int pj7 = (!pjokowi && pprabowo) ? 1 : 0;
  int j7 = (!jokowi && prabowo) ? 1 : 0;
  int delta7 = j7 - pj7;


  int parent = t.kel;
  int siluman = t.siluman();

  LastModified &m = modified[idx];
  m.hal3_enter = (int) time(NULL);
  while (parent) {
    auto &tp = tree[parent];
    tp.cnt_j1 += delta1;
    tp.cnt_j2 += delta2;
    tp.cnt_j5 += delta5;
    tp.cnt_j7 += delta7;
    tp.cnt_g1 += deltag1;
    tp.cnt_g2 += deltag2;
    tp.cnt_entered++;
    tp.cnt_prabowo += prabowo;
    tp.cnt_jokowi += jokowi;
    tp.cnt_sah += valid;
    tp.cnt_tidak_sah += invalid;
    tp.cnt_siluman += siluman;
    tp.last_modified = m.hal3_enter;
    assert(parent_id.count(parent));
    parent = parent_id[parent];
  }

  res.body() << "{\"received\": \"ok\"}";
  res.set_max_runtime_warning(2);
  res.send(Response::Code::OK);
}

static void submit_tps_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int idx, prabowo, jokowi, valid, invalid;
  long long fbid, token;

  if (sscanf(url, "/api/submit_tps/%d/%d/%d/%d/%d/%lld/%lld",
    &idx, &prabowo, &jokowi, &valid, &invalid, &fbid, &token) != 7 || token != SECRET_CODE) {
    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  update_verified_tps(idx, prabowo, jokowi, valid, invalid, fbid, res, 0, 0, 0);
}

static void submit_tps_rel_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent, tps_no, prabowo, jokowi, valid, invalid;
  long long fbid, token;

  if (sscanf(url, "/api/submit_tps_rel/%d/%d/%d/%d/%d/%d/%lld/%lld",
    &parent, &tps_no, &prabowo, &jokowi, &valid, &invalid, &fbid, &token) != 8 || token != SECRET_CODE) {
    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  if (!tree.count(parent)) {
    res.body() << "{\"received\": \"wrong parent\"}";
    return res.send(Response::Code::OK);
  }

  int idx = get_tps_idx(parent, tps_no);
  if (idx == -1) {
    res.body() << "{\"received\": \"wrong tps no\"}";
    return res.send(Response::Code::OK);
  }

  update_verified_tps(idx, prabowo, jokowi, valid, invalid, fbid, res, 0, 0, 0);
}

static bool ir(int num) {
  return num >= 0 && num < 999;
}

static void update_verified_tps3(int idx, int a1, int a2, int a3, int a4, int b1, int b2, int b3, int b4,
  int ii1, int ii2, int ii3, int ii4, long long fbid, Response &res) {

  if (!(ir(a1) && ir(a2) && ir(a3) && ir(a4) && ir(b1) && ir(b2) && ir(b3) && ir(b4) && ir(ii1) && ir(ii2) && ir(ii3) && ir(ii4))) {
    res.body() << "{\"received\": \"wrong range\"}";
    return res.send(Response::Code::OK);
  }

  Hal3 &c = hal3[idx];
  if (c.fb_enter) {
    res.body() << "{\"received\": \"exists\"}";
    return res.send(Response::Code::OK);
  }

  int pa1 = c.a1, pa2 = c.a2, pa3 = c.a3, pa4 = c.a4;
  int pb1 = c.b1, pb2 = c.b2, pb3 = c.b3, pb4 = c.b4;
  int pb5 = c.b1 + c.b2 + c.b3 + c.b4;
  int pii1 = c.ii1;
  int pii2 = c.ii2;
  int pii3 = c.ii3;
  int pii4 = c.ii4;

  assert(!c.fb_enter);
  c.fb_enter = fbid;
  c.a1 = a1;
  c.a2 = a2;
  c.a3 = a3;
  c.a4 = a4;
  c.b1 = b1;
  c.b2 = b2;
  c.b3 = b3;
  c.b4 = b4;
  c.ii1 = ii1;
  c.ii2 = ii2;
  c.ii3 = ii3;
  c.ii4 = ii4;

  int b5 = b1 + b2 + b3 + b4;

  Tps &t = tps[idx];
  int sts = t.sah + t.tidak_sah;

  int pj1 = (pb5 != pii4) || (pb5 != sts);
  int j1 = (b5 != ii4) || (b5 != sts);
  int delta1 = j1 - pj1;

  int pg1 = max(max(pb5, pii4), sts) - min(min(pb5, pii4), sts);
  int  g1 = max(max(b5, ii4), sts) - min(min(b5, ii4),  sts);
  int deltag1 = g1 - pg1;


  int pj2 = pii4 != sts;
  int j2 = ii4 != sts;
  int delta2 = j2 - pj2;

  int pg2 = max(pii4, sts) - min(pii4, sts);
  int  g2 = max(ii4, sts) - min(ii4, sts);
  int deltag2 = g2 - pg2;


  int pj3 = pb2 > pa2;
  int j3 = b2 > a2;
  int delta3 = j3 - pj3;

  int pg3 = max(0, pb2 - pa2);
  int g3 = max(0, b2 - a2);
  int deltag3 = g3 - pg3;


  int pj4 = pb4 > pa4;
  int j4 = b4 > a4;
  int delta4 = j4 - pj4;

  int pg4 = max(0, pb4 - pa4);
  int g4 = max(0, b4 - a4);
  int deltag4 = g4 - pg4;


  int pj6 = (pii1 - pii2 - pii3) != pii4;
  int j6 = (ii1 - ii2 - ii3) != ii4;
  int delta6 = j6 - pj6;

  int pg6 = abs((pii1 - pii2 - pii3) - pii4);
  int g6 = abs((ii1 - ii2 - ii3) != ii4);
  int deltag6 = g6 - pg6;


  int parent = t.kel;
  LastModified &m = modified[idx];
  m.hal3_enter = (int) time(NULL);
  assert(parent);
  while (parent) {
    auto &tp = tree[parent];
    tp.cnt_j1 += delta1;
    tp.cnt_j2 += delta2;
    tp.cnt_j3 += delta3;
    tp.cnt_j4 += delta4;
    tp.cnt_j6 += delta6;

    tp.cnt_g1 += deltag1;
    tp.cnt_g2 += deltag2;
    tp.cnt_g3 += deltag3;
    tp.cnt_g4 += deltag4;
    tp.cnt_g6 += deltag6;

    tp.cnt_a1 += a1 - pa1;
    tp.cnt_a2 += a2 - pa2;
    tp.cnt_a3 += a3 - pa3;
    tp.cnt_a4 += a4 - pa4;

    tp.cnt_b1 += b1 - pb1;
    tp.cnt_b2 += b2 - pb2;
    tp.cnt_b3 += b3 - pb3;
    tp.cnt_b4 += b4 - pb4;

    tp.cnt_ii1 += ii1 - pii1;
    tp.cnt_ii2 += ii2 - pii2;
    tp.cnt_ii3 += ii3 - pii3;
    tp.cnt_ii4 += ii4 - pii4;

    tp.cnt_entered3++;
    tp.last_modified = m.hal3_enter;
    assert(parent_id.count(parent));
    parent = parent_id[parent];
  }

  res.body() << "{\"received\": \"ok\"}";
  res.set_max_runtime_warning(2);
  res.send(Response::Code::OK);
}

static void submit_tps3_rel_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent, tps_no, a1, a2, a3, a4, b1, b2, b3, b4, ii1, ii2, ii3, ii4;
  long long fbid, token;

  if (sscanf(url, "/api/submit_tps3_rel/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%lld/%lld",
    &parent, &tps_no, &a1, &a2, &a3, &a4, &b1, &b2, &b3, &b4, &ii1, &ii2, &ii3, &ii4, &fbid, &token) != 16 || token != SECRET_CODE) {
    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  if (!tree.count(parent)) {
    res.body() << "{\"received\": \"wrong parent\"}";
    return res.send(Response::Code::OK);
  }

  int idx = get_tps_idx(parent, tps_no);
  if (idx == -1) {
    res.body() << "{\"received\": \"wrong tps no\"}";
    return res.send(Response::Code::OK);
  }

  if (idx < 0 || idx >= (TOTAL_TPS + 2)) {
    res.body() << "{\"received\": \"wrong idx\"}";
    return res.send(Response::Code::OK);
  }

  update_verified_tps3(idx, a1, a2, a3, a4, b1, b2, b3, b4, ii1, ii2, ii3, ii4, fbid, res);
}

static void whitelist_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  long long whitelist_id, fbid, token;

  if (sscanf(url, "/api/whitelist/%lld/%lld/%lld", &whitelist_id, &fbid, &token) != 3
    || fbid != FHALIM || token != SECRET_CODE) {

    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  whitelist.insert(whitelist_id);

  res.body() << "{\"received\": \"ok\"}";
  return res.send(Response::Code::OK);
}

static void whitelist_edit_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  long long whitelist_id, fbid, token;

  if (sscanf(url, "/api/whitelist_edit/%lld/%lld/%lld", &whitelist_id, &fbid, &token) != 3
    || fbid != FHALIM || token != SECRET_CODE) {

    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  whitelist_edit.insert(whitelist_id);

  res.body() << "{\"received\": \"ok\"}";
  return res.send(Response::Code::OK);
}

static void in_whitelist_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  long long whitelist_id;

  if (sscanf(url, "/api/in_whitelist/%lld", &whitelist_id) != 1) {
    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  res.body() << "{\"received\": " << whitelist.count(whitelist_id) << "}";
  return res.send(Response::Code::OK);
}

static void fix_error_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent, tps_no, prabowo, jokowi, valid, invalid;
  long long fbid, token;

  if (sscanf(url, "/api/fix_error/%d/%d/%d/%d/%d/%d/%lld/%lld",
    &parent, &tps_no, &prabowo, &jokowi, &valid, &invalid, &fbid, &token) != 8 || token != SECRET_CODE) {
    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  if (!whitelist.count(fbid) || !whitelist_edit.count(fbid)) {
    res.body() << "{\"received\": \"no power\"}";
    return res.send(Response::Code::OK);
  }

  if (!tree.count(parent)) {
    res.body() << "{\"received\": \"wrong parent\"}";
    return res.send(Response::Code::OK);
  }

  int idx = get_tps_idx(parent, tps_no);
  if (idx == -1) {
    res.body() << "{\"received\": \"wrong tps no\"}";
    return res.send(Response::Code::OK);
  }

  Crowd &c = crowd[idx];
  if (!c.fb_enter) {
    res.body() << "{\"received\": \"not wrong\"}";
    return res.send(Response::Code::OK);
  }

  if (!c.fb_de_reported) {
    res.body() << "{\"received\": \"not reported\"}";
    return res.send(Response::Code::OK);
  }

  c.fb_enter = 0;
  c.fb_de_reported = 0;

  Tps &t = tps[idx];

  // Undo existing count.
  if (!(parent == t.kel || !t.kel)) {
    fprintf(stderr, "parent = %d, kel = %d\n", parent, t.kel);
  }
  assert(parent == t.kel || !t.kel);
  while (parent) {
    auto &tp = tree[parent];
    tp.cnt_de_errors--;
    tp.cnt_entered--;
    tp.cnt_prabowo -= t.prabowo;
    tp.cnt_jokowi -= t.jokowi;
    tp.cnt_sah -= t.sah;
    tp.cnt_tidak_sah -= t.tidak_sah;
    tp.cnt_siluman -= t.siluman();
    assert(parent_id.count(parent));
    parent = parent_id[parent];
  }

  int psts = t.sah + t.tidak_sah;
  int pprabowo = t.prabowo;
  int pjokowi = t.jokowi;

  t.prabowo = t.jokowi = t.sah = t.tidak_sah = 0;

  update_verified_tps(idx, prabowo, jokowi, valid, invalid, fbid, res, psts, pprabowo, pjokowi);
}

static void fix_error3_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();
  int parent, tps_no, a1, a2, a3, a4, b1, b2, b3, b4, ii1, ii2, ii3, ii4;
  long long fbid, token;

  if (sscanf(url, "/api/fix_error3/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%lld/%lld",
    &parent, &tps_no, &a1, &a2, &a3, &a4, &b1, &b2, &b3, &b4, &ii1, &ii2, &ii3, &ii4, &fbid, &token) != 16 || token != SECRET_CODE) {
    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  if (!whitelist.count(fbid) || !whitelist_edit.count(fbid)) {
    res.body() << "{\"received\": \"no power\"}";
    return res.send(Response::Code::OK);
  }

  if (!tree.count(parent)) {
    res.body() << "{\"received\": \"wrong parent\"}";
    return res.send(Response::Code::OK);
  }

  int idx = get_tps_idx(parent, tps_no);
  if (idx == -1) {
    res.body() << "{\"received\": \"wrong tps no\"}";
    return res.send(Response::Code::OK);
  }

  Hal3 &c = hal3[idx];
  if (!c.fb_enter) {
    res.body() << "{\"received\": \"not wrong\"}";
    return res.send(Response::Code::OK);
  }

  if (!c.fb_de3_reported) {
    res.body() << "{\"received\": \"not reported\"}";
    return res.send(Response::Code::OK);
  }

  c.fb_enter = 0;
  c.fb_de3_reported = 0;

  // Undo existing count.
  while (parent) {
    auto &tp = tree[parent];
    tp.cnt_de3_errors--;
    tp.cnt_entered3--;
    assert(parent_id.count(parent));
    parent = parent_id[parent];
  }

  update_verified_tps3(idx, a1, a2, a3, a4, b1, b2, b3, b4, ii1, ii2, ii3, ii4, fbid, res);
}

static void fix_tps_no_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();

  if (!advance_prefix(url, "/api/fix_tps_no/")) {
    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  int parent;
  if (!read_int(url, parent) || !tree.count(parent)) {
    res.body() << "{\"received\": \"wrong parent\"}";
    return res.send(Response::Code::OK);
  }

  if (!advance_prefix(url, "/")) {
    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  long long token;
  if (!read_ll(url, token) || token != SECRET_CODE) {
    res.body() << "{\"received\": \"who are you?\"}";
    return res.send(Response::Code::OK);
  }

  if (!advance_prefix(url, "/")) {
    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  vector<int> numbers = read_ints(url);
  auto &v = tree[parent].tps_index;
  if (numbers.size() != v.size()) {
    res.body() << "{\"received\": \"size mismatch " <<
      v.size() << " != " << numbers.size() << " \"}";
    return res.send(Response::Code::OK);
  }

  for (int i = 0; i < (int) v.size(); i++) {
    tps[v[i]].tps_no = numbers[i];
  }

  res.body() << "{\"received\": \"ok\"}";
  res.set_max_runtime_warning(2);
  res.send(Response::Code::OK);
}

static void sync_images_handler(Request& req, Response& res) {
  const char* url = req.url.c_str();

  if (!advance_prefix(url, "/api/sync_images/")) {
    res.body() << "{\"received\": 404}";
    return res.send(Response::Code::NOT_FOUND);
  }

  long long token;
  if (!read_ll(url, token) || token != SECRET_CODE) {
    res.body() << "{\"received\": \"who are you?\"}";
    return res.send(Response::Code::OK);
  }

  sync_images = false;
  Log::warn("Stopping syncing images...");

  res.body() << "{\"received\": \"ok\"}";
  res.set_max_runtime_warning(2);
  res.send(Response::Code::OK);
}

int main(int argc, char* argv[]) {
  signal(SIGINT, signal_callback_handler); // Register signal and signal handler

  load();

  // memset(modified, sizeof(LastModified) * (TOTAL_TPS + 2), 0);
  // memset(hal3, sizeof(Hal3) * TOTAL_TPS, 0);

  // int ttps = 0;
  // for (auto ts : tree) {
  //   if (ts.second.children.size()) {
  //     assert(!ts.second.tps_sizes.size());
  //   } else {
  //     for (int idx : ts.second.tps_index) {
  //       Tps &t = tps[idx];
  //       if (!t.scan_hash) {
  //         printf("%07d%03d%02d.jpg\n", ts.first, t.tps_no, 4);
  //       }
  //     }
  //     ttps += ts.second.tps_sizes.size();
  //   }
  // }
  // Log::info("sdfasdf  = %d", ttps);
  // assert(TOTAL_TPS == tree[0].cnt_tps);

  // Beware one is the prefix of another.
  app().get("/api/ids2names/", ids2names_handler);
  app().get("/api/children/", children_handler);
  app().get("/api/tps/", tps_handler);
  app().get("/api/report/", report_handler);
  app().get("/api/submit_tps/", submit_tps_handler);
  app().get("/api/submit_tps_rel/", submit_tps_rel_handler);
  app().get("/api/whitelist/", whitelist_handler);
  app().get("/api/whitelist_edit/", whitelist_edit_handler);
  app().get("/api/in_whitelist/", in_whitelist_handler);
  app().get("/api/fix_error/", fix_error_handler);
  app().get("/api/fix_tps_no/", fix_tps_no_handler);
  app().get("/api/sync_images/", sync_images_handler);
  app().get("/api/report_comment/", report_comment_handler);
  app().get("/api/report_h3/", report_h3_handler);
  app().get("/api/report_h4/", report_h4_handler);
  app().get("/api/submit_tps3_rel/", submit_tps3_rel_handler);
  app().get("/api/report3/", report3_handler);
  app().get("/api/fix_error3/", fix_error3_handler);

  if (argc != 2 || strcmp(argv[1], "nosync")) {
    Log::info("Start syncing /images");
    sync_images = true;
    uv_queue_work(uv_default_loop(), &hash_computer_req, compute_hash, update_hash);
  }

  whitelist.insert(FHALIM);
  whitelist.insert(AINUN);
  whitelist.insert(KURKUR);

  // Who can make edits?
  whitelist_edit.insert(FHALIM);
  whitelist_edit.insert(AINUN);
  whitelist_edit.insert(KURKUR);

  // Starts the server.
  app().listen("0.0.0.0", 8000);
}
