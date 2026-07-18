FROM ghcr.io/tcarland/:v1.6.14

ENV TCMAKE_PROJECT=/opt
ENV TCMAKE_HOME=/opt/tcmake
ENV TCMAKE_PREFIX=/usr

USER root
WORKDIR /opt

COPY . /opt/trino-cpp

RUN chown -R tdh /opt/trino-cpp && \
    cd trino-cpp && \
    make && \
    make install

USER tdh
