FROM alpine:3.10.2
USER root
RUN apk add gcc make musl-dev pkgconfig openssl openssl-dev curl
RUN curl https://raw.githubusercontent.com/eqela/sushivm/master/install.sh | VERSION="v1.6.0" sh
COPY src /sushi-build
RUN PATH="/root/.sushi/bin:$PATH" make STATIC_BUILD=yes -C /sushi-build
RUN strip /sushi-build/sushi

FROM scratch
LABEL maintainer="Eqela <contact@eqela.com>"
COPY --from=0 /sushi-build/sushi /sushi
