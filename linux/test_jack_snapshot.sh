#!/bin/sh
cd $(dirname "$0")

exec sqlite3 :memory: \
     -cmd '.import -csv test_jack_snapshot.csv snapshot' \
     -cmd '.read jack_db.sql' \
     "$@"

