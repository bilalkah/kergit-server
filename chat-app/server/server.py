# server/server.py
import asyncio
import websockets
import json
from collections import defaultdict

rooms = defaultdict(set)  # room_name -> set of websockets

async def handler(ws, path):
    room = None
    try:
        async for msg in ws:
            data = json.loads(msg)
            if data["type"] == "join":
                room = data["room"]
                rooms[room].add(ws)
                await ws.send(json.dumps({"type": "joined", "room": room}))
            elif data["type"] == "chat":
                if room:
                    msg = json.dumps({"type": "chat", "sender": data["sender"], "text": data["text"]})
                    await asyncio.gather(*[client.send(msg) for client in rooms[room] if client != ws])
    finally:
        if room:
            rooms[room].remove(ws)

async def main():
    async with websockets.serve(handler, "0.0.0.0", 8765):
        await asyncio.Future()

if __name__ == "__main__":
    asyncio.run(main())