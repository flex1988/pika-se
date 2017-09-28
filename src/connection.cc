#include "connection.h"
#include "common.h"
#include "redis_command.h"
#include "server.h"

#include <muduo/base/Logging.h>

#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>

Connection::Connection(Server* owner, const muduo::net::TcpConnectionPtr& conn)
    : _owner(owner), _conn(conn), _reqtype(PROTO_NULL), _flags(0), _multibulklen(0), _bulklen(-1)
{
    _conn->setMessageCallback(boost::bind(&Connection::onMessage, this, _1, _2, _3));
}

Connection::~Connection() {}
void Connection::onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time)
{
    while (buf->readableBytes() > 0) {
        if (_flags & CONN_CLOSE_AFTER_REPLY) break;

        if (_reqtype == PROTO_NULL) {
            if (*buf->peek() == '*') {
                _reqtype = PROTO_MULTIBULK;
            }
            else {
                _reqtype = PROTO_INLINE;
            }
        }

        if (_reqtype == PROTO_INLINE) {
            if (!processInlineBuffer(buf)) break;
        }
        else if (_reqtype == PROTO_MULTIBULK) {
            if (!processMultibulkBuffer(buf)) break;
        }
        else {
            LOG_INFO << "server panic";
        }

        if (_argv.size() == 0) {
            reset();
        }
        else {
            if (processCommand()) {
                reset();
            }
        }
    }
}

bool Connection::processInlineBuffer(muduo::net::Buffer* buf)
{
    const char* newline;
    size_t querylen;

    newline = buf->findCRLF();

    if (newline == NULL) {
        if (buf->readableBytes() > PROTO_INLINE_MAX_SIZE) {
            sendReplyError("Protocol error: too big inline request");
            setProtocolError(buf, "too big mbulk count string", 0);
        }
        return false;
    }

    querylen = newline - buf->peek();

    std::string req(buf->peek(), querylen);

    splitQueryArgs(req);

    if (_argv.size() == 0) {
        sendReplyError("Protocol error: unbalanced quotes in request");
        setProtocolError(buf, "unbalanced quotes in inline request", 0);
        return false;
    }

    buf->retrieve(querylen + 2);

    return true;
}

bool Connection::processMultibulkBuffer(muduo::net::Buffer* buf)
{
    const char* newline = NULL;
    int pos = 0;
    long long ll;

    if (_multibulklen == 0) {
        newline = buf->findCRLF();
        if (newline == NULL) {
            if (buf->readableBytes() > PROTO_INLINE_MAX_SIZE) {
                sendReplyError("Protocol error: too big mbulk count string");
                setProtocolError(buf, "too big mbulk count string", 0);
            }
            return false;
        }

        assert(*buf->peek() == '*');
        ll = std::stoll(std::string(buf->peek() + 1, newline - (buf->peek() + 1)), NULL, 0);
        if (ll > 1024 * 1024) {
            sendReplyError("Protocol error: invalid multibulk length");
            setProtocolError(buf, "invalid mbulk count", pos);
            return false;
        }

        pos = (newline - buf->peek()) + 2;
        if (ll <= 0) {
            buf->retrieve(pos);
            return true;
        }

        _multibulklen = ll;
        buf->retrieve(pos);
    }

    while (_multibulklen) {
        if (_bulklen == -1) {
            newline = buf->findCRLF();
            if (newline == NULL) {
                if (buf->readableBytes() > PROTO_INLINE_MAX_SIZE) {
                    sendReplyError("Protocol error: too big mbulk count string");
                    setProtocolError(buf, "too big bulk count string", 0);
                    return false;
                }
                break;
            }

            if (*buf->peek() != '$') {
                sendReplyError("Protocol error: expected '$', got %c" + *buf->peek());
                setProtocolError(buf, "expected $ but got something else", 0);
                return false;
            }

            ll = std::stoll(std::string(buf->peek() + 1, newline - (buf->peek() + 1)), NULL, 0);

            if (ll < 0 || ll > 512 * 1024 * 1024) {
                sendReplyError("Protocol error: invalid bulk length");
                setProtocolError(buf, "invalid bulk length", 0);
                return false;
            }

            buf->retrieve(newline - buf->peek() + 2);

            _bulklen = ll;
        }

        if ((int)buf->readableBytes() < _bulklen + 2) {
            break;
        }
        else {
            std::string bulk = std::string(buf->peek(), _bulklen);
            _argv.push_back(bulk);
            buf->retrieve(_bulklen + 2);
            _bulklen = -1;
            _multibulklen--;
        }
    }

    if (_multibulklen == 0) return true;

    return false;
}

void Connection::reset()
{
    _argv.clear();
    _reqtype = PROTO_NULL;
    _multibulklen = 0;
    _bulklen = -1;
}

bool Connection::processCommand()
{
    if (_argv[0] == "quit") {
        sendReply("+OK\r\n");
        _flags |= CONN_CLOSE_AFTER_REPLY;
        return false;
    }

    boost::algorithm::to_lower(_argv[0]);

    RedisCommand* cmd = LookupCommand(_argv[0]);

    if (!cmd) {
        sendReplyError("unknown command: " + _argv[0]);
        return true;
    }

    if (cmd->argc > (int)_argv.size()) {
        sendReplyError("wrong number of arguments");
        return true;
    }

    if (_argv.size() > 1 && (_argv[1].size() >= KEY_MAX_LENGTH || _argv[1].size() <= 0)) {
        sendReplyError("invalid key length");
        return true;
    }

    cmd->proc(this);

    return true;
}

void Connection::sendReplyBulk(const std::string& value)
{
    std::string reply = str(boost::format("$%1%\r\n%2%\r\n") % value.size() % value.c_str());
    _conn->send(muduo::string(reply.c_str()));
}
void Connection::sendReply(const std::string& msg) { _conn->send(muduo::string(msg.c_str())); }
void Connection::sendReplyError(const std::string& msg)
{
    std::string ret = "-ERR " + msg + "\r\n";
    _conn->send(muduo::string(ret.c_str()));
}

void Connection::setProtocolError(muduo::net::Buffer* buf, muduo::string msg, int len)
{
    _flags |= CONN_CLOSE_AFTER_REPLY;
    buf->retrieve(len);
}

void Connection::sendReplyLongLong(int64_t val)
{
    std::string ret = ":" + std::to_string(val) + "\r\n";
    _conn->send(muduo::string(ret.c_str()));
}

bool Connection::splitQueryArgs(std::string req)
{
    std::istringstream iss(req);
    for (std::string s; iss >> s;) {
        _argv.push_back(s);
    }

    return true;
}
