FROM ubuntu:12.04 as base

RUN apt-get update -y
RUN apt-get install -y gcc g++ build-essential libopenmpi-dev openmpi-bin default-jdk cmake zlib1g-dev git ant

ADD . /pg

WORKDIR /pg

RUN ./configure

RUN cd /pg/release/toolkits/graph_analytics && make -j4

ENTRYPOINT [ "/bin/bash" ]