# Server CLI example, this runs 'main' from src/URLFetcherServer.cpp
FROM urlfetcher:v1
ENTRYPOINT ["/usr/local/bin/URLFetcherServer", "-vv", "--address=172.17.0.2:8000"]
