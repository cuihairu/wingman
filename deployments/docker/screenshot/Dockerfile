# Wingman 部署记录截图用 headless chromium（deploy-server.yml build-screenshot 构建推 GHCR）
#
# 选型历程（2026-10-03）：
#   - chromedp/headless-shell（151/153 精简构建）：内核 7.0 上 zygote 崩溃
#     （GPU process launch failed error_code=1002），about:blank 都挂死，弃用；
#   - mcr playwright（完整 chromium）：镜像 2.5GB，部署机到 mcr 带宽 1500s 拉不完，
#     截图步连败，弃用；
#   - 本镜像：alpine + chromium + CJK 字体，push 后数百 MB，GHCR 同机秒拉。
#
# 版本 pin：alpine 3.20 仓库 frozen，chromium 固定 131.x；升级时同步改
# deploy-server.yml 里 build-screenshot 的镜像 tag。
FROM alpine:3.20
# 自建 runner 到 dl-cdn 的索引拉取偶发 SSL 抖断（main 索引断流时 chromium
# 的 main 仓依赖全量 no such package，2026-10-09 build-screenshot 连败）；
# 换国内稳定镜像源，再加重试兜底（失败 3 次后 apk info 校验仍会置失败）
RUN sed -i 's|https://dl-cdn.alpinelinux.org|https://mirrors.tuna.tsinghua.edu.cn|g' /etc/apk/repositories \
 && for i in 1 2 3; do \
      apk add --no-cache chromium font-noto-cjk && break; \
      [ "$i" -lt 3 ] && sleep 10; \
    done && apk info -e chromium font-noto-cjk
# 完整 chromium（非 headless-shell）必须显式 --headless，且容器内 root 跑要
# --no-sandbox —— 这两个 flag 由部署工作流的截图命令传入
ENTRYPOINT ["/usr/bin/chromium-browser"]
