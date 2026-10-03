ARG NODE_IMAGE=node:22-alpine
# nginx-unprivileged：以 nginx 用户（uid 101）运行，监听 8080（非特权端口）。
# nginx.conf 同步改为 listen 8080，宿主端口映射保持 8080:8080 不变。
ARG NGINX_IMAGE=nginxinc/nginx-unprivileged:alpine

FROM ${NODE_IMAGE} AS build
WORKDIR /app

ARG NPM_REGISTRY=
RUN if [ -n "$NPM_REGISTRY" ]; then npm config set registry "$NPM_REGISTRY"; fi

COPY package.json pnpm-lock.yaml ./
RUN corepack enable && corepack prepare pnpm@9 --activate && pnpm install --no-frozen-lockfile --ignore-scripts

COPY . .
RUN pnpm run postinstall
RUN pnpm run build

FROM ${NGINX_IMAGE} AS runtime
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/dist/ /usr/share/nginx/html/

EXPOSE 8080
