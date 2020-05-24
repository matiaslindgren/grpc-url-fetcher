FROM debian:buster-slim

ARG URLFETCHER_TAR=https://github.com/matiaslindgren/grpc-url-fetcher/archive/master.tar.gz

RUN \
	apt update \
	&& apt install -y --no-install-recommends ca-certificates curl tar python3 python3-flask python3-setuptools \
	&& update-ca-certificates \
	&& mkdir --parents --verbose /usr/src/urlfetcher \
	&& curl --location $URLFETCHER_TAR | tar --extract --gunzip --strip-components 1 --directory /usr/src/urlfetcher

ENV FLASK_APP=/usr/src/urlfetcher/tests/http_echo_server.py

ENTRYPOINT ["flask", "run", "--host=172.17.0.2", "--port=7000"]
