// 检测后处理纯函数（decodeYoloOutput/nmsBoxes）：零 onnxruntime 依赖，
// ML 档（ml.cpp）与替身档（ml_stub.cpp）共编同一份实现——两档仅
// ModelEngine/推理路径分文件，解码与 NMS 是纯 CPU 数学，共编保证
// 单源且两个构建档的测试都真实跑这段逻辑。

#include "wingman/ml.hpp"

#include <algorithm>

namespace wingman {

namespace {

// float32 张量数据首元素指针（解码只支持 float32 输出）
const float* tensorFloatData(const TensorData& t) {
    return reinterpret_cast<const float*>(t.data.data());
}

float iouOf(const ModelHelpers::Detection& a, const ModelHelpers::Detection& b) {
    const float ax2 = a.x + a.width;
    const float ay2 = a.y + a.height;
    const float bx2 = b.x + b.width;
    const float by2 = b.y + b.height;
    const float ix = std::max(0.0f, std::min(ax2, bx2) - std::max(a.x, b.x));
    const float iy = std::max(0.0f, std::min(ay2, by2) - std::max(a.y, b.y));
    const float inter = ix * iy;
    const float uni = a.width * a.height + b.width * b.height - inter;
    return uni > 0.0f ? inter / uni : 0.0f;
}

} // namespace

std::vector<ModelHelpers::Detection> ModelHelpers::decodeYoloOutput(
    const TensorData& output, float confThreshold) {
    std::vector<Detection> out;
    if (output.dataType != TensorDataType::FLOAT32 || output.shape.size() != 3
        || output.shape[0] != 1) {
        return out;
    }
    const auto& s = output.shape;
    const float* d = tensorFloatData(output);
    // v5 [1,N,5+C]（N 在中间维、末维小）与 v8 [1,4+C,N]（末维 N 大）按
    // 形状关系判别：N 维显著大于类别维。两侧维数相等属不可判歧义，返回空。
    const bool v5Layout = s[2] >= 6 && s[1] > s[2];
    const bool v8Layout = s[1] >= 5 && s[2] > s[1];
    if (v5Layout) {
        // YOLOv5 布局 [1, N, 5+C]：cx,cy,w,h,obj,classes...（conf=obj×cls）
        const int64_t n = s[1];
        const int64_t stride = s[2];
        const int numClasses = static_cast<int>(stride - 5);
        for (int64_t i = 0; i < n; ++i) {
            const float* row = d + i * stride;
            const float obj = row[4];
            if (obj <= confThreshold) continue;
            int bestCls = -1;
            float bestScore = 0.0f;
            for (int c = 0; c < numClasses; ++c) {
                if (row[5 + c] > bestScore) {
                    bestScore = row[5 + c];
                    bestCls = c;
                }
            }
            if (bestCls < 0) continue;
            const float conf = obj * bestScore;
            if (conf < confThreshold) continue;
            const float w = row[2];
            const float h = row[3];
            Detection det;
            det.x = std::max(0.0f, row[0] - w / 2.0f);
            det.y = std::max(0.0f, row[1] - h / 2.0f);
            det.width = w;
            det.height = h;
            det.classId = bestCls;
            det.confidence = conf;
            out.push_back(det);
        }
    } else if (v8Layout) {
        // YOLOv8 布局 [1, 4+C, N]：cx,cy,w,h 打头，class score 无 obj
        //（CHW 排布：通道主维）
        const int64_t channels = s[1];
        const int64_t n = s[2];
        const int numClasses = static_cast<int>(channels - 4);
        for (int64_t i = 0; i < n; ++i) {
            int bestCls = -1;
            float bestScore = 0.0f;
            for (int c = 0; c < numClasses; ++c) {
                const float score = d[(4 + c) * n + i];
                if (score > bestScore) {
                    bestScore = score;
                    bestCls = c;
                }
            }
            if (bestCls < 0 || bestScore < confThreshold) continue;
            const float w = d[2 * n + i];
            const float h = d[3 * n + i];
            Detection det;
            det.x = std::max(0.0f, d[0 * n + i] - w / 2.0f);
            det.y = std::max(0.0f, d[1 * n + i] - h / 2.0f);
            det.width = w;
            det.height = h;
            det.classId = bestCls;
            det.confidence = bestScore;
            out.push_back(det);
        }
    }
    return out; // 其余形状（歧义/退化形态）不支持，返回空
}

std::vector<ModelHelpers::Detection> ModelHelpers::nmsBoxes(
    std::vector<Detection> detections, float iouThreshold) {
    std::stable_sort(detections.begin(), detections.end(),
                     [](const Detection& a, const Detection& b) {
                         return a.confidence > b.confidence;
                     });
    std::vector<bool> suppressed(detections.size(), false);
    std::vector<Detection> out;
    for (size_t i = 0; i < detections.size(); ++i) {
        if (suppressed[i]) continue;
        out.push_back(detections[i]);
        for (size_t j = i + 1; j < detections.size(); ++j) {
            if (!suppressed[j] && iouOf(detections[i], detections[j]) > iouThreshold) {
                suppressed[j] = true;
            }
        }
    }
    return out;
}

} // namespace wingman
