#!/bin/sh
JACK_SNAPSHOT=$(dirname "$0")/jack_snapshot.dynamic.host.elf
[ ! -x $JACK_SNAPSHOT ] && echo "need $JACK_SNAPSHOT" && exit 1

if [ $(whoami) == tom ]; then
	mkdir -p /tmp/jack
	JACK_CSV=/tmp/jack/$(hostname).csv
	echo "writing to $JACK_CSV" >&2
	$JACK_SNAPSHOT "$@" >$JACK_CSV
	DB=~tom/exo.sqlite3
	echo "importing to $DB" >&2
	echo ".import --csv --skip 1 $JACK_CSV jack" | sqlite3 $DB
else
	exec su tom "$0"
fi
