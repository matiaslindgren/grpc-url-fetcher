#include <iostream>
#include <cxxopts/cxxopts.hpp>
#include "URLFetcherClient.hpp"


using urlfetcher::Response;
using urlfetcher::client::fetch_urls_from_server;
using urlfetcher::client::logger;


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
    std::string server_address = args["address"].as<std::string>();
    std::vector<std::string> urls = {
        "https://matiaslindgren.github.io/",
        "https://httpstat.us/200",
        "https://httpstat.us/308",
        "https://httpstat.us/404",
        "https://yle.fi",
    };
    auto responses = fetch_urls_from_server(urls, server_address);
    return 0;
}
