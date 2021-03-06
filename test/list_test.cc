#include "aof.h"
#include "db.h"
#include "rocksdb/db.h"
#include "rocksdb/options.h"
#include "gtest/gtest.h"

using namespace blink;

class RedisDBTest : public testing::Test {
  protected:
    virtual void SetUp() { db_ = std::shared_ptr<RedisDB>(new RedisDB("/tmp/db")); }
    std::shared_ptr<RedisDB> db_;
};

TEST_F(RedisDBTest, LPUSH)
{
    rocksdb::Status s;
    int64_t llen;
    std::string val;

    for (int i = 0; i < 200000; i++) {
        s = db_->LPush("mylist", std::to_string(i), &llen);
        EXPECT_EQ(true, s.ok());
        EXPECT_EQ(llen, i + 1);
    }
}

TEST_F(RedisDBTest, LLEN)
{
    rocksdb::Status s;
    int64_t llen;
    int64_t rlen;

    for (int i = 0; i < 200000; i++) {
        s = db_->LPush("llen", std::to_string(i), &llen);
        EXPECT_EQ(true, s.ok());
        EXPECT_EQ(llen, i + 1);

        s = db_->LLen("llen", &rlen);
        EXPECT_EQ(true, s.ok());
        EXPECT_EQ(llen, rlen);
    }
}

TEST_F(RedisDBTest, LINDEX)
{
    rocksdb::Status s;
    int64_t llen;
    std::string val;

    for (int i = 0; i < 200000; i++) {
        s = db_->LPush("lindex", std::to_string(i), &llen);
        EXPECT_EQ(true, s.ok());
        EXPECT_EQ(llen, i + 1);
    }

    for (int i = 0; i < 200000; i++) {
        s = db_->LIndex("lindex", i, &val);
        ASSERT_TRUE(s.ok());
        EXPECT_EQ(std::to_string(199999 - i), val);
    }
}

TEST_F(RedisDBTest, LPOP)
{
    rocksdb::Status s;
    int64_t llen;
    std::string val;

    for (int i = 0; i < 200000; i++) {
        s = db_->LPush("mylist", std::to_string(i), &llen);
        EXPECT_EQ(true, s.ok());
        EXPECT_EQ(llen, 1);

        s = db_->LPop("mylist", val);
        ASSERT_TRUE(s.ok());
        EXPECT_EQ(std::to_string(i), val);
    }
}

TEST_F(RedisDBTest, LRANGE)
{
    rocksdb::Status s;
    std::vector<std::string> values;
    int64_t llen;

    for (int i = 0; i < 200000; i++) {
        s = db_->LPush("lrange", std::to_string(i), &llen);
        EXPECT_EQ(true, s.ok());
    }

    s = db_->LRange("lrange", 0, 190000, values);
    ASSERT_TRUE(s.ok());

    for (int i = 0; i <= 190000; i++) {
        EXPECT_EQ(values.at(i), std::to_string(199999 - i));
    }
}

int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
