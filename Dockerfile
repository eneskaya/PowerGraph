FROM ubuntu:12.04 as base

RUN apt-get update -y
RUN apt-get install -y gcc g++ build-essential libopenmpi-dev openmpi-bin default-jdk cmake zlib1g-dev git ant

RUN apt-get update && apt-get install -y openssh-server
RUN mkdir /var/run/sshd
RUN echo 'root:password' | chpasswd
RUN sed -i 's/PermitRootLogin without-password/PermitRootLogin yes/' /etc/ssh/sshd_config

# SSH login fix. Otherwise user is kicked off after login
RUN sed 's@session\s*required\s*pam_loginuid.so@session optional pam_loginuid.so@g' -i /etc/pam.d/sshd

ENV NOTVISIBLE "in users profile"
RUN echo "export VISIBLE=now" >> /etc/profile

EXPOSE 22

ADD ssh /root/.ssh
RUN chmod 400 /root/.ssh/id_rsa

# Build graph analytics toolkit
ADD . /graphlab
RUN cd /graphlab && ./configure
WORKDIR /graphlab/release/toolkits
RUN cd /graphlab/release/toolkits/graph_analytics && make -j4

FROM base

# ...

ENTRYPOINT ["/usr/sbin/sshd", "-D"]