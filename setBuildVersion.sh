#!/bin/bash
sed -i '.bak' "s/.*define VERSION_COMMIT_COUNT.*/#define VERSION_COMMIT_COUNT \"$(git rev-list --count HEAD)\"/" ./futurerestore/config.h 2>/dev/null || sed -i "s/.*VERSION_COMMIT_COUNT.*/#define VERSION_COMMIT_COUNT \"$(git rev-list --count HEAD)\"/" ./futurerestore/config.h 2>/dev/null
sed -i '.bak' "s/.*define VERSION_COMMIT_SHA.*/#define VERSION_COMMIT_SHA \"$(git rev-parse HEAD)\"/" ./futurerestore/config.h 2>/dev/null || sed -i "s/.*VERSION_COMMIT_SHA.*/#define VERSION_COMMIT_SHA \"$(git rev-parse HEAD)\"/" ./futurerestore/config.h 2>/dev/null
