#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <cxxopts/cxxopts.hpp>
#include "URLFetcherClient.hpp"

using urlfetcher::Response;
using urlfetcher::client::URLFetcherClient;
using urlfetcher::client::logger;
using urlfetcher::client::uint64;


decltype(auto) parse_args_or_exit(int argc, char** argv) {
    cxxopts::Options options("URLFetcherClient", "Client for URLFetcherServer.");
    options.add_options()
        ("h,help",
         "Print this message and exit")
        ("v,verbose",
         "Increase logging verbosity by each given -v up to 2. 0 = warning, 1 = info, 2 = debug")
        ("a,address",
         "gRPC serving address, establish connection to this server.",
         cxxopts::value<std::string>()->default_value("localhost:8000"))
        ;
    auto args = options.parse(argc, argv);
    if (args.count("help")) {
        std::cout << options.help() << std::endl;
        exit(0);
    }
    switch (args.count("verbose")) {
        case 0:
            logger->set_level(spdlog::level::warn);
            break;
        case 1:
            logger->set_level(spdlog::level::info);
            break;
        case 2:
            logger->set_level(spdlog::level::debug);
            break;
        default:
            std::cerr << "Unknown verbosity level " << args.count("verbose") << "\n";
            exit(1);
            break;
    }
    return args;
}

int main(int argc, char** argv) {
    auto args = parse_args_or_exit(argc, argv);
    std::string grpc_address = args["address"].as<std::string>();
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
