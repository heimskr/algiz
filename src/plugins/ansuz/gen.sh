#!/bin/bash
base=$(dirname "$1")
echo "#include <cstdlib>" > "$2"
bin2c ansuz_index < "$base/index.t" | tail -n +2 >> "$2"
bin2c ansuz_css < "$base/style.css" | tail -n +2 >> "$2"
bin2c ansuz_message < "$base/message.t" | tail -n +2 >> "$2"
bin2c ansuz_unloaded < "$base/unloaded.t" | tail -n +2 >> "$2"
bin2c ansuz_load < "$base/load.t" | tail -n +2 >> "$2"
bin2c ansuz_edit_config < "$base/edit_config.t" | tail -n +2 >> "$2"
