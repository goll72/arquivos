#!/bin/sh
exec git diff --no-index --no-prefix -U100 "$@"
