# LOTA: Logging Over The Air

<!-- ![LOTA logo](https://github.com/kaizoku-oh/lota/blob/main/docs/image/logo.png) -->
<!-- ![](https://github.com/<OWNER>/<REPOSITORY>/workflows/<WORKFLOW_NAME>/badge.svg) -->
![GitHub Build workflow status](https://github.com/kaizoku-oh/lota/workflows/Build/badge.svg)
![GitHub release](https://img.shields.io/github/v/release/kaizoku-oh/lota)
![GitHub issues](https://img.shields.io/github/issues/kaizoku-oh/lota)
![GitHub top language](https://img.shields.io/github/languages/top/kaizoku-oh/lota)
![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)
![Twitter follow](https://img.shields.io/twitter/follow/kaizoku_ouh?style=social)

LOTA is a remote logging system based on ESP32, making remote logging and debugging in IoT and robotics applications much easier. It does this by connecting to the serial port of any embedded system and exposing a WIFI AP and a web dashboard showing the logging data.

## TODO ✅

* Start with an empty AP example on platformio ✅
* Add http server handling multiple connections
* Design a basic web dashboard with Bootstrap
* Add websocket server and integrate it with the http server
* Add js websocket client to the web dashboard
* Add counter task to send counter value periodically to the js websocket client
* Add uart task to handle received uart data and send it to the js websocket client
* Add ascii terminal view in the web dashboard to display received uart data.
* Add device status info view to the web dashboard

### How to open menuconfig:
```bash
# open platformio terminal and type this command
$ pio run -t menuconfig
```
