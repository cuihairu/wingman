FROM golang:1.26-alpine AS builder

WORKDIR /build

# Install build dependencies (gcc + musl-dev required for CGO / go-sqlite3)
RUN apk add --no-cache git ca-certificates gcc musl-dev

# Copy go mod files
COPY go.mod go.sum ./
RUN go mod download

# Copy source code
COPY . .

# Build the server (CGO_ENABLED=1 required: go-sqlite3 needs cgo)
RUN CGO_ENABLED=1 GOOS=linux go build -o wingman-server .

# Runtime stage
FROM alpine:latest

RUN apk add --no-cache ca-certificates tzdata sqlite-libs

# 非 root 运行账号（uid/gid 10001）。数据卷 wingman-data / wingman-scripts 由
# 部署腿迁移属主到该 uid（named volume 首次挂载会继承镜像目录属主，仅对
# 全新卷生效；存量 root 属主卷必须显式 chown，见 deploy-server.yml）。
RUN addgroup -S -g 10001 wingman \
 && adduser -S -u 10001 -G wingman -h /app -s /sbin/nologin wingman

WORKDIR /app

# Copy binary from builder
COPY --from=builder /build/wingman-server .

# Create necessary directories
RUN mkdir -p /app/data /app/scripts \
 && chown -R wingman:wingman /app

USER wingman

# Expose ports
# 9527 - HTTP API
# 8888 - Agent TCP connection
EXPOSE 9527 8888

# Environment variables
ENV WINGMAN_HOST=0.0.0.0
ENV WINGMAN_PORT=9527
ENV WINGMAN_DB_PATH=/app/data/wingman.db
ENV WINGMAN_STATIC_DIR=/app/static
ENV WINGMAN_AGENT_ADDR=0.0.0.0:8888
ENV WINGMAN_SCRIPTS_DIR=/app/scripts

# Health check (probe the TCP listener; '/' returns 404 which is fine —
# wget --spider treats any HTTP response as success with --server-response,
# but 4xx/5xx exit non-zero by default, so check the login endpoint instead).
HEALTHCHECK --interval=10s --timeout=3s --start-period=10s --retries=3 \
  CMD wget -q -O /dev/null --spider http://localhost:9527/swagger/index.html || exit 1

# Run the server
CMD ["./wingman-server"]
