FROM alpine:3.10.2
USER root
RUN apk add gcc make musl-dev pkgconfig openssl openssl-dev curl
RUN curl https://raw.githubusercontent.com/eqela/sushivm/master/install.sh | VERSION="v1.6.0" sh
COPY src /sushi-build
RUN PATH="/root/.sushi/bin:$PATH" make STATIC_BUILD=yes -C /sushi-build
RUN strip /sushi-build/sushi
RUN mkdir -p /staging/home /staging/tmp && chown 100.100 /staging/home && chmod ugo+rwx /staging/tmp

FROM scratch
LABEL maintainer="Eqela <contact@eqela.com>"
COPY --from=0 /sushi-build/sushi /sushi
COPY --from=0 /staging/home /home
COPY --from=0 /staging/tmp /tmp
WORKDIR /home
ENV HOME /home
