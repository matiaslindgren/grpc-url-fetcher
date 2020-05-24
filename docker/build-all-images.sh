docker build --no-cache --tag urlfetcher:v1 --file docker/urlfetcher.dockerfile .
docker build --no-cache --tag test-runner:v1 --file docker/test-runner.dockerfile .
docker build --no-cache --tag http-echo-server:v1 --file docker/http-echo-server.dockerfile .
