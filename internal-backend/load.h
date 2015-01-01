/*
Loads data from file to memory.

Static files:
- tps_parent.in
- tps_indexes.in
- tps_indexes2.in
- lokasi.csv

Dynamic files (memorry-mapped):
- tps.bin
- hal3.bin
- crowd.bin
- kesalahan.bin
*/

#include <algorithm>
#include <set>
#include <vector>
#include <unordered_map>

#include "simple_http.h"
#include "fallocator.h"
#include "uv.h"

using namespace std;
using namespace simple_http;

// Memory-mapped struct. DO NOT CHANGE!
struct Tps {
  int prop;
  int kab;
  int kec;
  int kel;
  int tps_no;
  int max_records;    // DPT max records
  int prabowo;
  int jokowi;
  int sah;
  int tidak_sah;
  int scan_hash;      // Whether the image scan is ready.

  int siluman() {
    return max_records ? max(0, sah + tidak_sah - max_records) : 0;
  }
};

// Memory-mapped struct. DO NOT CHANGE!
struct Hal3 {
  int a1;
  int a2;
  int a3;
  int a4;
  int b1;
  int b2;
  int b3;
  int b4;
  int ii1;
  int ii2;
  int ii3;
  int ii4;
  long long fb_enter;         // Who did the entry for halaman 3.
  long long fb_de3_reported;  // Who reported halaman 3 for data entry error.
  long long fb_h3_reported;   // Who reported halaman 3 for scan error.
};

// Memory-mapped struct. DO NOT CHANGE!
struct Crowd {
  long long fb_de_reported;   // Who reported halaman 4 for data entry error.
  long long fb_enter;         // Who did the entry for halaman 4.
  long long fb_h4_reported;   // Who reported halaman 4 for scan error.
  long long comment_id;       // The offset for kesalahan.
};

struct LastModified {
  int hal3_enter;
  int hal3_scane;
  int hal3_entrye;
  int hal4_enter;
  int hal4_scane;
  int hal4_entrye;
};

const int TOTAL_TPS = 478828;
const int SLACK_TPS =  20000;
const int ADDITIONAL_TPS_MARKER = -389273981; // Any random number.
const int KESALAHAN_SIZE = 30000;
const int KESALAHAN_CHAR_LENGTH = 1024;

// Static files:
const char* TPS_PARENTS_FILE = "tps_parents.in";      // Id with names.
const char* TPS_HIERARCHY_FILE1 = "tps_indexes.in";   // Initial hierarcy.
const char* TPS_HIERARCHY_FILE2 = "tps_indexes2.in";  // Hierarchy fix 1.
const char* TPS_HIERARCHY_FILE3 = "lokasi.csv";       // Hierarchy fix 2.

// Memory-mapped files:
static Fallocator<Tps> tps_fallocator("tps.bin", true);
static Fallocator<Hal3> hal3_fallocator("hal3.bin", true);
static Fallocator<Crowd> crowd_fallocator("crowd.bin", true);
static Fallocator<char> kesalahan_fallocator("kesalahan.bin", true);
static Fallocator<LastModified> modified_fallocator("modified.bin", true);

static Tps *tps;
static Hal3 *hal3;
static Crowd *crowd;
static char* kesalahan;
static LastModified *modified;

// In-memory tree. Can be modified at will.
struct Node {
  // Either children or tps_sizes is populated.
  set<int> children;      // Contains the children ids.
  vector<int> tps_sizes;  // Contains DPT max records.
  vector<int> tps_index;  // Reference to tps array.

  // Aggregated values from its children.
  int cnt_prabowo;
  int cnt_jokowi;
  int cnt_sah;
  int cnt_tidak_sah;
  int cnt_de_errors;
  int cnt_de3_errors;
  int cnt_h3_errors;
  int cnt_h4_errors;
  int cnt_entered;
  int cnt_entered3;
  int cnt_j1;
  int cnt_j2;
  int cnt_j3;
  int cnt_j4;
  int cnt_g1;
  int cnt_g2;
  int cnt_g3;
  int cnt_g4;
  int cnt_available;
  int cnt_tps;
  int cnt_siluman;
  int last_modified;
  int cnt_a1;
  int cnt_a2;
  int cnt_a3;
  int cnt_a4;
  int cnt_b1;
  int cnt_b2;
  int cnt_b3;
  int cnt_b4;
  int cnt_j5;
  int cnt_j6;
  int cnt_j7;
  int cnt_g6;
  int cnt_ii1;
  int cnt_ii2;
  int cnt_ii3;
  int cnt_ii4;
};

static unordered_map<int, Node> tree;
static unordered_map<int, string> parent_name;
static unordered_map<int, int> parent_id;
static int init_tps_index;

static set<long long> whitelist;
static set<long long> whitelist_edit;

static void load_parents() {
  char s[10000];
  int parent;
  freopen(TPS_PARENTS_FILE, "r", stdin);
  while (gets(s)) {
    sscanf(s, "%d", &parent);
    gets(s);
    for (char *p = s; *p; p++)
      if (*p == '\\' || *p == '"') *p = '\'';
    parent_name[parent] = s;
  }
}

static void populate_tps_idx(int parent, int depth) {
  auto &tp = tree[parent];
  if (depth == 4) {
    for (int i = 0; i < (int) tp.tps_sizes.size(); i++) {
      tp.tps_index.push_back(init_tps_index++);
    }
    return;
  }
  for (auto &c : tree[parent].children) {
    populate_tps_idx(c, depth + 1);
  }
}

static void load_indexes() {
  char s[10000];
  freopen(TPS_HIERARCHY_FILE1, "r", stdin);
  int total_tps = 0;
  while (gets(s)) {
    vector<int> arr;
    for (char *p = strtok(s, " "); p; p = strtok(NULL, " "))
      arr.push_back(atoi(p));
    assert((int) arr.size() > 4);
    for (int i = 0, p = 0; i < 4; i++) {
      tree[p].children.insert(arr[i]);
      parent_id[arr[i]] = p;
      p = arr[i];
    }
    total_tps += arr[4];
    auto &c = tree[arr[3]];
    c.tps_sizes.clear();
    for (int i = 0; i < arr[4]; i++) {
      assert(5 + i < (int) arr.size());
      assert(arr[5 + i] != ADDITIONAL_TPS_MARKER);
      c.tps_sizes.push_back(arr[5 + i]);
    }
  }
  fprintf(stderr, "total_tps = %d\n", total_tps);
  // assert(total_tps == TOTAL_TPS);

  populate_tps_idx(0, 0);
}

static void populate_tps_idx2(int parent, int depth) {
  auto &tp = tree[parent];
  if (depth == 4) {
    for (int i = 0; i < (int) tp.tps_sizes.size(); i++) {
      if (tp.tps_sizes[i] == ADDITIONAL_TPS_MARKER) {
        tp.tps_sizes[i] = 1;
        int idx = init_tps_index++;
        tp.tps_index.push_back(idx);
      }
    }
    return;
  }
  for (auto &c : tree[parent].children) {
    populate_tps_idx2(c, depth + 1);
  }
}

static void load_indexes2() {
  char s[10000];
  freopen(TPS_HIERARCHY_FILE2, "r", stdin);
  int additional_tps = 0;
  for (int line = 1; gets(s); line++) {
    vector<int> arr;
    for (char *p = strtok(s, " "); p; p = strtok(NULL, " "))
      arr.push_back(atoi(p));
    if (arr.size() != 6) {
      // fprintf(stderr, "skipped line %d\n", line);
      continue;
    }
    for (int i = 0, p = 0; i < 4; i++) {
      bool inserted = tree[p].children.insert(arr[i]).second;
      assert(!inserted); // No new hierarchy, only new TPS.
      parent_id[arr[i]] = p;
      p = arr[i];
    }
    auto &c = tree[arr[3]];
    // fprintf(stderr, "%d %lu %d\n", arr[3], c.tps_sizes.size(), arr[4]);
    // assert(c.tps_sizes.size() <= arr[4]);
    for (int i = c.tps_sizes.size(); i < arr[4]; i++) {
      c.tps_sizes.push_back(ADDITIONAL_TPS_MARKER);
      additional_tps++;
    }
  }
  fprintf(stderr, "additional_tps = %d\n", additional_tps);

  populate_tps_idx2(0, 0);
  fprintf(stderr, "init_tps_index = %d\n", init_tps_index);
  // assert(init_tps_index == TOTAL_TPS);
}

static void load_indexes3() {
  char s[10000];
  freopen(TPS_HIERARCHY_FILE3, "r", stdin);
  int additional_tps = 0;
  gets(s);
  int ttps = 0;
  for (int line = 1; gets(s); line++) {
    // puts(s);
    int id, parent, level, nTps;
    int cnt = sscanf(s, "%d,%d,%d,", &id, &parent, &level);
    if (cnt != 3) puts(s);
    assert(id == 0 || (parent_id.count(id) && parent_id[id] == parent));
    assert(cnt == 3);
    int i = strlen(s) - 1;
    if (s[i] == 13) {
      s[i--] = '\0';
      assert(isdigit(s[i]));
    }
    while (isdigit(s[i])) i--;
    sscanf(s + i + 1, "%d", &nTps);
    if (level == 4) ttps += nTps;

    bool is_tps = level == 4;

    for (int j = id; parent_id.count(j); j = parent_id[j]) level--;
    assert(level == 0);
    assert(tree.count(id));

    auto &c = tree[id];
    if (c.children.size()) {
      assert(!is_tps);
      continue;
    }
    assert(is_tps);

    if ((int) c.tps_sizes.size() > nTps) {
      // fprintf(stderr, "MAKNYOS >> %d %lu %d\n", id, c.tps_sizes.size(), nTps);
    }
    for (i = c.tps_sizes.size(); i < nTps; i++) {
      c.tps_sizes.push_back(ADDITIONAL_TPS_MARKER);
      additional_tps++;
    }
    if ((int) c.tps_sizes.size() != nTps) {
      // fprintf(stderr, "AAAAAAAAAA %lu %d\n", (int) c.tps_sizes.size(), nTps);
    } else {
      assert((int) c.tps_sizes.size() == nTps);
    }
    // fprintf(stderr, "d\n");
  }
  fprintf(stderr, "additional_tps = %d, ttps = %d\n", additional_tps, ttps);

  // fprintf(stderr, "init_tps_index1 = %d\n", init_tps_index);
  populate_tps_idx2(0, 0);
  fprintf(stderr, "init_tps_index = %d\n", init_tps_index);
  // assert(init_tps_index == TOTAL_TPS);
}

// DO NOT RUN THIS! it will reset all data.
static void init_all(int parent, int depth) {
  // Log::info("parent = %d, depth = %d, init_tps_index = %d, %p", parent, depth, init_tps_index, tps);
  assert(init_tps_index < TOTAL_TPS);
  Tps &t = tps[init_tps_index];

  if (depth) switch (depth) {
    case 1: t.prop = parent; break;
    case 2: t.kab = parent; break;
    case 3: t.kec = parent; break;
    case 4: t.kel = parent; break;
  }

  if (depth == 4) {
    auto &tp = tree[parent];
    if (tp.tps_sizes.empty()) {
      return;
    }
    t.tps_no = 1;
    t.max_records = tp.tps_sizes[0];
    t.prabowo = 0;
    t.jokowi = 0;
    t.sah = 0;
    t.tidak_sah = 0;
    t.scan_hash = 0;
    tp.tps_index.push_back(init_tps_index);
    for (int i = 1; i < (int) tp.tps_sizes.size(); i++) {
      tps[init_tps_index + 1] = tps[init_tps_index];
      tps[++init_tps_index].tps_no++;
      tps[init_tps_index].max_records = tp.tps_sizes[i];
      tp.tps_index.push_back(init_tps_index);
    }
    init_tps_index++;
    return;
  }

  auto &n = tree[parent];
  for (auto &c : n.children) {
    init_all(c, depth + 1);
  }
}

void reset_all() {
  init_tps_index = 0;
  init_all(0, 0);
  memset(crowd, sizeof(Crowd) * TOTAL_TPS, 0);
}


static int agg_tps = 0;
static set<int> visited;
static bool compute_aggregate(int parent, int depth) {
  if (visited.count(parent)) {
    // fprintf(stderr, "already visited parent = %d\n", parent);
    assert(tree[parent].children.empty());
    return false;
  }
  visited.insert(parent);

  auto &tp = tree[parent];
  tp.cnt_prabowo = 0;
  tp.cnt_jokowi = 0;
  tp.cnt_sah = 0;
  tp.cnt_tidak_sah = 0;
  tp.cnt_de_errors = 0;
  tp.cnt_j1 = 0;
  tp.cnt_j2 = 0;
  tp.cnt_j3 = 0;
  tp.cnt_j4 = 0;
  tp.cnt_g1 = 0;
  tp.cnt_g2 = 0;
  tp.cnt_g3 = 0;
  tp.cnt_g4 = 0;
  tp.cnt_a1 = 0;
  tp.cnt_a2 = 0;
  tp.cnt_a3 = 0;
  tp.cnt_a4 = 0;
  tp.cnt_b1 = 0;
  tp.cnt_b2 = 0;
  tp.cnt_b3 = 0;
  tp.cnt_b4 = 0;
  tp.cnt_j5 = 0;
  tp.cnt_j6 = 0;
  tp.cnt_j7 = 0;
  tp.cnt_g6 = 0;
  tp.cnt_de3_errors = 0;
  tp.cnt_h3_errors = 0;
  tp.cnt_h4_errors = 0;
  tp.cnt_entered = 0;
  tp.cnt_entered3 = 0;
  tp.cnt_available = 0;
  tp.cnt_tps = 0;
  tp.cnt_siluman = 0;
  tp.last_modified = 0;
  tp.cnt_ii1 = 0;
  tp.cnt_ii2 = 0;
  tp.cnt_ii3 = 0;
  tp.cnt_ii4 = 0;

  if (depth == 4) {
    assert(tp.tps_index.size() == tp.tps_sizes.size());
    for (int idx : tp.tps_index) {
      Tps &x = tps[idx];
      if (!x.kel) x.kel = parent;
      tp.cnt_prabowo += x.prabowo;
      tp.cnt_jokowi += x.jokowi;
      tp.cnt_sah += x.sah;
      tp.cnt_tidak_sah += x.tidak_sah;
      tp.cnt_siluman += x.siluman();

      tp.last_modified = max(tp.last_modified, modified[idx].hal3_enter);

      Hal3 &y = hal3[idx];
      tp.cnt_de3_errors += y.fb_de3_reported ? 1 : 0;
      tp.cnt_h3_errors += y.fb_h3_reported ? 1 : 0;

      int b5 = y.b1 + y.b2 + y.b3 + y.b4;
      tp.cnt_j1 += (b5 != y.ii4) || (b5 != x.sah + x.tidak_sah);
      tp.cnt_j2 += (y.ii4 != x.sah + x.tidak_sah);
      tp.cnt_j3 += (y.b2 > y.a2);
      tp.cnt_j4 += (y.b4 > y.a4);
      tp.cnt_j5 += (!x.prabowo && x.jokowi) ? 1 : 0;
      tp.cnt_j6 += (y.ii1 - y.ii2 - y.ii3) != y.ii4;
      tp.cnt_j7 += (!x.jokowi && x.prabowo) ? 1 : 0;

      tp.cnt_g1 += max(max(b5, y.ii4), x.sah + x.tidak_sah) - min(min(b5, y.ii4), x.sah + x.tidak_sah);
      tp.cnt_g2 += max(y.ii4, x.sah + x.tidak_sah) - min(y.ii4, x.sah + x.tidak_sah);
      tp.cnt_g3 += max(0, y.b2 - y.a2);
      tp.cnt_g4 += max(0, y.b4 - y.a4);
      tp.cnt_g6 += abs((y.ii1 - y.ii2 - y.ii3) - y.ii4);

      tp.cnt_a1 += y.a1;
      tp.cnt_a2 += y.a2;
      tp.cnt_a3 += y.a3;
      tp.cnt_a4 += y.a4;

      tp.cnt_b1 += y.b1;
      tp.cnt_b2 += y.b2;
      tp.cnt_b3 += y.b3;
      tp.cnt_b4 += y.b4;

      tp.cnt_ii1 += y.ii1;
      tp.cnt_ii2 += y.ii2;
      tp.cnt_ii3 += y.ii3;
      tp.cnt_ii4 += y.ii4;

      Crowd &c = crowd[idx];
      tp.cnt_de_errors += c.fb_de_reported ? 1 : 0;
      tp.cnt_h4_errors += c.fb_h4_reported ? 1 : 0;
      tp.cnt_entered += c.fb_enter ? 1 : 0;
      tp.cnt_entered3 += y.fb_enter ? 1 : 0;
      tp.cnt_available += x.scan_hash ? 1 : 0;
    }
    agg_tps += tp.tps_index.size();
    tp.cnt_tps = tp.tps_index.size();
    return true;
  }

  for (auto &c : tree[parent].children) {
    if (compute_aggregate(c, depth + 1)) {
      auto &tc = tree[c];
      tp.cnt_prabowo += tc.cnt_prabowo;
      tp.cnt_jokowi += tc.cnt_jokowi;
      tp.cnt_sah += tc.cnt_sah;
      tp.cnt_tidak_sah += tc.cnt_tidak_sah;

      tp.cnt_j1 += tc.cnt_j1;
      tp.cnt_j2 += tc.cnt_j2;
      tp.cnt_j3 += tc.cnt_j3;
      tp.cnt_j4 += tc.cnt_j4;
      tp.cnt_j5 += tc.cnt_j5;
      tp.cnt_j6 += tc.cnt_j6;
      tp.cnt_j7 += tc.cnt_j7;

      tp.cnt_g1 += tc.cnt_g1;
      tp.cnt_g2 += tc.cnt_g2;
      tp.cnt_g3 += tc.cnt_g3;
      tp.cnt_g4 += tc.cnt_g4;
      tp.cnt_g6 += tc.cnt_g6;

      tp.cnt_a1 += tc.cnt_a1;
      tp.cnt_a2 += tc.cnt_a2;
      tp.cnt_a3 += tc.cnt_a3;
      tp.cnt_a4 += tc.cnt_a4;

      tp.cnt_b1 += tc.cnt_b1;
      tp.cnt_b2 += tc.cnt_b2;
      tp.cnt_b3 += tc.cnt_b3;
      tp.cnt_b4 += tc.cnt_b4;

      tp.cnt_ii1 += tc.cnt_ii1;
      tp.cnt_ii2 += tc.cnt_ii2;
      tp.cnt_ii3 += tc.cnt_ii3;
      tp.cnt_ii4 += tc.cnt_ii4;

      tp.cnt_de_errors += tc.cnt_de_errors;
      tp.cnt_de3_errors += tc.cnt_de3_errors;
      tp.cnt_h3_errors += tc.cnt_h3_errors;
      tp.cnt_h4_errors += tc.cnt_h4_errors;
      tp.cnt_entered += tc.cnt_entered;
      tp.cnt_entered3 += tc.cnt_entered3;
      tp.cnt_available += tc.cnt_available;
      tp.cnt_tps += tc.cnt_tps;
      tp.cnt_siluman += tc.cnt_siluman;
      tp.last_modified = max(tp.last_modified, tc.last_modified);
    }
  }
  return true;
}

static void signal_callback_handler(int signum) {
  Log::info("Exiting signum = %d",signum);
  tps_fallocator.deallocate(tps, TOTAL_TPS + SLACK_TPS);
  hal3_fallocator.deallocate(hal3, TOTAL_TPS + SLACK_TPS);
  crowd_fallocator.deallocate(crowd, TOTAL_TPS + SLACK_TPS);
  kesalahan_fallocator.deallocate(kesalahan, KESALAHAN_SIZE * KESALAHAN_CHAR_LENGTH);
  modified_fallocator.deallocate(modified, TOTAL_TPS + SLACK_TPS);
  Log::info("It's safe to crash");
  exit(signum);
}

static void load() {
  tps = tps_fallocator.allocate(TOTAL_TPS + SLACK_TPS);
  hal3 = hal3_fallocator.allocate(TOTAL_TPS + SLACK_TPS);
  crowd = crowd_fallocator.allocate(TOTAL_TPS + SLACK_TPS);
  kesalahan = kesalahan_fallocator.allocate(KESALAHAN_SIZE * KESALAHAN_CHAR_LENGTH);
  modified = modified_fallocator.allocate(TOTAL_TPS + SLACK_TPS);

  load_parents();
  load_indexes();
  load_indexes2();
  load_indexes3();

  // Two kelurahan has one too many TPS.
  assert(tree.count(76777));
  tree[76777].tps_sizes.pop_back();
  tree[76777].tps_index.pop_back();
  assert(tree.count(83222));
  tree[83222].tps_sizes.pop_back();
  tree[83222].tps_index.pop_back();

  // Only execute this in the beginning.
  // reset_all();

  // Compute in-memory tree aggregation counts.
  agg_tps = 0;
  compute_aggregate(0, 0);

  // Who can do data entry?
  whitelist.insert(123LL);

  fprintf(stderr, "init_tps_index = %d %d, %d, agg_tps = %d\n", init_tps_index, TOTAL_TPS, tree[0].cnt_tps, agg_tps);
}
