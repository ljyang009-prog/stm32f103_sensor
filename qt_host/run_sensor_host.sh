#!/bin/bash

cd "$(dirname "$0")"
exec sg dialout -c "./sensor_host"
