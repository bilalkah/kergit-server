# Project notes

To run the server and client applications, follow these steps:
```bash
bazel run --config=vanilla //app:server
bazel run //clients/web:web_server
```

Ensure that you have the necessary environment variables set in a `.env` file or your system environment for `SERVER_HOST` and `SERVER_PORT`.

