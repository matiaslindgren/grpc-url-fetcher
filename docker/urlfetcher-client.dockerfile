# Client CLI example, this runs 'main' from src/URLFetcherClient.cpp
FROM urlfetcher:v1
ENTRYPOINT ["/usr/local/bin/URLFetcherClient", "-vv", "--address=172.17.0.2:8000"]
