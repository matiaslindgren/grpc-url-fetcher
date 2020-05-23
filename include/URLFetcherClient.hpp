#ifndef INCLUDED_URLFETCHERCLIENT_HPP
#define INCLUDED_URLFETCHERCLIENT_HPP

#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <google/protobuf/stubs/common.h>
#include <grpcpp/grpcpp.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include "urlfetcher.grpc.pb.h"


namespace urlfetcher::client {

using google::protobuf::uint64;
using grpc::ClientContext;
using grpc::ClientReaderWriter;
using grpc::Status;
using urlfetcher::PendingFetch;
using urlfetcher::Request;
using urlfetcher::Response;
using urlfetcher::URLFetcher;


auto logger = spdlog::stdout_logger_mt("URLFetcherClient");


class URLFetcherClient final {
public:
    explicit URLFetcherClient(const std::string& server_address) {
        auto channel = grpc::CreateChannel(server_address, grpc::InsecureChannelCredentials());
        stub_ = URLFetcher::NewStub(channel);
    }

    std::vector<uint64> request_fetches(const std::vector<std::string>& urls) {
        logger->info("Requesting {:d} urls from server", urls.size());
        ClientContext context;
        std::shared_ptr<ClientReaderWriter<Request, PendingFetch> > stream(stub_->RequestFetch(&context));
        for (auto url : urls) {
            logger->debug("Writing '{:s}' to stream", url);
            Request request;
            request.set_url(url);
            stream->Write(request);
        }
        stream->WritesDone();
        logger->debug("All {:d} urls written to stream", urls.size());
        PendingFetch pending_fetch;
        std::vector<uint64> keys;
        keys.reserve(urls.size());
        while (stream->Read(&pending_fetch)) {
            logger->info("Received pending fetch with key {:d}", pending_fetch.key());
            keys.push_back(pending_fetch.key());
        }
        Status status = stream->Finish();
        if (!status.ok()) {
            logger->warn("RequestFetch RPC stream finished with errors:\n   code: {:d}\n  message: {:s}\n  details: {:s}",
                    status.error_code(),
                    status.error_message(),
                    status.error_details());
        }
        return keys;
    }

    std::vector<Response> resolve_fetches(const std::vector<uint64>& keys) {
        logger->info("Resolving {:d} pending fetches", keys.size());
        ClientContext context;
        std::shared_ptr<ClientReaderWriter<PendingFetch, Response> > stream(stub_->ResolveFetch(&context));
        for (auto key : keys) {
            logger->debug("Writing {:d} to stream", key);
            PendingFetch pending_fetch;
            pending_fetch.set_key(key);
            stream->Write(pending_fetch);
        }
        stream->WritesDone();
        logger->debug("All {:d} keys written to stream", keys.size());
        std::vector<Response> responses;
        responses.reserve(keys.size());
        Response response;
        while (stream->Read(&response)) {
            logger->info("Received response, header size {:d}, body size {:d}, error code {:d}",
                response.header().size(),
                response.body().size(),
                response.curl_error());
            responses.push_back(response);
        }
        Status status = stream->Finish();
        if (!status.ok()) {
            logger->warn("ResolveFetch RPC stream finished with errors:\n   code: {:d}\n  message: {:s}\n  details: {:s}",
                    status.error_code(),
                    status.error_message(),
                    status.error_details());
        }
        return responses;
    }

private:
    std::unique_ptr<URLFetcher::Stub> stub_;
};


std::vector<Response> fetch_urls_from_server(const std::vector<std::string>& urls, const std::string& server_address) {
    URLFetcherClient fetcher(server_address);
    auto keys = fetcher.request_fetches(urls);
    return fetcher.resolve_fetches(keys);
}


} // namespace urlfetcher

#endif // INCLUDED_URLFETCHERCLIENT_HPP
