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

## 🛠️ Compilation

### Linux / macOS / Windows (with MinGW)

1. Clone the repository:

   ```sh
   git clone https://github.com/Evr5/ClipTransfer.git
   cd clip-transfer
   ```

2. Build with `make`:

    ```sh
    make
    ```

Note: Requires a C++17-compatible compiler and CMake.

## 🧩 Snapcraft (All Linux Distros with Snap Support)

ClipTransfer is available on the Snap Store and can be installed easily on **any Linux distribution** that supports [Snap](https://snapcraft.io/docs/installing-snapd):

```sh
sudo snap install clip-transfer
```

> 📦 This is the easiest way to get started on Linux without compiling manually.

## 🧪 Arch Linux Users

ClipTransfer is also available on the AUR. You can install it using [`yay`](https://github.com/Jguer/yay) or [`paru`](https://github.com/Morganamilo/paru):

```sh
yay -S clip-transfer        # Stable version (build from source)
yay -S clip-transfer-bin    # Stable version (precompiled binary)
yay -S clip-transfer-git    # Latest development version (from Git)
```

Choose the one that suits your preference:

- `clip-transfer`: stable and open-source (compiled locally)
- `clip-transfer-bin`: precompiled binary for quick install
- `clip-transfer-git`: latest development version from the main branch

## 🖥️ Windows Users

If you prefer not to compile the project, you can directly download and run the precompiled [clip-transfer.exe](https://github.com/Evr5/ClipTransfer/releases/download/v1.1.0/clip-transfer.exe).

> ⚠️ **Important**: On first launch, Windows Defender SmartScreen may block the app because it's not signed.
>
> - Click on **"More info"**  
> - Then click **"Run anyway"**

You may also see a Windows Firewall prompt asking for network access—this is expected. You can safely allow access to **Private networks** to enable communication between devices on the same LAN.

## 🔧 Tech Stack

- C++17
- Standalone ASIO
- TCP/IP (LAN only)
- CMake

## 📁 License

[MIT](./LICENSE)
