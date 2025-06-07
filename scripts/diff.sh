#!/bin/sh
exec git diff --no-index -U100 "$@"
