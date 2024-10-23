#!/bin/bash
export z8lclocks=500
pdp8v/driver/loadmod.sh
exec ./pipan8l -z8l "$@"
