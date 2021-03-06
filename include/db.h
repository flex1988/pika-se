#ifndef __REDISDB_H__
#define __REDISDB_H__

#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>

#include "concurrent_queue.h"
#include "meta.h"
#include "mutex.h"
#include "port.h"

#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "rocksdb/slice.h"
#include "rocksdb/utilities/db_ttl.h"

namespace blink {
class SetMeta;

class RedisDB {
  public:
    RedisDB(const std::string& path);
    ~RedisDB();

    void Close();

    enum CommitMode { SYNC, ASYNC };
    // Status Del(const std::string &key, int64_t *count);
    // Status Set(const std::string &key, const std::string &val,
    // const int32_t ttl = 0);
    rocksdb::Status Get(const std::string& key, std::string& val);
    rocksdb::Status Set(const std::string& key, const std::string& val);

    // LIST
    rocksdb::Status LPush(const std::string& key, const std::string& val, int64_t* llen);
    rocksdb::Status LPushX(const std::string& key, const std::string& val, int64_t* llen);
    rocksdb::Status LPop(const std::string& key, std::string& val);
    rocksdb::Status LIndex(const std::string& key, const int64_t index, std::string* val);
    rocksdb::Status LLen(const std::string& key, int64_t* llen);
    rocksdb::Status LRange(const std::string& key, int start, int end, std::vector<std::string>& range);
    rocksdb::Status LSet(const std::string& key, int index, const std::string& val);
    rocksdb::Status LRem(const std::string& key, int count, const std::string& val, int64_t* remove);

    // SET
    rocksdb::Status SAdd(const std::string& key, const std::string& member, int64_t* res);
    rocksdb::Status SCard(const std::string& key, int64_t* res);

    void AppendMeta();
    void CompactMeta();
    void LoadMeta();

    void ReloadAof() { reload_aof_ = true; };
    void ReloadAofDone() { reload_aof_ = false; };
    bool IsReloadingAof() { return reload_aof_; };
    void ClearMemMeta() { memmeta_.clear(); };
    void LoadMetaSnapshot();
    void LoadMetaAppendonly();

    void DisableCompact() { forbid_compact_ = true; };
    void EnableCompact() { forbid_compact_ = false; };
    Queue<std::string> metaqueue_;

    std::unordered_map<std::string, std::shared_ptr<MetaBase>> GetMemMeta() { return memmeta_; };

  private:
    std::string path_;
    std::unique_ptr<rocksdb::DBWithTTL> kv_;
    std::unique_ptr<rocksdb::DBWithTTL> list_;
    std::unique_ptr<rocksdb::DBWithTTL> set_;

    std::shared_ptr<ListMetaOperator> lop_;

    rocksdb::Options options_;

    std::unordered_map<std::string, std::shared_ptr<MetaBase>> memmeta_;

    port::RecordMutex mutex_list_record_;
    port::RecordMutex mutex_set_record_;

    void ReloadListActionBuffer(char* buf, size_t blen);

    void ReloadAppendOnlyCommand(const std::vector<std::string>& argv);

    int aof_fd_;

    uint64_t meta_log_size_;

    std::mutex snap_mutex_;
    std::string msnap_;
    std::string mnewsnap_;
    std::string aof_;

    bool reload_aof_;
    bool forbid_compact_;
};
}
#endif
