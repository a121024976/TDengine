
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include "index.h"
#include "indexInt.h"
#include "index_cache.h"
#include "index_fst.h"
#include "index_fst_counting_writer.h"
#include "index_fst_util.h"
#include "index_tfile.h"
#include "tskiplist.h"
#include "tutil.h"

void* callback(void* s) { return s; }

static std::string fileName = "/tmp/tindex.tindex";
class FstWriter {
 public:
  FstWriter() {
    remove(fileName.c_str());
    _wc = writerCtxCreate(TFile, fileName.c_str(), false, 64 * 1024 * 1024);
    _b = fstBuilderCreate(_wc, 0);
  }
  bool Put(const std::string& key, uint64_t val) {
    FstSlice skey = fstSliceCreate((uint8_t*)key.c_str(), key.size());
    bool     ok = fstBuilderInsert(_b, skey, val);
    fstSliceDestroy(&skey);
    return ok;
  }
  ~FstWriter() {
    fstBuilderFinish(_b);
    fstBuilderDestroy(_b);

    writerCtxDestroy(_wc, false);
  }

 private:
  FstBuilder* _b;
  WriterCtx*  _wc;
};

class FstReadMemory {
 public:
  FstReadMemory(size_t size, const std::string& fileName = fileName) {
    tfInit();
    _wc = writerCtxCreate(TFile, fileName.c_str(), true, 64 * 1024);
    _w = fstCountingWriterCreate(_wc);
    _size = size;
    memset((void*)&_s, 0, sizeof(_s));
  }
  bool init() {
    char* buf = (char*)calloc(1, sizeof(char) * _size);
    int   nRead = fstCountingWriterRead(_w, (uint8_t*)buf, _size);
    if (nRead <= 0) { return false; }
    _size = nRead;
    _s = fstSliceCreate((uint8_t*)buf, _size);
    _fst = fstCreate(&_s);
    free(buf);
    return _fst != NULL;
  }
  bool Get(const std::string& key, uint64_t* val) {
    FstSlice skey = fstSliceCreate((uint8_t*)key.c_str(), key.size());
    bool     ok = fstGet(_fst, &skey, val);
    fstSliceDestroy(&skey);
    return ok;
  }
  bool GetWithTimeCostUs(const std::string& key, uint64_t* val, uint64_t* elapse) {
    int64_t s = taosGetTimestampUs();
    bool    ok = this->Get(key, val);
    int64_t e = taosGetTimestampUs();
    *elapse = e - s;
    return ok;
  }
  // add later
  bool Search(AutomationCtx* ctx, std::vector<uint64_t>& result) {
    FstStreamBuilder*      sb = fstSearch(_fst, ctx);
    StreamWithState*       st = streamBuilderIntoStream(sb);
    StreamWithStateResult* rt = NULL;
    while ((rt = streamWithStateNextWith(st, NULL)) != NULL) {
      // result.push_back((uint64_t)(rt->out.out));
      FstSlice*   s = &rt->data;
      int32_t     sz = 0;
      char*       ch = (char*)fstSliceData(s, &sz);
      std::string key(ch, sz);
      printf("key: %s, val: %" PRIu64 "\n", key.c_str(), (uint64_t)(rt->out.out));
      swsResultDestroy(rt);
    }
    for (size_t i = 0; i < result.size(); i++) {}
    std::cout << std::endl;
    return true;
  }
  bool SearchWithTimeCostUs(AutomationCtx* ctx, std::vector<uint64_t>& result) {
    int64_t s = taosGetTimestampUs();
    bool    ok = this->Search(ctx, result);
    int64_t e = taosGetTimestampUs();
    return ok;
  }

  ~FstReadMemory() {
    fstCountingWriterDestroy(_w);
    fstDestroy(_fst);
    fstSliceDestroy(&_s);
    writerCtxDestroy(_wc, false);
    tfCleanup();
  }

 private:
  FstCountingWriter* _w;
  Fst*               _fst;
  FstSlice           _s;
  WriterCtx*         _wc;
  size_t             _size;
};

#define L 100
#define M 100
#define N 100

int Performance_fstWriteRecords(FstWriter* b) {
  std::string str("aa");
  for (int i = 0; i < L; i++) {
    str[0] = 'a' + i;
    str.resize(2);
    for (int j = 0; j < M; j++) {
      str[1] = 'a' + j;
      str.resize(2);
      for (int k = 0; k < N; k++) {
        str.push_back('a');
        b->Put(str, k);
        printf("(%d, %d, %d, %s)\n", i, j, k, str.c_str());
      }
    }
  }
  return L * M * N;
}
void checkFstCheckIterator() {
  tfInit();
  FstWriter* fw = new FstWriter;
  int64_t    s = taosGetTimestampUs();
  int        count = 2;
  Performance_fstWriteRecords(fw);
  int64_t e = taosGetTimestampUs();

  std::cout << "insert data count :  " << count << "elapas time: " << e - s << std::endl;
  delete fw;

  FstReadMemory* m = new FstReadMemory(1024 * 64);
  if (m->init() == false) {
    std::cout << "init readMemory failed" << std::endl;
    delete m;
    return;
  }

  // prefix search
  std::vector<uint64_t> result;

  AutomationCtx* ctx = automCtxCreate((void*)"ab", AUTOMATION_ALWAYS);
  m->Search(ctx, result);
  std::cout << "size: " << result.size() << std::endl;
  // assert(result.size() == count);
  for (int i = 0; i < result.size(); i++) {
    // assert(result[i] == i);  // check result
  }

  free(ctx);
  delete m;
  tfCleanup();
}

void fst_get(Fst* fst) {
  for (int i = 0; i < 10000; i++) {
    std::string term = "Hello";
    FstSlice    key = fstSliceCreate((uint8_t*)term.c_str(), term.size());
    uint64_t    offset = 0;
    bool        ret = fstGet(fst, &key, &offset);
    if (ret == false) {
      std::cout << "not found" << std::endl;
    } else {
      std::cout << "found value:" << offset << std::endl;
    }
  }
}

#define NUM_OF_THREAD 10
void validateTFile(char* arg) {
  tfInit();

  std::thread threads[NUM_OF_THREAD];
  // std::vector<std::thread> threads;
  TFileReader* reader = tfileReaderOpen(arg, 0, 295868, "tag1");

  for (int i = 0; i < NUM_OF_THREAD; i++) {
    threads[i] = std::thread(fst_get, reader->fst);
    // threads.push_back(fst_get, reader->fst);
    // std::thread t(fst_get, reader->fst);
  }
  for (int i = 0; i < NUM_OF_THREAD; i++) {
    // wait join
    threads[i].join();
  }
  tfCleanup();
}
int main(int argc, char* argv[]) {
  if (argc > 1) { validateTFile(argv[1]); }
  // checkFstCheckIterator();
  // checkFstPrefixSearch();

  return 1;
}