#include <iostream>
#include <cxxopts/cxxopts.hpp>
#include "URLFetcherServer.hpp"

using urlfetcher::server::logger;
using urlfetcher::server::run_forever;


decltype(auto) parse_args_or_exit(int argc, char** argv) {
    cxxopts::Options options(
            "URLFetcherServer",
            "gRPC and cURL powered URL fetching service with internal thread pool to hide HTTP latency.");
    options.add_options()
        ("h,help",
         "Print this message and exit")
        ("v,verbose",
         "Increase logging verbosity by each given -v up to 2. 0 = warning (default), 1 = info, 2 = debug")
        ("a,address",
         "gRPC serving address, clients should connect to this",
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
    run_forever(server_address);
    return 0;
}
