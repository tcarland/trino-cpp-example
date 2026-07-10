FROM ghcr.io/tcarland/tcanetpp:v1.6.14

ENV TCAMAKE_PROJECT=/opt
ENV TCAMAKE_HOME=/opt/tcmake
ENV TCAMAKE_PREFIX=/usr

USER root
WORKDIR /opt

COPY . /opt/trino-cpp

RUN chown -R tdh /opt/trino-cpp && \
    cd trino-cpp && \
    make && \
    make install

USER tdh
