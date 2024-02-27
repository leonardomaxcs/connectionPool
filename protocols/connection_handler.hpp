#pragma once

#include <memory>
#include <unordered_map>
#include <shared_mutex>
#include <atomic>
#include <optional>
#include <functional>
#include <libssh/libssh.h>
#include <libssh/sftp.h>
#include <stdexcept>
#include <string>
#include <mutex>

class Connection {
public:
    virtual void connect() = 0;
    virtual void disconnect() = 0;
    virtual void authenticate(const std::string& password) = 0;
    virtual std::string getProtocolName() const = 0;
    virtual ~Connection() {}
};