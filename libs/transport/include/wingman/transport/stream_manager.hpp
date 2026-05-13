#pragma once

#include "wingman/transport/stream_type.hpp"
#include "wingman/transport/stream_channel.hpp"
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <mutex>

namespace wingman::transport {

/**
 * @brief 流通道管理器
 *
 * 负责创建、销毁和查找流通道。
 */
class StreamManager {
public:
    StreamManager();
    ~StreamManager();

    // 禁止拷贝
    StreamManager(const StreamManager&) = delete;
    StreamManager& operator=(const StreamManager&) = delete;

    /**
     * @brief 创建流通道
     * @param type 流类型
     * @param name 流名称
     * @param params 流参数（使用默认值如果为空）
     * @return 流通道指针
     */
    std::shared_ptr<StreamChannel> createStream(
        StreamType type,
        const std::string& name,
        const StreamParams& params = StreamParams()
    );

    /**
     * @brief 获取流通道
     * @param name 流名称
     * @return 流通道指针，不存在返回 nullptr
     */
    std::shared_ptr<StreamChannel> getStream(const std::string& name) const;

    /**
     * @brief 移除流通道
     * @param name 流名称
     */
    void removeStream(const std::string& name);

    /**
     * @brief 获取指定类型的所有流
     * @param type 流类型
     * @return 流通道列表
     */
    std::vector<std::shared_ptr<StreamChannel>> getStreamsByType(StreamType type) const;

    /**
     * @brief 断开所有流
     */
    void disconnectAll();

    /**
     * @brief 获取流数量
     * @return 流的总数
     */
    size_t getStreamCount() const;

    /**
     * @brief 检查流是否存在
     * @param name 流名称
     * @return 存在返回 true
     */
    bool hasStream(const std::string& name) const;

    /**
     * @brief 获取所有流名称
     * @return 流名称列表
     */
    std::vector<std::string> getStreamNames() const;

    /**
     * @brief 清空所有流
     */
    void clear();

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<StreamChannel>> streams_;

    /**
     * @brief 生成流名称
     * @param type 流类型
     * @return 自动生成的名称
     */
    std::string generateStreamName(StreamType type);

    uint32_t nextId_ = 1;
};

} // namespace wingman::transport
