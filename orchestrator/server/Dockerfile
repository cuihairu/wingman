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

WORKDIR /app

# Copy binary from builder
COPY --from=builder /build/wingman-server .

# Create necessary directories
RUN mkdir -p /app/data /app/scripts

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

# Health check (probe the TCP listener; /api/v1/health requires auth)
HEALTHCHECK --interval=10s --timeout=3s --start-period=5s --retries=3 \
  CMD wget --no-verbose --tries=1 --spider http://localhost:9527/ || exit 1

# Run the server
CMD ["./wingman-server"]
