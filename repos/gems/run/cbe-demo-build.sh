#!/bin/sh
#
# This script MUST be executed from the Genode base directory
# and the 'cbe' repository must be checked out to 'repos/cbe'.
#

echo "Please read $0 first and delete this and the following line."
exit 0

[ $# -lt 3 ] && { echo "usage: $0 <depot-dir> <user> <version>"; exit 1; }

DEPOT_DIR="$1"
DEPOT_USER="$2"
VERSION="$3"

TARGET_DIR="$DEPOT_DIR/$DEPOT_USER/raw/cbe_binaries/$VERSION"

[ -d "$TARGET_DIR" ] && { echo "abort, package $VERSION already exists"; exit 1; }

make -C build/x86_64 cleanall

make -C build/x86_64 app/cbe_check           || exit 1
make -C build/x86_64 app/cbe_dump            || exit 1
make -C build/x86_64 app/cbe_init            || exit 1
make -C build/x86_64 app/cbe_tester          || exit 1
make -C build/x86_64 LIB=vfs_cbe             || exit 1
make -C build/x86_64 KERNEL=nova virtualbox5 || exit 1

mkdir $TARGET_DIR

cp repos/cbe/run/disk0.vmdk repos/cbe/run/machine.vbox $TARGET_DIR

for file in cbe_check cbe_dump cbe_init cbe_tester \
         libc_pipe.lib.so libsparkcrypto.lib.so qemu-usb.lib.so \
         spark.lib.so vfs_cbe.lib.so virtualbox5-nova; do

	cp -v build/x86_64/bin/$file $TARGET_DIR
done

for pkg in cbe_check cbe_dump cbe_init cbe_tester cbe_vfs cbe_vbox5-nova; do

	sed -e 's;.*cbe_binaries.*;'$DEPOT_USER'/raw/cbe_binaries/'$VERSION';' \
	    -i repos/cbe/recipes/pkg/$pkg/archives
done

./tool/depot/create -j6 DEPOT_DIR=$DEPOT_DIR UPDATE_VERSIONS=1 \
                        FORCE=1 REBUILD= $DEPOT_USER/pkg/x86_64/cbe

for pkg in cbe cbe_block cbe_check cbe_dump cbe_fs cbe_init cbe_log \
           cbe_shell cbe_tester cbe_vbox5-nova cbe_vfs cbe_vm_fs; do

	hash_file="repos/cbe/recipes/pkg/$pkg/hash"
	awk '{ $1 = "'$VERSION'"; print }' $hash_file > $hash_file.new \
		&& mv $hash_file.new $hash_file
done

# create again to use changed hash files
./tool/depot/create -j6 DEPOT_DIR=$DEPOT_DIR UPDATE_VERSIONS=1 \
                        FORCE=1 REBUILD= $DEPOT_USER/pkg/x86_64/cbe

cat << EOF
  <index name="CBE">
    <pkg path="$DEPOT_USER/pkg/cbe_block/$VERSION" info="VFS to Block"/>
    <pkg path="$DEPOT_USER/pkg/cbe_check/$VERSION" info="Tool for checking the CBE"/>
    <pkg path="$DEPOT_USER/pkg/cbe_dump/$VERSION" info="Tool for dumping the CBE meta-data"/>
    <pkg path="$DEPOT_USER/pkg/cbe_fs/$VERSION" info="CBE file system chroot"/>
    <pkg path="$DEPOT_USER/pkg/cbe_init/$VERSION" info="Tool for initializing the CBE"/>
    <pkg path="$DEPOT_USER/pkg/cbe_log/$VERSION" info="LOG terminal"/>
    <pkg path="$DEPOT_USER/pkg/cbe_shell/$VERSION" info="Shell for CBE management"/>
    <pkg path="$DEPOT_USER/pkg/cbe_tester/$VERSION" info="Tool for testing the CBE"/>
    <pkg path="$DEPOT_USER/pkg/cbe_vbox5-nova/$VERSION" info="VBox5 for running Linux on the CBE"/>
    <pkg path="$DEPOT_USER/pkg/cbe_vfs/$VERSION" info="CBE VFS server"/>
    <pkg path="$DEPOT_USER/pkg/cbe_vm_fs/$VERSION" info="VM file system chroot"/>
  </index>
EOF

echo "Do not forget to publish the archives and use the proper cbe pkg version (see above):"
echo "  ./tool/depot/publish DEPOT_DIR=$DEPOT_DIR $DEPOT_USER/pkg/x86_64/cbe/xxx $DEPOT_USER/index/20.02"

exit 0
