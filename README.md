# ClipTransfer

**ClipTransfer** is a lightweight C++ application that allows seamless sharing of clipboard or text snippets between devices on the same local network (LAN). Built with standalone ASIO, it automatically determines whether to act as a client or server depending on availability—no manual setup required.

## ✨ Features

- 📡 Automatic server/client role detection
- 🧠 Intelligent connection logic with ASIO
- 🔄 Instant LAN-based text transfer
- 🖥️ Cross-platform compatible (Linux, Windows, macOS)
- 💡 Ideal for clipboard sharing, text syncing, quick notes

## 🚀 How it works

1. On launch, the app tries to connect as a client.
2. If no server is found, it becomes the server.
3. Once connected, text can be sent from one device to another instantly.

## 🔧 Tech Stack

- C++17
- Standalone ASIO
- TCP/IP (LAN only)
- CMake

## 📦 Future Plans

- Clipboard auto-sync
- Bi-directional messaging
- GUI support (Qt or Electron)

## 📁 License

MIT
