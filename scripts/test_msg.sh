#!/bin/bash
set -euo pipefail

FMT='{
  "next_batch": "1",
  "rooms": {
    "join": {
      "room": {
        "timeline": {
          "events": [{
            "sender": "%s",
            "type": "m.room.message",
            "content": {
              "msgtype": "m.text",
              "body": "%s"
            }
          }]
        }
      }
    }
  }
}\n'

sender=$1
msg=$2

printf "$FMT" "$sender" "$msg" | jq --compact-output
