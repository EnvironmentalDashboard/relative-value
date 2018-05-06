FROM ubuntu:latest
ENV DEBIAN_FRONTEND=noninteractive TZ=America/New_York
COPY . /src
WORKDIR /src
RUN apt-get update && \
  apt-get -qq -y install apt-utils build-essential tzdata libcurl4-openssl-dev libmysqlclient-dev gdb && \
  make all && \
  ln -snf /usr/share/zoneinfo/$TZ /etc/localtime && echo $TZ > /etc/timezone
  # timezone: https://serverfault.com/a/683651/456938
CMD ["/src/init.sh"]
