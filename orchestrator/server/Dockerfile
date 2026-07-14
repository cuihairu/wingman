FROM golang:1.21-alpine AS builder

WORKDIR /build

# Install build dependencies
RUN apk add --no-cache git ca-certificates

# Copy go mod files
COPY go.mod go.sum ./
RUN go mod download

# Copy source code
COPY . .

# Build the server
RUN CGO_ENABLED=0 GOOS=linux go build -a -installsuffix cgo -o wingman-server .

# Runtime stage
FROM alpine:latest

RUN apk add --no-cache ca-certificates tzdata

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

# Health check
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD wget --no-verbose --tries=1 --spider http://localhost:9527/api/v1/health || exit 1

# Run the server
CMD ["./wingman-server"]
