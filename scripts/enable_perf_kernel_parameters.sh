#!/bin/sh

# Allow perf events for unprivileged users (less restrictive)
sudo sysctl -w kernel.perf_event_paranoid=-1

# Allow kernel pointers to be visible (needed for symbol resolution)
sudo sysctl -w kernel.kptr_restrict=0
