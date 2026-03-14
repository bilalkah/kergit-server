LiveKit docs › Other › Room service API

---

# Room service API

> Use LiveKit's built-in API to manage rooms, participants, and tracks in your backend.

## Overview

LiveKit has built-in APIs that let you manage rooms, participants, and tracks. These APIs are designed for use by your backend and are fully distributed across multiple nodes: any instance is capable of fulfilling requests about any room, participant, or track.

## Implementation details

LiveKit provides [server SDKs](https://docs.livekit.io/reference.md#server-apis) that make it easy to use these APIs. However, you can implement your own client using the details in the following sections.

### Endpoints

Server APIs are built with [Twirp](https://twitchtv.github.io/twirp/docs/intro.html), and differ from a traditional REST interface. Arguments are passed as JSON to an endpoint using the POST method.

The room service API is accessible via `/twirp/livekit.RoomService/<MethodName>`.

### Authorization header

All endpoints require a signed access token. This token should be set via HTTP header:

```
Authorization: Bearer <token>

```

LiveKit's server SDKs automatically include the above header.

### Post body

Twirp expects an HTTP POST request. The body of the request must be a JSON object (`application/json`) containing parameters specific to that request. Use an empty `{}` body for requests that don't require parameters.

For example, the following lists the room <room-name>. The token in this example requires the `roomList` [permission](https://docs.livekit.io/frontends/authentication/tokens.md#video-grant).

```shell
curl -X POST <your-host>/twirp/livekit.RoomService/ListRooms \
	-H "Authorization: Bearer <token-with-roomList>" \
	-H 'Content-Type: application/json' \
	-d '{ "names": ["<room-name>"] }'

```

When passing in parameters, the server accepts either `snake_case` or `camelCase` for keys.

## RoomService APIs

The RoomService API allows you to manage rooms, participants, tracks, and data.

### CreateRoom

Create a room with the specified settings. Requires `roomCreate` permission. This method is optional; a room is created automatically when the first participant joins it.

When creating a room, it's possible to configure automatic recording of the room or individually published tracks. See [Auto Egress](https://docs.livekit.io/transport/media/ingress-egress/egress/autoegress.md) docs.

Returns [Room](#room)

| Parameter | Type | Required | Description |
| name | string | yes | Name of the room. |
| empty_timeout | uint32 |  | Number of seconds to keep the room open if no one joins. Default is 300 seconds. |
| departure_timeout | uint32 |  | Number of seconds the room remains open after the last participant leaves. Default is 20 seconds. |
| max_participants | uint32 |  | Limit number of participants that can be in the room. Default is 0. |
| node_id | string |  | Override node selection (note: for advanced users). |
| metadata | string |  | Initial metadata to assign to the room. |
| egress | [RoomEgress](#roomegress) |  | Set the room to be recorded or streamed. |
| min_playout_delay | uint32 |  | Minimum playout delay in ms. Default is 0 ms. |
| max_playout_delay | uint32 |  | Maximum playout delay in ms. Default is 0 ms. |

### ListRooms

List rooms that are active/open. Requires `roomList` permission.

Returns List<[Room](#room)>

| Parameter | Type | Required | Description |
| names | List<string> |  | when passed in, only returns rooms matching one or more specified names |

### DeleteRoom

Delete an existing room. Requires `roomCreate` permission. DeleteRoom will forcibly disconnect all participants currently in the room.

| Parameter | Type | Required | Description |
| room | string | yes | name of the room |

### ListParticipants

List participants in a room, Requires `roomAdmin`

| Parameter | Type | Required | Description |
| room | string | yes | name of the room |

Returns List<[ParticipantInfo](#ParticipantInfo)>

### GetParticipant

Get information about a specific participant in a room, Requires `roomAdmin`

| Parameter | Type | Required | Description |
| room | string | yes | name of the room |
| identity | string | yes | identity of the participant |

Returns [ParticipantInfo](#ParticipantInfo)

### RemoveParticipant

Remove a participant from a room. Requires `roomAdmin`

On LiveKit Cloud, this API also revokes the participant's current access token, preventing them from immediately reconnecting with the same token. The participant needs to obtain a new token to rejoin the room. To learn more, see [Token revocation](https://docs.livekit.io/frontends/authentication/tokens.md#token-revocation).

| Parameter | Type | Required | Description |
| room | string | yes | name of the room |
| identity | string | yes | identity of the participant |

### MutePublishedTrack

Mute or unmute a participant's track. Requires `roomAdmin`

For privacy reasons, LiveKit server is configured by default to disallow the remote unmuting of tracks. To enable it, set [enable_remote_unmute](https://github.com/livekit/livekit/blob/4b630d2156265b9dc5ba6c6f786a408cf1a670a4/config-sample.yaml#L134) to true.

| Parameter | Type | Required | Description |
| room | string | yes | name of the room |
| identity | string | yes |  |
| track_sid | string | yes | sid of the track to mute |
| muted | bool | yes | set to true to mute, false to unmute |

### UpdateParticipant

Update information for a participant. Updating metadata will broadcast the change to all other participants in the room. Requires `roomAdmin`

When you update a participant's permissions, their current access token is revoked to prevent them from reconnecting with the old permissions. The server will automatically issue a new token to the connected participant. If they reconnect later, they must obtain a fresh token. To learn more, see [Token revocation](https://docs.livekit.io/frontends/authentication/tokens.md#token-revocation).

| Parameter | Type | Required | Description |
| room | string | yes |  |
| identity | string | yes |  |
| metadata | string |  | user-provided payload, an empty value is equivalent to a no-op |
| permission | [ParticipantPermission](#ParticipantPermission) |  | set to update the participant's permissions |

### UpdateSubscriptions

Subscribe or unsubscribe a participant from one or more published tracks. Requires `roomAdmin`.

As an admin, you can subscribe a participant to a track even if they do not have `canSubscribe` permission.

| Parameter | Type | Required | Description |
| room | string | yes |  |
| identity | string | yes |  |
| track_sids | List<string> | yes | list of sids of tracks |
| subscribe | bool | yes | set to true to subscribe and false to unsubscribe from tracks |

### UpdateRoomMetadata

Update room metadata. A metadata update will be broadcast to all participants in the room. Requires `roomAdmin`

| Parameter | Type | Required | Description |
| room | string | yes |  |
| metadata | string | yes | user-provided payload; opaque to LiveKit |

### SendData

Send data packets to one or more participants in a room. See the [data packet docs](https://docs.livekit.io/transport/data/packets.md) for more details and examples of client-side integration.

| Parameter | Type | Required | Description |
| room | string | yes | The room to send the packet in |
| data | bytes | yes | The raw packet bytes |
| kind | enum | yes | `reliable` or `lossy` delivery mode |
| destination_identities | List<[string]> | yes | List of participant identities to receive packet, leave blank to send the packet to everyone |
| topic | string |  | Topic for the packet |

## Types

The following types are used by the Room service API.

### Room

| Field | Type | Description |
| sid | string | Unique session ID. |
| name | string |  |
| empty_timeout | uint32 | Number of seconds the room remains open if no one joins. |
| departure_timeout | uint32 | Number of seconds the room remains open after the last participant leaves. |
| max_participants | uint32 | Maximum number of participants that can be in the room (0 = no limit). |
| creation_time | int64 | Unix timestamp (seconds since epoch) when this room was created. |
| turn_password | string | Password that the embedded TURN server requires. |
| metadata | string | User-specified metadata, opaque to LiveKit. |
| num_participants | uint32 | Number of participants currently in the room, excludes hidden participants. |
| active_recording | bool | True if a participant with `recorder` permission is currently in the room. |

### RoomAgentDispatch

A `RoomAgentDispatch` object can be passed to automatically [dispatch a named agent](https://docs.livekit.io/agents/server/agent-dispatch.md#explicit) to a room.

| Field | Type | Description |
| agent_name | string | Name of agent to dispatch to room. |
| metadata | string | User-specified metadata, opaque to LiveKit. |

### RoomConfiguration

A `RoomConfiguration` object can be passed when you create an [access token](https://docs.livekit.io/frontends/authentication/tokens.md#room-configuration) or [SIP dispatch rule](https://docs.livekit.io/telephony/accepting-calls/dispatch-rule.md), and can be used to automatically [dispatch an agent](https://docs.livekit.io/agents/server/agent-dispatch.md) to a room.

| Field | Type | Description |
| name | string |  |
| empty_timeout | int | Number of seconds the room remains open if no one joins. |
| departure_timeout | int | Number of seconds the room remains open after the last participant leaves. |
| max_participants | int | Maximum number of participants that can be in the room (0 = no limit). |
| egress | [RoomEgress](#roomegress) | If set, automatically start recording or streaming when room is created. |
| min_playout_delay | int | Minimum playout delay in ms. |
| max_playout_delay | int | Maximum playout delay in ms. |
| sync_streams | bool | If true, enable A/V sync for playout delays >200ms. |
| agents | List<[[RoomAgentDispatch](#roomagentdispatch)]> | One or more agents to be dispatched to the room on connection. |

### ParticipantInfo

| Field | Type | Description |
| sid | string | server-generated identifier |
| identity | string | user-specified unique identifier for the participant |
| name | string | name given to the participant in access token (optional) |
| state | [ParticipantInfo_State](#ParticipantInfo-State) | connection state of the participant |
| tracks | List<[TrackInfo](#TrackInfo)> | tracks published by the participant |
| metadata | string | user-specified metadata for the participant |
| joined_at | int64 | timestamp when the participant joined room |
| permission | ParticipantPermission | permission given to the participant via access token |
| is_publisher | bool | true if the participant has published media or data |

### TrackInfo

| Field | Type | Description |
| sid | string | server-generated identifier |
| type | [TrackType](#TrackType) | audio or video |
| source | [TrackSource](#TrackSource) | source of the Track |
| name | string | name given at publish time (optional) |
| mime_type | string | mime type of codec used |
| muted | bool | true if track has been muted by the publisher |
| width | uint32 | original width of video (unset for audio) |
| height | uint32 | original height of video (unset for audio) |
| simulcast | bool | true if track is simulcasted |
| disable_dtx | bool | true if DTX is disabled |
| layers | List<[VideoLayer](#VideoLayer)> | simulcast or SVC layers in the track |

### ParticipantPermission

Permissions that can be granted or revoked for a participant. For update operations, only include permissions you want to update.

| Name | Type | Description |
| `can_subscribe` | bool | Allow the participant to subscribe to other tracks in the room. |
| `can_publish` | bool | Allow the participant to publish new tracks to the room. |
| `can_publish_data` | bool | Allow the participant to publish data to the room. |
| `can_publish_sources` | [TrackSource](#tracksource)[] | Allow the participant to publish specific track sources to the room. |
| `hidden` | bool | Hide the participant from others in the room. |
| `can_update_metadata` | bool | Allow the participant to update their own metadata and attributes. |
| `can_subscribe_metrics` | bool | Allow the participant to subscribe to metrics. |

### VideoLayer

Represents a single simulcast layer in a [Track](#TrackInfo)

| Field | Type | Description |
| quality | [VideoQuality](#VideoQuality) | high, medium, or low |
| width | uint32 |  |
| height | uint32 |  |

### RoomEgress

Used to specify Auto Egress settings when creating a room.

| Field | Type | Description |
| room | [RoomCompositeEgressRequest](https://docs.livekit.io/transport/media/ingress-egress/egress/composite-recording.md#roomcomposite-egress) | set to start a Room Composite Egress when participant joins, same parameters as `StartCompositeEgress` API |
| tracks | [AutoTrackEgress](#AutoTrackEgress) | set to export each published track automatically |

### AutoTrackEgress

| Field | Type | Description |
| filepath | string | template to use for file name. see [Egress filenames](https://docs.livekit.io/transport/media/ingress-egress/egress/outputs.md#filename-templating) |
| disable_manifest | bool | when set to true, disables uploading of JSON manifests |
| s3 | [S3Upload](https://github.com/livekit/protocol/blob/85bf30570f0f4ce1d06e40cd98222a6350013315/livekit_egress.proto#L112) | set when uploading to S3 |
| gcp | [GCPUpload](https://github.com/livekit/protocol/blob/85bf30570f0f4ce1d06e40cd98222a6350013315/livekit_egress.proto#L121) | set when uploading to Google Cloud Storage |
| azure | [AzureBlobUpload](https://github.com/livekit/protocol/blob/85bf30570f0f4ce1d06e40cd98222a6350013315/livekit_egress.proto#L126) | set when uploading to Azure Blob Storage |

### ParticipantInfo_State

Enum, valid values:

- JOINING: 0
- JOINED: 1
- ACTIVE: 2
- DISCONNECTED: 3

### TrackSource

Enum representing the source of a track. Valid values are as follows:

- UNKNOWN: 0
- CAMERA: 1
- MICROPHONE: 2
- SCREEN_SHARE: 3
- SCREEN_SHARE_AUDIO: 4

### TrackType

Enum, valid values:

- AUDIO: 0
- VIDEO: 1

### VideoQuality

Enum, valid values:

- LOW: 0
- MEDIUM: 1
- HIGH: 2
- OFF: 3

---

This document was rendered at 2026-03-09T21:04:43.495Z.
For the latest version of this document, see [https://docs.livekit.io/reference/other/roomservice-api.md](https://docs.livekit.io/reference/other/roomservice-api.md).

To explore all LiveKit documentation, see [llms.txt](https://docs.livekit.io/llms.txt).