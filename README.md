# Project notes

To run the server and client applications, follow these steps:
```bash
bazel run --config=vanilla //app:server
bazel run //clients/web:web_server
```

Ensure that you have the necessary environment variables set in a `.env` file or your system environment for `SERVER_HOST` and `SERVER_PORT`.

server/
├─ core/                              # process lifecycle, config, app factory
│  ├─ ChatServerApp.{h,cpp}           # owns IApp, start/stop/join, signal handling
│  ├─ AppFactory.{h,cpp}              # builds PlainApp or TLSApp from TlsConfig
│  ├─ ServerConfig.{h,cpp}            # host, ports, flags, env loading
│  ├─ IApp.h                          # type-erased app interface
│  ├─ IWebSocket.h                    # type-erased websocket interface
│  ├─ PlainApp.{h,cpp}                # wraps uWS::App, wires IApp
│  ├─ TLSApp.{h,cpp}                  # wraps uWS::SSLApp, cert options
│  └─ Types.h                         # PerSocketData typedefs shared across net/core
│
├─ net/                               # websocket plumbing only (no business rules)
│  ├─ WebSocketServer.{h,cpp}         # .upgrade .open .message .close registration
│  ├─ ConnectionManager.{h,cpp}       # track connections, map<connId, WS*>
│  ├─ middleware/
│  │   └─ RateLimiter.{h,cpp}         # per-IP or per-socket throttling
│  ├─ WSAdapter.{h}                   # WSAdapter<false/true> → IWebSocket
│  └─ PerSocketData.h                 # user_id, authenticated, current_channel, etc.
│
├─ app/                               # business logic (transport-agnostic)
│  ├─ Dispatcher.{h,cpp}              # routes type → ICommand
│  ├─ Command.{h}                     # interface: execute(ctx, payload) → Result DTO
│  ├─ CommandRegistry.{h,cpp}         # binds all commands into Dispatcher
│  ├─ services/                       # use-cases (how to do things)
│  │   ├─ HubService.{h,cpp}
│  │   ├─ ChannelService.{h,cpp}
│  │   ├─ MessageService.{h,cpp}
│  │   └─ AuthService.{h,cpp}
│  ├─ commands/                       # thin adapters (what to do)
│  │   ├─ CreateHubCommand.{h,cpp}
│  │   ├─ CreateChannelCommand.{h,cpp}
│  │   ├─ SendMessageCommand.{h,cpp}
│  │   ├─ ListCommand.{h,cpp}
│  │   ├─ UsersCommand.{h,cpp}
│  │   ├─ AuthenticateCommand.{h,cpp}
│  │   └─ PingCommand.{h,cpp}
│  ├─ validation/
│  │   └─ MessageValidator.{h,cpp}    # JSON shape, field checks
│  ├─ policy/
│  │   └─ SecurityPolicy.{h,cpp}      # permissions, access rules
│  ├─ limits/
│  │   └─ RateLimiter.{h,cpp}         # per-user or per-channel limits (optional)
│  └─ pubsub/
│      └─ PubSub.{h,cpp}              # topic → set<ConnId>; later swap to Redis/NATS
│
├─ infra/                             # infrastructure adapters
│  └─ persistence/
│      ├─ Database.{h,cpp}            # connection pool or client
│      ├─ HubRepository.{h,cpp}
│      ├─ ChannelRepository.{h,cpp}
│      └─ UserRepository.{h,cpp}
│
├─ domain/                            # pure models, no IO or uWS
│  ├─ Hub.{h,cpp}
│  ├─ Channel.{h,cpp}
│  ├─ User.{h,cpp}
│  └─ Message.{h,cpp}
│
├─ utils/
│  ├─ EnvLoader.{h,cpp}
│  ├─ Logger.{h,cpp}
│  ├─ JsonUtils.{h,cpp}
│  ├─ TimeUtils.{h,cpp}               # steadyclock helpers for timeouts, lat
│  ├─ TlsConfig.{h,cpp}               # validate cert/key paths
│  ├─ OriginAllowlist.{h,cpp}
│  └─ crypto/
│      ├─ Hmac.{h,cpp}
│      └─ Crypto.{h,cpp}              # encrypt/decrypt helpers
│
├─ main_plain.cc                      # optional entry for non-TLS
├─ main_tls.cc                        # optional entry for TLS
└─ BUILD
