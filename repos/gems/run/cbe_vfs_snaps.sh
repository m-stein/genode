#!/bin/bash

echo "--- Automated CBE testing ---"

RW_TEST=0
KEY_TEST=1
CREATE_SNAPSHOT_TEST=1
CREATE_SNAPSHOT_DIRTY_TEST=1
WRITE_1_BLOCK_TEST=1
WRITE_CREATE_TEST=1
LIST_SNAPSHOTS_TEST=1
DISCARD_SNAPSHOT_TEST=1

test_key() {
	local cbe_dir="$1"

	echo "Write key to CBE"
	echo -n "23 All your base are belong to usxxx" > $cbe_dir/control/key
}

test_create_snapshot() {
	local cbe_dir="$1"

	echo "Create snapshot"
	echo true > $cbe_dir/control/create_snapshot
}

test_rw() {
	local data_file="$1"
	[ "$data_file" = "" ] && exit 1

	# read complete current snapshot
	echo "Read '$data_file'"
	dd if=$data_file of=/dev/null bs=4096

	# write complete current snapshot
	echo "Write '$data_file'"
	dd if=$data_file of=/dev/null bs=4096
}

produce_pattern() {
	[ "$pattern" = "" ] && exit 1

	seq -s "$pattern" 4096
}

test_write_1() {
	local data_file="$1"
	local offset=$2

	local pattern="1"
	local pattern_file="/tmp/pattern.$pattern"
	# create pattern file because using '| dd' leads to a one byte short write
	produce_pattern "$pattern" > $pattern_file
	dd bs=4096 count=1 if=/tmp/pattern.$pattern of=$data_file seek=$offset || exit 1
	rm $pattern_file
}

test_list_snapshots() {
	local cbe_dir="$1"

	echo "List content of '$cbe_dir'"
	ls -l $cbe_dir/snapshots
}

test_discard_snapshot() {
	local cbe_dir="$1"
	local snap_id=$2

	echo "Discard snapshot with id: $snap_id"
	echo $snap_id > $cbe_dir/control/discard_snapshot
}

main() {
	local cbe_dir="/dev/cbe"
	local data_file="$cbe_dir/current/data"

	if [ $CREATE_SNAPSHOT_TEST -eq 1 ]; then
		test_create_snapshot "$cbe_dir"
	fi

	if [ $CREATE_SNAPSHOT_DIRTY_TEST -eq 1 ]; then
		test_create_snapshot "$cbe_dir"
		test_write_1 "$data_file" "0"
		test_create_snapshot "$cbe_dir"
		test_write_1 "$data_file" "8"
		test_create_snapshot "$cbe_dir"
		test_write_1 "$data_file" "16"
		test_create_snapshot "$cbe_dir"
	fi

	if [ $RW_TEST -eq 1 ]; then
		test_rw "$data_file"
	fi

	if [ $WRITE_1_BLOCK_TEST -eq 1 ]; then
		test_write_1 "$data_file" "0"
	fi

	if [ $WRITE_CREATE_TEST -eq 1 ]; then
		test_write_1 "$data_file" "16"
		test_create_snapshot "$cbe_dir"
	fi


	if [ $LIST_SNAPSHOTS_TEST -eq 1 ]; then
		test_list_snapshots "$cbe_dir"
	fi

	if [ $DISCARD_SNAPSHOT_TEST -eq 1 ]; then
		# make sure there is at least one snapshot
		test_write_1 "$data_file" "0"
		test_create_snapshot "$cbe_dir"
		test_list_snapshots "$cbe_dir"
		test_discard_snapshot "$cbe_dir" 6
		test_list_snapshots "$cbe_dir"
	fi

	echo "--- Automated CBE testing finished, shell is yours ---"
}

main "$@"

# just drop into shell
# exit 0
