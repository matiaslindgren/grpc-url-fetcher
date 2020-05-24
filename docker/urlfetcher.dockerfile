# Build URL fetcher and all its dependencies, then copy the binaries into a minimal runtime image

# Step 1: Build gRPC and the URL fetcher project
FROM debian:buster-slim

# gRPC URL fetcher service, client, and tests
ARG URLFETCHER_TAR=https://github.com/matiaslindgren/grpc-url-fetcher/archive/master.tar.gz

# Third party libraries and projects
ARG CXXOPTS_TAR=https://github.com/jarro2783/cxxopts/archive/v2.2.0.tar.gz
ARG CATCH2_TAR=https://github.com/catchorg/Catch2/archive/v2.12.1.tar.gz
ARG SPDLOG_TAR=https://github.com/gabime/spdlog/archive/v1.6.0.tar.gz
ARG CONCURRENTQUEUE_TAR=https://github.com/cameron314/concurrentqueue/archive/v1.0.1.tar.gz
ARG FMTLIB_TAR=https://github.com/fmtlib/fmt/archive/6.2.1.tar.gz
ARG GRPC_GIT=https://github.com/grpc/grpc

# Compile everything
RUN \
	apt update \
	# cURL might complain about SSL certs when trying to get URLs over HTTPS
	&& apt install -y --no-install-recommends ca-certificates \
	&& update-ca-certificates \
	# Minimal build deps
	&& apt install -y --no-install-recommends tar curl libcurl4-openssl-dev cmake g++ git build-essential autoconf libtool pkg-config \
	# Download all third party dependencies
\
	&& mkdir --parents --verbose /usr/src/cxxopts \
	&& curl --location $CXXOPTS_TAR | tar --extract --gunzip --strip-components 1 --directory /usr/src/cxxopts \
	&& ln --symbolic /usr/src/cxxopts/include /usr/local/include/cxxopts \
\
	&& mkdir --parents --verbose /usr/src/catch2 \
	&& curl --location $CATCH2_TAR | tar --extract --gunzip --strip-components 1 --directory /usr/src/catch2 \
	&& ln --symbolic /usr/src/catch2/single_include/catch2 /usr/local/include/catch2 \
\
	&& mkdir --parents --verbose /usr/src/spdlog \
	&& curl --location $SPDLOG_TAR | tar --extract --gunzip --strip-components 1 --directory /usr/src/spdlog \
	&& ln --symbolic /usr/src/spdlog/include/spdlog /usr/local/include/spdlog \
\
	&& mkdir --parents --verbose /usr/src/concurrentqueue \
	&& curl --location $CONCURRENTQUEUE_TAR | tar --extract --gunzip --strip-components 1 --directory /usr/src/concurrentqueue \
	&& ln --symbolic /usr/src/concurrentqueue /usr/local/include/concurrentqueue \
\
	&& mkdir --parents --verbose /usr/src/fmtlib \
	&& curl --location $FMTLIB_TAR | tar --extract --gunzip --strip-components 1 --directory /usr/src/fmtlib \
	&& ln --symbolic /usr/src/fmtlib/include/fmt /usr/local/include/fmt \
\
	&& git clone --depth 1 --recurse-submodules --branch v1.29.1 $GRPC_GIT /usr/src/grpc \
	&& mkdir --parents --verbose /usr/src/grpc/cmake/build \
	# Build gRPC
	&& cd /usr/src/grpc/cmake/build \
	&& cmake -DgRPC_INSTALL=ON -DgRPC_BUILD_TESTS=OFF ../.. \
	&& make -j \
	&& make install \
\
	# Build URL fetcher
	&& mkdir --parents --verbose /usr/src/urlfetcher/build \
	&& curl --location $URLFETCHER_TAR | tar --extract --gunzip --strip-components 1 --directory /usr/src/urlfetcher \
	&& cd /usr/src/urlfetcher/build \
	&& cmake .. \
	&& make -j


# Step 2: Create runtime environment without build stuff installed in the above step
FROM debian:buster-slim

# Copy compiled binaries from previous step 1
COPY --from=0 /usr/src/urlfetcher/build/URLFetcher* /usr/local/bin/

# Install runtime dependencies
RUN apt update && apt install -y --no-install-recommends libcurl4
