LiveKit docs › Understanding LiveKit › Rooms, participants, & tracks › Webhooks & events

---

# Webhooks & events

> Configure webhooks and handle events to monitor and respond to changes in rooms, participants, and tracks.

## Overview

LiveKit provides two mechanisms for monitoring and responding to changes in rooms, participants, and tracks:

- **Webhooks**: Server-side notifications sent to your backend when room and participant events occur.
- **Events**: Client-side event system in the SDKs that allows your application to respond to state changes in realtime.

These mechanisms enable you to build reactive applications that stay synchronized with room state and respond to changes as they happen.

## Managing webhooks

Webhooks enable your backend to receive realtime notifications about room and participant events. Use webhooks to integrate LiveKit with your application logic, trigger actions, and maintain state synchronization.

### Configuration

You can create and configure webhooks using LiveKit Cloud or self-hosted deployments.

#### LiveKit Cloud

Create a webhook in LiveKit Cloud:

1. Sign in to LiveKit Cloud, select **Settings** → [**Webhooks**](https://cloud.livekit.io/projects/p_/settings/webhooks).
2. Select **Create new webhook**.
3. Enter your webhook **Name** and **URL**, and select an API key for **Signing API key**.
4. Select **Create**.

After you create a webhook, you can test it by sending a test event to the webhook URL:

1. Select **Actions** → **Send a test event**.
2. Select the [webhook event](#webhook-events) to send.

#### Self-hosted deployments

For self-hosted deployments, webhooks can be enabled by setting the `webhook` section in your config.

```yaml
webhook:
  # The API key to use in order to sign the message
  # This must match one of the keys LiveKit is configured with
  api_key: 'api-key-to-sign-with'
  urls:
    - 'https://yourhost'

```

#### Webhooks in Egress

You can also configure extra webhooks inside [Egress requests](https://docs.livekit.io/reference/other/egress/api.md#WebhookConfig).

### Receiving webhooks

Webhook requests are HTTP POST requests sent to URLs that you specify in your config or LiveKit Cloud dashboard. A `WebhookEvent` is encoded as JSON and sent in the body of the request.

The `Content-Type` header of the request is `application/webhook+json`. Your web server must be configured to receive payloads with this content type.

To ensure webhook requests are coming from LiveKit, these requests have an `Authorization` header containing a signed JWT token. The token includes a sha256 hash of the payload.

LiveKit's server SDKs provide webhook receiver libraries for validating and decoding the payload.

**Node.js**:

```typescript
import { WebhookReceiver } from 'livekit-server-sdk';

const receiver = new WebhookReceiver('apikey', 'apisecret');

// In order to use the validator, WebhookReceiver must have access to the raw
// POSTed string (instead of a parsed JSON object). If you are using express
// middleware, ensure that `express.raw` is used for the webhook endpoint
// app.use(express.raw({type: 'application/webhook+json'}));

app.post('/webhook-endpoint', async (req, res) => {
  // Event is a WebhookEvent object
  const event = await receiver.receive(req.body, req.get('Authorization'));
});

```

---

**Go**:

```go
import (
  "github.com/livekit/protocol/auth"
  "github.com/livekit/protocol/livekit"
  "github.com/livekit/protocol/webhook"
)

func ServeHTTP(w http.ResponseWriter, r *http.Request) {
  authProvider := auth.NewSimpleKeyProvider(
    apiKey, apiSecret,
  )
  // Event is a livekit.WebhookEvent{} object
  event, err := webhook.ReceiveWebhookEvent(r, authProvider)
  if err != nil {
    // Could not validate, handle error
    return
  }
  // Consume WebhookEvent
}

```

---

**Java**:

```java
import io.livekit.server.*;

WebhookReceiver webhookReceiver = new WebhookReceiver("apiKey", "secret");

// postBody is the raw POSTed string.
// authHeader is the value of the "Authorization" header in the request.
LivekitWebhook.WebhookEvent event = webhookReceiver.receive(postBody, authHeader);

// Consume WebhookEvent

```

### Delivery and retries

Webhooks are HTTP requests initiated by LiveKit and sent to your backend. Due to the protocol's push-based nature, there are no guarantees around delivery.

LiveKit aims to mitigate transient failures by retrying a webhook request multiple times. Each message undergoes several delivery attempts before being abandoned. If multiple events are queued for delivery, LiveKit properly sequences them, only delivering newer events after older ones have been delivered or abandoned.

### Webhook events

The following table lists all webhook events and their payload fields. In addition to the fields in the **Payload fields** column, all webhook events include the following fields:

- `id`: UUID identifying the event
- `createdAt`: UNIX timestamp in seconds
- `event`: Event name

| Event name | Description | Payload fields |
| `room_started` | The first participant joins an empty room. | [room](https://docs.livekit.io/reference/other/roomservice-api.md#room) |
| `room_finished` | Room closes. Either by `room.close()` or the last participant left and the room's empty timeout expired. | [room](https://docs.livekit.io/reference/other/roomservice-api.md#room) |
| `participant_joined` | Participant joins (media connection established). This event is fired after the participant's state changes to `active`. To learn more, see [Connection events](#connection-events). | [room](https://docs.livekit.io/reference/other/roomservice-api.md#room), [participant](https://docs.livekit.io/reference/other/roomservice-api.md#participantinfo) |
| `participant_left` | Participant leaves a room and all cleanup processes are complete. | [room](https://docs.livekit.io/reference/other/roomservice-api.md#room), [participant](https://docs.livekit.io/reference/other/roomservice-api.md#participantinfo) |
| `participant_connection_aborted` | Participant connection aborts unexpectedly. This event can be fired after a signal connection is established if the media connection fails. See [Connection events](#connection-events). | [room](https://docs.livekit.io/reference/other/roomservice-api.md#room), [participant](https://docs.livekit.io/reference/other/roomservice-api.md#participantinfo) |
| `track_published` | Participant publishes a track.

The `room` and `participant` objects in the payload for this event only include SID, name, and identity. | room, participant, [track](https://docs.livekit.io/reference/other/roomservice-api.md#trackinfo) |
| `track_unpublished` | Participant unpublishes a track.

The `room` and `participant` objects in the payload for this event only include SID, name, and identity. | room, participant, [track](https://docs.livekit.io/reference/other/roomservice-api.md#trackinfo) |
| `egress_started` | Recording/streaming (egress) starts. | [egressInfo](https://docs.livekit.io/reference/other/egress/api.md#egressinfo) |
| `egress_updated` | Egress updates (for example, file size change). | [egressInfo](https://docs.livekit.io/reference/other/egress/api.md#egressinfo) |
| `egress_ended` | Egress ends. | [egressInfo](https://docs.livekit.io/reference/other/egress/api.md#egressinfo) |
| `ingress_started` | Ingress (external stream) starts. | [ingressInfo](https://docs.livekit.io/reference/other/ingress/api.md#ingressinfo) |
| `ingress_ended` | Ingress ends. | [ingressInfo](https://docs.livekit.io/reference/other/ingress/api.md#ingressinfo) |

## Connection events

Connecting to a room happens in two phases:

- **Signal connection**: Establishes the initial signaling channel to exchange metadata and control messages.
- **Media connection**: Establishes the connection that allows the exchange of realtime media and data.

Media can't be exchanged until both phases are successfully established. In some cases, the signal connection might succeed but the media connection can still fail, preventing audio or video from being transmitted.

Webhook and SDK events represent different parts of this lifecycle and don't always share names or fire at the same time. For example, the `ParticipantConnected` [SDK event](#sdk-events) indicates that a participant has established a signal connection. There is no corresponding webhook event at this phase.

A participant is considered fully connected after their media connection is established. At that point, their state changes to `active`, and the `participant_joined` [webhook event](#webhook-events) is emitted. This is the same as the `ParticipantActive` SDK event.

> ℹ️ **ParticipantActive availability**
> 
> The `ParticipantActive` event is emitted when a participant's state transitions to `active`. This indicates that the media connection is established and the participant can publish and subscribe to tracks.
> 
> This event is currently available only in the JavaScript SDK. In other SDKs, monitor participant state changes and check for the active state to determine when media connectivity is established. See the SDK-specific [documentation](https://docs.livekit.io/reference.md#livekit-sdks) for details.

## Handling events

The LiveKit SDKs use events to notify your application of changes taking place in the room.

There are two kinds of events, **room events** and **participant events**. Room events are emitted from the main `Room` object, reflecting any change in the room. Participant events are emitted from each `Participant`, when that specific participant has changed.

Room events are generally a superset of participant events. As you can see, some events are fired on both `Room` and `Participant`—this is intentional. This duplication is designed to make it easier to componentize your application. For example, if you have a UI component that renders a participant, it should only listen to events scoped to that participant.

### Declarative UI

Event handling can be quite complicated in a realtime, multi-user system. Participants could be joining and leaving, each publishing tracks or muting them. To simplify this, LiveKit offers built-in support for [declarative UI](https://alexsidorenko.com/blog/react-is-declarative-what-does-it-mean/) for most platforms.

With declarative UI you specify how the UI should look given a particular state, without having to worry about the sequence of transformations to apply. Modern frameworks are highly efficient at detecting changes and rendering only what's changed.

**React**:

There are a few hooks and components that make working with React much simpler.

- [useParticipant](https://docs.livekit.io/reference/components/react/hook/useparticipants.md) - maps participant events to state
- [useTracks](https://docs.livekit.io/reference/components/react/hook/usetracks.md) - returns the current state of the specified audio or video track
- [VideoTrack](https://docs.livekit.io/reference/components/react/component/videotrack.md) - React component that renders a video track
- [RoomAudioRenderer](https://docs.livekit.io/reference/components/react/component/roomaudiorenderer.md) - React component that renders the sound of all audio tracks

```tsx
const Stage = () => {
  const tracks = useTracks([Track.Source.Camera, Track.Source.ScreenShare]);
  return (
    <SessionProvider session={/* ... */}>
      {/* Render all video */}
      {tracks.map((track) => (
        <VideoTrack key={track.sid} trackRef={track} />
      ))}
      {/* ...and all audio tracks. */}
      <RoomAudioRenderer />
    </SessionProvider>
  );
};

function ParticipantList() {
  const participants = useParticipants();
  return (
    <ParticipantLoop participants={participants}>
      <ParticipantName />
    </ParticipantLoop>
  );
}

```

---

**SwiftUI**:

Most core objects in the Swift SDK, including `Room`, `Participant`, and `TrackReference`, implement the `ObservableObject` protocol so they are ready-made for use with SwiftUI.

For the simplest integration, the [Swift Components SDK](https://github.com/livekit/components-swift) contains ready-made utilities for modern SwiftUI apps, built on `.environmentObject`:

- `RoomScope` - creates and (optionally) connects to a `Room`, leaving upon dismissal
- `ForEachParticipant` - iterates each `Participant` in the current room, automatically updating
- `ForEachTrack` - iterates each `TrackReference` on the current participant, automatically updating

```swift
struct MyChatView: View {
    var body: some View {
        RoomScope(url: /* URL */,
                  token: /* Token */,
                  connect: true,
                  enableCamera: true,
                  enableMicrophone: true) {
            VStack {
                ForEachParticipant { _ in
                    VStack {
                        ForEachTrack(filter: .video) { _ in
                            MyVideoView()
                                .frame(width: 100, height: 100)
                        }
                    }
                }
            }
        }
    }
}

struct MyVideoView: View {
  @EnvironmentObject private var trackReference: TrackReference

  var body: some View {
      VideoTrackView(trackReference: trackReference)
        .frame(width: 100, height: 100)
  }
}

```

---

**Android Compose**:

The `Room` and `Participant` objects have built-in `Flow` support. Any property marked with a `@FlowObservable` annotation can be observed with the `flow` utility method. It can be used like this:

```kotlin
@Composable
fun Content(
  room: Room
) {
  val remoteParticipants by room::remoteParticipants.flow.collectAsState(emptyMap())
  val remoteParticipantsList = remoteParticipants.values.toList()
  LazyRow {
      items(
          count = remoteParticipantsList.size,
          key = { index -> remoteParticipantsList[index].sid }
      ) { index ->
          ParticipantItem(room = room, participant = remoteParticipantsList[index])
      }
  }
}

@Composable
fun ParticipantItem(
    room: Room,
    participant: Participant,
) {
  val videoTracks by participant::videoTracks.flow.collectAsState(emptyList())
  val subscribedTrack = videoTracks.firstOrNull { (pub) -> pub.subscribed } ?: return
  val videoTrack = subscribedTrack.second as? VideoTrack ?: return

  VideoTrackView(
      room = room,
      videoTrack = videoTrack,
  )
}

```

---

**Flutter**:

Flutter supports [declarative UI](https://docs.flutter.dev/get-started/flutter-for/declarative) by default. The LiveKit SDK notifies changes in two ways:

- ChangeNotifier - generic notification of changes. This is useful when you are building reactive UI and only care about changes that may impact rendering
- EventsListener<Event> - listener pattern to listen to specific events (see [events.dart](https://github.com/livekit/client-sdk-flutter/blob/main/lib/src/events.dart))

```dart
class RoomWidget extends StatefulWidget {
  final Room room;

  RoomWidget(this.room);

  @override
  State<StatefulWidget> createState() {
    return _RoomState();
  }
}

class _RoomState extends State<RoomWidget> {
  late final EventsListener<RoomEvent> _listener = widget.room.createListener();

  @override
  void initState() {
    super.initState();
    // used for generic change updates
    widget.room.addListener(_onChange);

    // Used for specific events
    _listener
      ..on<RoomDisconnectedEvent>((_) {
        // handle disconnect
      })
      ..on<ParticipantConnectedEvent>((e) {
        print("participant joined: ${e.participant.identity}");
      })
  }

  @override
  void dispose() {
    // Be sure to dispose listener to stop listening to further updates
    _listener.dispose();
    widget.room.removeListener(_onChange);
    super.dispose();
  }

  void _onChange() {
    // Perform computations and then call setState
    // setState will trigger a build
    setState(() {
      // your updates here
    });
  }

  @override
  Widget build(BuildContext context) => Scaffold(
    // Builds a room layout with a main participant in the center, and a row of
    // participants at the bottom.
    // ParticipantWidget is located here: https://github.com/livekit/client-sdk-flutter/blob/main/example/lib/widgets/participant.dart
    body: Column(
      children: [
        Expanded(
            child: participants.isNotEmpty
                ? ParticipantWidget.widgetFor(participants.first)
                : Container()),
        SizedBox(
          height: 100,
          child: ListView.builder(
            scrollDirection: Axis.horizontal,
            itemCount: math.max(0, participants.length - 1),
            itemBuilder: (BuildContext context, int index) => SizedBox(
              width: 100,
              height: 100,
              child: ParticipantWidget.widgetFor(participants[index + 1]),
            ),
          ),
        ),
      ],
    ),
  );
}

```

### SDK events

This table captures a consistent set of events that are available across platform SDKs. In addition to what's listed here, there may be platform-specific events on certain platforms.

| Event | Description | Event type |
| `ParticipantConnected` | A remote participant joins the room _after_ a local participant joins. | Room |
| `ParticipantActive` | A remote participant's state changes to `active`. An active state means the participant has established a media connection. This event is only available in the [JavaScript SDK](https://docs.livekit.io/reference/components/javascript/index.html.md). For other SDKs, you must monitor participant state for the state to change to `active`. To learn more, see [Connection events](#connection-events). | Room |
| `ParticipantDisconnected` | A remote participant leaves the room. | Room |
| `Reconnecting` | The connection to the server has been interrupted and it's attempting to reconnect. | Room |
| `Reconnected` | Reconnection succeeded and the session is active again. | Room |
| `Disconnected` | Disconnected from the room because the room closed or there was an unrecoverable failure. | Room |
| `TrackPublished` | A new track is published to the room after the local participant has joined. | Room, Participant |
| `TrackUnpublished` | A remote participant has unpublished a track. | Room, Participant |
| `TrackSubscribed` | The local participant has successfully subscribed to a remote track. | Room, Participant |
| `TrackUnsubscribed` | A previously subscribed track has been unsubscribed. | Room, Participant |
| `TrackMuted` | A track was muted; fires for both local and remote tracks. | Room, Participant |
| `TrackUnmuted` | A track was unmuted; fires for both local and remote tracks. | Room, Participant |
| `LocalTrackPublished` | A local track was published successfully. | Room, Participant |
| `LocalTrackUnpublished` | A local track was unpublished. | Room, Participant |
| `ActiveSpeakersChanged` | The set of active speakers has changed. | Room |
| `IsSpeakingChanged` | The current participant's speaking status has changed. | Participant |
| `ConnectionQualityChanged` | Connection quality for a participant has changed. | Room, Participant |
| `ParticipantAttributesChanged` | A participant's attributes were updated. | Room, Participant |
| `ParticipantMetadataChanged` | A participant's metadata was updated. | Room, Participant |
| `ParticipantNameChanged` | A participant's display name has changed. | Room, Participant |
| `RoomMetadataChanged` | Room metadata has changed. | Room |
| `DataReceived` | Data received from another participant or the server. | Room, Participant |
| `TrackStreamStateChanged` | A subscribed track's stream state changed (for example, paused due to bandwidth issues); resumes automatically when conditions allow. | Room, Participant |
| `TrackSubscriptionPermissionChanged` | Track-level subscription permission for the current participant has changed. | Room, Participant |
| `ParticipantPermissionsChanged` | The current participant's permissions have changed. | Room, Participant |

---

This document was rendered at 2026-03-09T21:06:01.231Z.
For the latest version of this document, see [https://docs.livekit.io/intro/basics/rooms-participants-tracks/webhooks-events.md](https://docs.livekit.io/intro/basics/rooms-participants-tracks/webhooks-events.md).

To explore all LiveKit documentation, see [llms.txt](https://docs.livekit.io/llms.txt).