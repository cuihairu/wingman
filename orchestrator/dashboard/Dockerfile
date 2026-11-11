ARG NODE_IMAGE=node:22-alpine
# nginx-unprivileged：以 nginx 用户（uid 101）运行，监听 8080（非特权端口）。
# nginx.conf 同步改为 listen 8080，宿主端口映射保持 8080:8080 不变。
ARG NGINX_IMAGE=nginxinc/nginx-unprivileged:alpine

FROM ${NODE_IMAGE} AS build
WORKDIR /app

ARG NPM_REGISTRY=
RUN if [ -n "$NPM_REGISTRY" ]; then npm config set registry "$NPM_REGISTRY"; fi

COPY package.json pnpm-lock.yaml ./
# corepack 对 registry.npmjs.org 偶发连不上（2026-11-11 部署连败），重试兜底
RUN corepack enable && for i in 1 2 3; do \
      corepack prepare pnpm@9 --activate && break; \
      [ "$i" -lt 3 ] && sleep 10; \
    done && pnpm --version
RUN for i in 1 2 3; do \
      pnpm install --no-frozen-lockfile --ignore-scripts && break; \
      [ "$i" -lt 3 ] && sleep 10; \
    done

COPY . .
RUN pnpm run postinstall
RUN pnpm run build

FROM ${NGINX_IMAGE} AS runtime
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/dist/ /usr/share/nginx/html/

EXPOSE 8080
