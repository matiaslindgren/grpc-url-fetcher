FROM urlfetcher:v1

ENV URLFETCHER_ECHO_SERVICE_ADDRESS=172.17.0.2:7000
ENV URLFETCHER_GRPC_TEST_ADDRESS=172.17.0.3:8000

ENTRYPOINT ["/usr/local/bin/URLFetcherTests", "--success", "--reporter=compact"]
