# TASK: URL Fetcher Service

Create a service which receives a stream of URLs, fetches the contents of those URLs and returns the contents to the caller.
The fetch operation must be done asynchronously to enable the caller to send additional fetch requests freely.

Authentication and authorization are handled outside of this service.

You should:
* Design the bidirectional streaming API using gRPC (https://grpc.io)
* Implement the API
* Make it as close to production ready as possible

The API should:
* Use a single streaming connection for the whole session
* Receive and send requests/responses asynchronously
* Know when the client will not send any more URLs
* Close the connection from server-side only after all the results have been sent

You can select the programming language.
We will run your service in Kubernetes, but we don’t expect you to create any deployment descriptors, a Dockerfile is sufficient.

When you are ready, please send us the source code along with any tests or documentation you might create.
We will go through your response and get back to you.

We appreciate that your time is valuable, so we don’t expect you to use more than a few hours to finish this exercise (but you can use more time if you want to).

Thank you!
