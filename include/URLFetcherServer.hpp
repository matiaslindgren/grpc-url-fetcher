#ifndef INCLUDED_URLFETCHERSERVER_HPP
#define INCLUDED_URLFETCHERSERVER_HPP

#include <atomic>
#include <csignal>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <concurrentqueue/blockingconcurrentqueue.h>
#include <curl/curl.h>
#include <google/protobuf/stubs/common.h>
#include <grpcpp/grpcpp.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include "urlfetcher.grpc.pb.h"


namespace urlfetcher::server {

using grpc::Server;
using grpc::ServerReaderWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using google::protobuf::uint64;
using urlfetcher::PendingFetch;
using urlfetcher::Request;
using urlfetcher::Response;
using urlfetcher::URLFetcher;


auto logger = spdlog::stdout_logger_mt("URLFetcherServer");

constexpr long TIMEOUT_CURL_GET_MS{60'000L};
constexpr int NUM_FETCH_THREADS{16};
constexpr int FETCHER_THREAD_WAIT_ON_EMPTY_MS{200};


size_t curl_response_to_std_string(void* curl_response, size_t size, size_t nmemb, std::string* response) {
    size_t response_size{size * nmemb};
    response->append(static_cast<char*>(curl_response), response_size);
    return response_size;
}

Response fetch_URL(const std::string& url) {
    Response response;
    CURL *curl = curl_easy_init();
    if (!curl) {
        logger->critical("Failed to initialize cURL instance, cannot request given URL '{:s}'", url);
    }
    else {
        // Prepare to fetch given URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        // If requested URL is redirected, fetch the contents after redirection
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        // Timeout if there's no response within given time
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, TIMEOUT_CURL_GET_MS);
        // On response, use callback to write header and body into two different strings
        std::string result_header;
        std::string result_body;
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_response_to_std_string);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &result_header);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result_body);
        // Perform the request
        logger->debug("cURL performing GET on '{:s}' with timeout {:d} ms", url, TIMEOUT_CURL_GET_MS);
        CURLcode error = curl_easy_perform(curl);
        // Write response object if there were no errors
        if (error != CURLE_OK) {
            logger->error("cURL GET failed with error string '{:s}'", curl_easy_strerror(error));
        }
        else {
            logger->debug("cURL GET successful on '{:s}'", url);
            response.set_header(result_header);
            response.set_body(result_body);
        }
        response.set_curl_error(error);
        curl_easy_cleanup(curl);
    }
    return response;
}


class URLFetcherService final : public URLFetcher::Service {
public:
    explicit URLFetcherService(int num_fetcher_threads) : fetchers_(num_fetcher_threads) {
        StartFetcherThreads();
    }
    ~URLFetcherService() noexcept {
        StopFetcherThreads();
    }
    // Let's disallow copy and move semantics because our thread pool is tied to the instance of this class
    // Trying to copy or move it will require some additional thought
    URLFetcherService (const URLFetcherService&) = delete;
    URLFetcherService (URLFetcherService&&) = delete;
    URLFetcherService& operator=(const URLFetcherService&) = delete;
    URLFetcherService& operator=(URLFetcherService&&) = delete;

    Status RequestFetch(ServerContext* context, ServerReaderWriter<PendingFetch, Request>* stream) override {
        logger->info("Reading URL fetch requests from stream");
        Request request;
        while (stream->Read(&request)) {
            logger->debug("Got URL '{:s}'", request.url());
            PendingFetch pending_fetch;
            uint64 key = create_uuid();
            pending_fetch.set_key(key);
            stream->Write(pending_fetch);
            fetch_queue_.enqueue({key, request.url()});
        }
        logger->info("RequestFetch finished, returning OK");
        return Status::OK;
    }

    Status ResolveFetch(ServerContext* context, ServerReaderWriter<Response, PendingFetch>* stream) override {
        PendingFetch pending_fetch;
        while (stream->Read(&pending_fetch)) {
            logger->info("Reading pending fetch {:d}", pending_fetch.key());
            // Polling for results from the fetcher thread pool using simple exponential backoff up to approx. 1 min
            //TODO remove polling by using subqueues from concurrentqueue.h, where queue tokens could be gRPC client id's that are sent with the metadata
            for (int poll_ms = 2 << 3;
                 is_fetching_ && result_is_pending(pending_fetch.key());
                 poll_ms = std::min(2 << 14, poll_ms << 1))
            {
                logger->debug("No results for key {:d}, waiting for {:d} ms", pending_fetch.key(), poll_ms);
                std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
            }
            Response response = pop_completed_fetch(pending_fetch.key());
            stream->Write(response);
        }
        logger->info("ResolveFetch finished, returning OK");
        return Status::OK;
    }

    void StartFetcherThreads() {
        logger->info("Starting {:d} fetcher threads", fetchers_.size());
        is_fetching_ = true;
        for (int i = 0; i < fetchers_.size(); ++i) {
            if (fetchers_[i].joinable()) {
                logger->warn("Will not overwrite fetcher thread {:d} with new thread because it is already running", i);
            }
            else {
                logger->debug("Starting fetcher thread {:d}", i);
                fetchers_[i] = std::thread(&URLFetcherService::URL_fetch_loop, this);
            }
        }
    }

    void StopFetcherThreads() {
        logger->info("Stopping {:d} fetcher threads", fetchers_.size());
        is_fetching_ = false;
        for (int i = 0; i < fetchers_.size(); ++i) {
            if (!fetchers_[i].joinable()) {
                logger->warn("Fetcher thread {:d} is not running, will not join it", i);
            }
            else {
                logger->debug("Stopping fetcher thread {:d}", i);
                fetchers_[i].join();
            }
        }
    }

private:
    uint64 create_uuid() {
        return ++previous_uuid_;
    }

    void URL_fetch_loop() {
        auto wait_on_empty_ms = std::chrono::milliseconds(FETCHER_THREAD_WAIT_ON_EMPTY_MS);
        while (is_fetching_) {
            std::pair<uint64, std::string> key_and_url;
            if (fetch_queue_.wait_dequeue_timed(key_and_url, wait_on_empty_ms)) {
                auto [key, url] = key_and_url;
                logger->debug("URL_fetch_loop handling key {:d} url '{:s}'", key, url);
                Response response{fetch_URL(url)};
                write_completed_fetch(key, response);
            }
        }
    }

    bool result_is_pending(uint64 key) {
        std::unique_lock<std::mutex> guard(queue_mutex_);
        return completed_fetches_.find(key) == completed_fetches_.end();
    }

    Response pop_completed_fetch(uint64 key) {
        std::unique_lock<std::mutex> guard(queue_mutex_);
        auto item = completed_fetches_.find(key);
        Response response = item->second;
        completed_fetches_.erase(item);
        return response;
    }

    void write_completed_fetch(uint64 key, const Response& response) {
        std::unique_lock<std::mutex> guard(queue_mutex_);
        if (completed_fetches_.find(key) != completed_fetches_.end()) {
            logger->warn("Overwriting existing, completed fetch at key {:d}", key);
        }
        completed_fetches_[key] = response;
    }

    std::atomic<uint64> previous_uuid_{0};
    std::vector<std::thread> fetchers_;
    bool is_fetching_;
    moodycamel::BlockingConcurrentQueue<std::pair<uint64, std::string> > fetch_queue_;
    std::unordered_map<uint64, Response> completed_fetches_;
    std::mutex queue_mutex_;
};


// Wrapper for allowing capture in lambda signal handler
// https://stackoverflow.com/a/48164204
// An alternative might be to make the URLFetcherService instance a global variable
std::function<void(int)> shutdown_handler;
void signal_handler(int signal) { shutdown_handler(signal); }

void run_forever(const std::string& address, int num_fetcher_threads = NUM_FETCH_THREADS) {
    ServerBuilder builder;
    builder.AddListeningPort(address, grpc::InsecureServerCredentials());
    URLFetcherService service(num_fetcher_threads);
    builder.RegisterService(&service);
    std::unique_ptr<Server> server(builder.BuildAndStart());
    logger->info("Server listening on '{:s}'", address);
    // Allow parent process to terminate the server gracefully with a SIGTERM or SIGINT
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    shutdown_handler = [&server](int signal) -> void {
        logger->info("Received signal {:d}, server shutting down", signal);
        server->Shutdown();
    };
    server->Wait();
}

} // namespace urlfetcher

#endif // INCLUDED_URLFETCHERSERVER_HPP
