# Live Chat via HTTP Polling (C++)

A simple multi-user chat app: a C++ server holds messages in memory, and a
React frontend polls it repeatedly so multiple browser tabs (or people)
appear to chat in real time, without WebSockets or threads.

## Files
- `chat_server.cpp` — listens on port 8081, keeps a list of messages in memory
- `chat_frontend.html` — React UI: shows messages, polls every 2s, sends new ones

## Endpoints
- `GET /messages` — returns every message so far, one per line, as plain text
- `POST /send` — body is the new message text; gets appended to the list

## Build & run

```bash
g++ chat_server.cpp -o chat_server
./chat_server
```

Then open `chat_frontend.html` in your browser — or better, open it in **two
separate tabs** (or two devices on the same network) to see the chat
actually work between them. Give each tab a different name.

## Why it "feels" live without WebSockets
Every open tab calls `GET /messages` every 2 seconds and replaces its
message list with whatever the server currently has. So when Tab A sends a
message, Tab B doesn't get pushed anything — it just happens to ask again
within 2 seconds and sees the new line. This is called **polling**, and
it's the simplest way to fake real-time updates over plain HTTP.

## Why the server can get away with being single-threaded
Each request (`GET /messages` or `POST /send`) is answered in a few
milliseconds. Even if two tabs technically arrive "at the same time," the
OS queues the second connection for a few milliseconds until the server
loops back to `accept()` — invisible to a human using a chat app. This is
fine for a small demo; a production chat server would use threads (or
`select()`/`epoll()`) to handle thousands of connections concurrently.

## Limitations (intentional, to keep this simple)
- Messages live only in memory — restarting the server clears the chat.
- No usernames/accounts — the "name" field is just a label you type in.
- Not encrypted — anyone on the network could read the plain HTTP traffic.
- A message containing `\n` could break the one-message-per-line format.

## Ideas to extend it
- Save messages to a file so history survives a server restart
- Add timestamps to each message
- Switch to a real push mechanism (Server-Sent Events or WebSockets) so
  messages appear instantly instead of within ~2 seconds
- Add basic usernames/session handling so people can't impersonate others
