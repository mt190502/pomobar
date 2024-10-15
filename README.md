# Pomobar: A waybar compatible pomodoro timer (WIP)

# What is Pomobar?

Pomobar is a pomodoro timer that is compatible with waybar. This project is a work in progress.

# How does Pomobar work?

Pomobar is a client-server application. The server is responsible for keeping track of the time and the client is responsible for sending commands to the server. Connection is established via a unix socket. The server/client communication is done via raw bytes now.

# How to use Pomobar?

1. Clone the repository

```bash
git clone <this-repo>
```

2. Build the project

```bash
make
```

3. Run server in background

```bash
./pomobar-server &; disown
```

4. Run client for connection test

```bash
./pomobar-client status
```

5. If all is well, add the following to your waybar config

```json
{
    ...
    "modules-<left|center|right>": [..., "custom/pomobar", ...],
    ...

    "custom/pomobar": {
        "format"                   : "{}",
        "interval"                 : 1,
        "exec"                     : "/path/to/pomobar-client status",
        "on-click"                 : "/path/to/pomobar-client pause",
        "on-click-middle"          : "/path/to/pomobar-client reset",
        "return-type"              : "json"
    },
}
```

6. Reload waybar

```bash
killall -q waybar
waybar &; disown
```

# Screenshots

![image](https://github.com/user-attachments/assets/442cbb4f-74ad-4fc5-983e-7e7b0527cc52)
![image](https://github.com/user-attachments/assets/fdbfd9b7-d019-4d95-b799-fa9d9c3ed82f)
![image](https://github.com/user-attachments/assets/e035c758-882f-478a-9caf-d01ed42fc3c1)



# To-Do List

-   [x] Basic timer functionality
-   [x] Waybar integration
-   [ ] Config file support
    -   [ ] Configurable timer
    -   [ ] Configurable waybar module
    -   [ ] Configurable work/break command/script/icon
-   [ ] Optimizations
    -   [ ] Use JSON for server/client communication
    -   [ ] Use a more efficient way to keep track of time
    -   [ ] Code cleanup
-   [ ] Documentation
-   [ ] Error handling
-   [ ] Add more features
