#include "db.h"
#include "mutex.h"

namespace blink {

rocksdb::Status RedisDB::Set(const std::string& key, const std::string& val)
{
    return list_->Put(rocksdb::WriteOptions(), key, val);
}

rocksdb::Status RedisDB::Get(const std::string& key, std::string& val)
{
    return list_->Get(rocksdb::ReadOptions(), key, &val);
}

}
