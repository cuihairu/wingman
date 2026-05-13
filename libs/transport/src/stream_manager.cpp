#include "wingman/transport/stream_manager.hpp"
#include <sstream>

namespace wingman::transport {

StreamManager::StreamManager() = default;

StreamManager::~StreamManager() {
    clear();
}

std::shared_ptr<StreamChannel> StreamManager::createStream(
    StreamType type,
    const std::string& name,
    const StreamParams& params) {

    std::lock_guard<std::mutex> lock(mutex_);

    // 使用提供的名称或自动生成
    std::string streamName = name.empty() ? generateStreamName(type) : name;

    // 如果已存在同名流，先移除
    if (streams_.find(streamName) != streams_.end()) {
        streams_.erase(streamName);
    }

    // 使用默认参数如果未提供
    StreamParams actualParams = params;
    if (actualParams.type != type) {
        actualParams = StreamParams::getDefault(type);
    }

    auto stream = std::shared_ptr<StreamChannel>(new StreamChannel(type, actualParams));
    streams_[streamName] = stream;

    return stream;
}

std::shared_ptr<StreamChannel> StreamManager::getStream(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = streams_.find(name);
    if (it != streams_.end()) {
        return it->second;
    }
    return nullptr;
}

void StreamManager::removeStream(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = streams_.find(name);
    if (it != streams_.end()) {
        it->second->disconnect();
        streams_.erase(it);
    }
}

std::vector<std::shared_ptr<StreamChannel>> StreamManager::getStreamsByType(
    StreamType type) const {

    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::shared_ptr<StreamChannel>> result;
    for (const auto& [name, stream] : streams_) {
        if (stream->getType() == type) {
            result.push_back(stream);
        }
    }
    return result;
}

void StreamManager::disconnectAll() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto& [name, stream] : streams_) {
        stream->disconnect();
    }
}

size_t StreamManager::getStreamCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return streams_.size();
}

bool StreamManager::hasStream(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return streams_.find(name) != streams_.end();
}

std::vector<std::string> StreamManager::getStreamNames() const {
    std::lock_guard<std::mutex> lock(mutex_);

    std::vector<std::string> names;
    names.reserve(streams_.size());
    for (const auto& [name, stream] : streams_) {
        names.push_back(name);
    }
    return names;
}

void StreamManager::clear() {
    std::lock_guard<std::mutex> lock(mutex_);

    streams_.clear();
}

std::string StreamManager::generateStreamName(StreamType type) {
    std::ostringstream oss;
    oss << streamTypeName(type) << "_" << nextId_++;
    return oss.str();
}

} // namespace wingman::transport
