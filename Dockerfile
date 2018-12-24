FROM ubuntu:latest
ENV DEBIAN_FRONTEND=noninteractive TZ=America/New_York
RUN apt-get update && \
  apt-get -qq -y install apt-utils build-essential tzdata libcurl4-openssl-dev libmysqlclient-dev gdb && \
  ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
  # timezone: https://serverfault.com/a/683651/456938
COPY . /src
WORKDIR /src
RUN make all
CMD ["/src/init.sh"]
