# grpc-url-fetcher binaries
docker build --tag urlfetcher:v1 --file docker/urlfetcher.dockerfile .
# Tests
docker build --tag test-runner:v1 --file docker/test-runner.dockerfile .
docker build --tag http-echo-server:v1 --file docker/http-echo-server.dockerfile .
# CLI example
docker build --tag urlfetcher-client:v1 --file docker/urlfetcher-client.dockerfile .
docker build --tag urlfetcher-server:v1 --file docker/urlfetcher-server.dockerfile .
