FROM urlfetcher:v1

ENV URLFETCHER_ECHO_SERVICE_ADDRESS=127.0.0.1:7000
ENV URLFETCHER_GRPC_TEST_ADDRESS=127.0.0.01:8000

ENTRYPOINT ["/usr/local/bin/URLFetcherTests", "--success", "--reporter=compact"]
