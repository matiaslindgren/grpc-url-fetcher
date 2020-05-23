#include <algorithm>
#include <csignal>
#include <numeric>
#include <string>
#include <thread>

#include "URLFetcherClient.hpp"
#include "URLFetcherServer.hpp"

#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>
#undef CATCH_CONFIG_MAIN
#include <fmt/format.h>

const std::string http_echo_service_address{"localhost:8000"};
const std::string grpc_test_address{"localhost:7000"};
const auto test_loglevel{spdlog::level::warn};


std::string random_localhost_echo_url() {
    static std::random_device rd;
    static const auto data_generator_seed = rd();
    static std::default_random_engine rand_engine(data_generator_seed);
    std::uniform_int_distribution<long> random_int(1, 1 << 20);
    return fmt::format("{:s}/echo/{:d}", http_echo_service_address, random_int(rand_engine));
}

std::vector<std::string> generate_localhost_echo_urls(size_t num_urls) {
    std::vector<std::string> urls(num_urls);
    std::generate(urls.begin(), urls.end(), random_localhost_echo_url);
    return urls;
}

std::vector<std::string> slurp_lines(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    std::vector<std::string> lines;
    while (std::getline(file, line, '\n')) {
        lines.push_back(line);
    }
    return lines;
}


TEST_CASE("Server terminates on SIGINT and SIGTERM", "[server]") {
    using urlfetcher::server::run_forever;
    using urlfetcher::server::shutdown_handler;
    urlfetcher::server::logger->set_level(test_loglevel);
    for (auto signal : {SIGINT, SIGTERM}) {
        std::thread server_runner([] { run_forever(grpc_test_address); });
        std::this_thread::sleep_for(std::chrono::seconds(1));
        shutdown_handler(signal);
        server_runner.join();
        REQUIRE(true);
    }
}

TEST_CASE("Server returns monotonically increasing UUIDs for request_fetches", "[request-fetches]") {
    using urlfetcher::server::run_forever;
    using urlfetcher::server::shutdown_handler;
    using urlfetcher::client::URLFetcherClient;
    urlfetcher::server::logger->set_level(test_loglevel);
    urlfetcher::client::logger->set_level(test_loglevel);
    std::thread server_runner([] { run_forever(grpc_test_address); });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    using urlfetcher::client::uint64;
    uint64 min_key = 0;
    for (auto num_urls : {0, 1, 10, 100, 1000, 10'000}) {
        std::vector<std::string> urls = generate_localhost_echo_urls(num_urls);
        URLFetcherClient fetcher(grpc_test_address);
        auto keys = fetcher.request_fetches(urls);
        REQUIRE(keys.size() == urls.size());
        // UUIDs are generated for each new RPC and gRPC streams are ordered <-> returned UUIDs must be sorted
        REQUIRE(std::is_sorted(keys.begin(), keys.end()));
        // UUIDs must be unique
        REQUIRE(std::adjacent_find(keys.begin(), keys.end()) == keys.end());
        if (num_urls > 0) {
            REQUIRE(min_key < *keys.begin());
            min_key = *keys.begin();
        }
    }
    shutdown_handler(SIGTERM);
    server_runner.join();
    REQUIRE(true);
}

TEST_CASE("Server returns resolved URLs when requested with the UUIDs from request_fetches", "[resolve-fetches]") {
    using urlfetcher::server::run_forever;
    using urlfetcher::server::shutdown_handler;
    using urlfetcher::client::URLFetcherClient;
    urlfetcher::server::logger->set_level(test_loglevel);
    urlfetcher::client::logger->set_level(test_loglevel);
    std::thread server_runner([] { run_forever(grpc_test_address); });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (auto num_urls : {0, 1, 10, 100, 1000, 10'000}) {
        std::vector<std::string> urls = generate_localhost_echo_urls(num_urls);
        URLFetcherClient fetcher(grpc_test_address);
        auto keys = fetcher.request_fetches(urls);
        REQUIRE(keys.size() == urls.size());
        auto responses = fetcher.resolve_fetches(keys);
        REQUIRE(responses.size() == urls.size());
        for (int i = 0; i < urls.size(); ++i) {
            // The echo server returns just the route key,
            // e.g. a GET to "localhost:8000/1" returns response with "1" in the body
            REQUIRE(responses[i].curl_error() == 0);
            std::string url_route = urls[i].substr(urls[i].rfind("/") + 1);
            REQUIRE(responses[i].body() == url_route);
        }
    }
    shutdown_handler(SIGTERM);
    server_runner.join();
    REQUIRE(true);
}

TEST_CASE("fetch_urls_from_server convenience method and the URLFetcherClient both resolve the same URLs correctly", "[convenience-method]") {
    using urlfetcher::server::run_forever;
    using urlfetcher::server::shutdown_handler;
    using urlfetcher::client::fetch_urls_from_server;
    urlfetcher::server::logger->set_level(test_loglevel);
    urlfetcher::client::logger->set_level(test_loglevel);
    using urlfetcher::client::URLFetcherClient;
    std::thread server_runner_1([] { run_forever(grpc_test_address); });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (auto num_urls : {0, 1, 10, 100, 1000, 10'000}) {
        std::vector<std::string> urls = generate_localhost_echo_urls(num_urls);
        URLFetcherClient fetcher(grpc_test_address);
        auto keys = fetcher.request_fetches(urls);
        REQUIRE(keys.size() == urls.size());
        auto responses_1 = fetcher.resolve_fetches(keys);
        auto responses_2 = fetch_urls_from_server(urls, grpc_test_address);
        REQUIRE(responses_1.size() == urls.size());
        REQUIRE(responses_1.size() == responses_2.size());
        for (int i = 0; i < urls.size(); ++i) {
            REQUIRE(responses_1[i].curl_error() == 0);
            REQUIRE(responses_2[i].curl_error() == 0);
            std::string url_route = urls[i].substr(urls[i].rfind("/") + 1);
            REQUIRE(responses_1[i].body() == url_route);
            REQUIRE(responses_2[i].body() == url_route);
        }
    }
    shutdown_handler(SIGTERM);
    server_runner_1.join();
    REQUIRE(true);
}

TEST_CASE("Fetching common URLs with URLFetcherService returns non-empty responses without cURL errors.", "[external-urls]") {
    using urlfetcher::server::run_forever;
    using urlfetcher::server::shutdown_handler;
    using urlfetcher::client::fetch_urls_from_server;
    urlfetcher::server::logger->set_level(test_loglevel);
    urlfetcher::client::logger->set_level(test_loglevel);
    std::thread server_runner([] { run_forever(grpc_test_address); });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<std::string> urls = slurp_lines("common_urls.list");
    REQUIRE(!urls.empty());
    auto responses = fetch_urls_from_server(urls, grpc_test_address);
    shutdown_handler(SIGTERM);
    server_runner.join();
    REQUIRE(true);
    for (int i = 0; i < urls.size(); ++i) {
        REQUIRE(responses[i].curl_error() == 0);
        REQUIRE(!responses[i].header().empty());
        REQUIRE(!responses[i].body().empty());
    }
}

TEST_CASE("All URLs fetched by the URLFetcherService have correct HTTP status codes in the headers", "[localhost-httpstatus]") {
    using urlfetcher::server::run_forever;
    using urlfetcher::server::shutdown_handler;
    using urlfetcher::client::fetch_urls_from_server;
    urlfetcher::server::logger->set_level(test_loglevel);
    urlfetcher::client::logger->set_level(test_loglevel);
    std::thread server_runner([]{ run_forever(grpc_test_address); });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::vector<std::string> urls = slurp_lines("http-statuscodes.csv");
    std::transform(urls.begin(), urls.end(), urls.begin(), [](const std::string& s) -> std::string {
            return fmt::format("{:s}/error/{:s}", http_echo_service_address, s.substr(0, 3)); });
    REQUIRE(!urls.empty());
    auto responses = fetch_urls_from_server(urls, grpc_test_address);
    shutdown_handler(SIGTERM);
    server_runner.join();
    REQUIRE(true);
    for (int i = 0; i < urls.size(); ++i) {
        std::string response_header = responses[i].header();
        REQUIRE(!response_header.empty());
        std::string http_code = response_header.substr(response_header.find(" ") + 1, 3);
        std::string url = urls[i];
        std::string expected_code = url.substr(url.substr(0, url.size() - 1).rfind("/") + 1, 3);
        REQUIRE(http_code == expected_code);
    }
}

TEST_CASE("Server returns monotonically increasing UUIDs for request_fetches for multiple concurrent clients", "[request-fetches-concurrent]") {
    using urlfetcher::Response;
    using urlfetcher::server::run_forever;
    using urlfetcher::server::shutdown_handler;
    using urlfetcher::client::URLFetcherClient;
    using urlfetcher::client::uint64;
    urlfetcher::server::logger->set_level(test_loglevel);
    urlfetcher::client::logger->set_level(test_loglevel);
    std::thread server_runner([] { run_forever(grpc_test_address); });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (int num_clients : {1, 10, 50, 100, 200}) {
        for (int num_urls : {1, 10, 50, 100, 200}) {
            std::vector<std::string> urls = generate_localhost_echo_urls(num_urls);
            std::vector<std::thread> clients(num_clients);
            std::vector<std::vector<uint64>> all_keys(num_clients);
            std::mutex keys_lock;
            auto fetch_urls_and_write_results_with_lock = [&urls, &keys_lock, &all_keys](int t) -> void {
                URLFetcherClient fetcher(grpc_test_address);
                auto keys = fetcher.request_fetches(urls);
                {
                    std::unique_lock<std::mutex> lock(keys_lock);
                    all_keys[t] = keys;
                }
            };
            for (int t = 0; t < num_clients; ++t) {
                clients[t] = std::thread(fetch_urls_and_write_results_with_lock, t);
            }
            for (auto& client : clients) {
                client.join();
            }
            for (auto keys : all_keys) {
                REQUIRE(keys.size() == urls.size());
                REQUIRE(std::is_sorted(keys.begin(), keys.end()));
                REQUIRE(std::adjacent_find(keys.begin(), keys.end()) == keys.end());
            }
        }
    }
    shutdown_handler(SIGTERM);
    server_runner.join();
    REQUIRE(true);
}

TEST_CASE("Server returns resolved URLs when requested with the UUIDs from request_fetches for multiple concurrent clients", "[resolve-fetches-concurrent]") {
    using urlfetcher::Response;
    using urlfetcher::server::run_forever;
    using urlfetcher::server::shutdown_handler;
    using urlfetcher::client::URLFetcherClient;
    using urlfetcher::client::uint64;
    urlfetcher::server::logger->set_level(test_loglevel);
    urlfetcher::client::logger->set_level(test_loglevel);
    std::thread server_runner([] { run_forever(grpc_test_address); });
    std::this_thread::sleep_for(std::chrono::seconds(1));
    for (int num_clients : {1, 10, 50, 100, 200}) {
        for (int num_urls : {1, 10, 50, 100, 200}) {
            std::vector<std::string> urls = generate_localhost_echo_urls(num_urls);
            std::vector<std::thread> clients(num_clients);
            std::vector<std::vector<Response>> all_responses(num_clients);
            std::mutex responses_lock;
            auto fetch_urls_and_write_results_with_lock = [&urls, &responses_lock, &all_responses](int t) -> void {
                URLFetcherClient fetcher(grpc_test_address);
                auto keys = fetcher.request_fetches(urls);
                auto responses = fetcher.resolve_fetches(keys);
                {
                    std::unique_lock<std::mutex> lock(responses_lock);
                    all_responses[t] = responses;
                }
            };
            for (int t = 0; t < num_clients; ++t) {
                clients[t] = std::thread(fetch_urls_and_write_results_with_lock, t);
            }
            for (auto& client : clients) {
                client.join();
            }
            for (auto responses : all_responses) {
                REQUIRE(responses.size() == urls.size());
                for (int i = 0; i < urls.size(); ++i) {
                    REQUIRE(responses[i].curl_error() == 0);
                    std::string url_route = urls[i].substr(urls[i].rfind("/") + 1);
                    REQUIRE(responses[i].body() == url_route);
                }
            }
        }
    }
    shutdown_handler(SIGTERM);
    server_runner.join();
    REQUIRE(true);
}
