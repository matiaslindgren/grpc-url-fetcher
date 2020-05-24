# grpc-url-fetcher

gRPC and cURL powered URL fetching service with internal thread pool to hide HTTP latency.

## C++ API

Start a server:
```c++
#include <string>
#include "URLFetcherServer.hpp"

using urlfetcher::server::run_forever;

int main(int argc, char** argv) {
    std::string grpc_address{"localhost:8000"};
    int thread_pool_size{16};
    run_forever(grpc_address, thread_pool_size);
    return 0;
}
```

Request some URLs and get responses over a bidirectional gRPC stream:
```c++
#include <iostream>
#include <string>
#include <vector>
#include "URLFetcherClient.hpp"

using urlfetcher::Response;
using urlfetcher::client::uint64;
using urlfetcher::client::URLFetcherClient;

int main(int argc, char** argv) {
    std::string grpc_address{"localhost:8000"};
    std::vector<std::string> urls = {
        "https://matiaslindgren.github.io/",
        "https://httpstat.us/200",
        "https://httpstat.us/308",
        "https://httpstat.us/404",
        "https://yle.fi",
    };
    URLFetcherClient fetcher{grpc_address};
    // Request a fetch of URLs, this call resolves immediately, returning a list of keys
    std::vector<uint64> keys = fetcher.request_fetches(urls);
    std::copy(keys.begin(), keys.end(), std::ostream_iterator<uint64>(std::cout, ", "));
    std::cout << "\n";
    // The server passes all URLs to its thread pool, which starts to fetch them with cURL
    // We can ask for the resolved requests by passing the UUIDs returned by the server
    std::vector<Response> responses = fetcher.resolve_fetches(keys);
    for (int i = 0; i < urls.size(); ++i) {
        std::cout
            << urls[i]
            << ", header size " << responses[i].header().size()
            << ", body size " << responses[i].body().size()
            << ", error code " << responses[i].curl_error()
            << "\n------------\n";
    }
    return 0;
}
```

Send a SIGTERM or SIGINT to the server to shut it down.


## Building and testing with Docker

Three Dockerfiles have been included for building the project and the test runners.
Build them by running:
```
sh docker/build-all-images.sh
```
Run `docker images` and check that you have these images:
```
REPOSITORY          TAG                 IMAGE ID            CREATED             SIZE
test-runner         v1                  535c66a4c3c1        12 minutes ago      154MB
http-echo-server    v1                  76047517d54b        12 minutes ago      132MB
urlfetcher          v1                  416c0361ed24        13 minutes ago      154MB
```
To run the tests, you need to first start a Flask HTTP echo server that simply returns the URL route key for every HTTP GET request.
```
sh docker/run-echo-server.sh
```
In a second terminal, run all tests:
```
sh docker/run-tests.sh
```
The gRPC URL fetcher will request a lot of URLs from the localhost Flask server.
It will also request a few external URLs (see `external_urls` in `tests/main.cpp`).

Note that you might need to configure Docker networking to allow the URL fetcher access both the `http-echo-server` and the web.
