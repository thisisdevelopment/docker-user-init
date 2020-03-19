FROM gcc:latest AS build

ADD docker-user-init.c /app/
WORKDIR /app

RUN gcc -o docker-user-init docker-user-init.c

FROM scratch

COPY --from=build /app/docker-user-init /
