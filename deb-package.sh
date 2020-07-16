#/bin/bash

set -e

# Package needs to be build before creating the .deb file
# Recomended way to building the binary:
# ./configure --disable-lib-build
# make -j4
if [[ ! -f build/tekstitv ]]; then
    echo "build/tekstitv not found."
    echo "Binary needs to be build before creating the .deb file"
    exit 1
fi

architecture=`uname -m`
major_version=`awk '/TEKSTITV_MAJOR_VERSION/{print $3}' include/tekstitv.h`
minor_version=`awk '/TEKSTITV_MINOR_VERSION/{print $3}' include/tekstitv.h`

case "$architecture" in
    x86_64)
        architecture=amd64
        ;;
    x86)
        architecture=i386
        ;;
    # TODO: deal with more arm stuff
    armv7l)
        architecture=armhf
        ;;
    *)
        echo "Invalid architecture: $architecture"
        exit 1
        ;;
esac

deb_file_name="tekstitv_$major_version.$minor_version-1_$architecture"
echo "Building: $deb_file_name"

# Generate the DEBIAN control
echo "Package: tekstitv" > DEBIAN/control
echo "Version: $major_version.$minor_version" >> DEBIAN/control
echo "Architecture: $architecture" >> DEBIAN/control
cat DEBIAN/control.in >> DEBIAN/control

# Set up DEBIAN
mkdir -p "build/$deb_file_name"
cp -r DEBIAN "build/$deb_file_name"
# control.in doesn't belong in to the .deb package
rm build/$deb_file_name/DEBIAN/control.in

# Setup the files that are going to be installed
mkdir -p "build/$deb_file_name/usr/bin"
cp build/tekstitv "build/$deb_file_name/usr/bin"
mkdir -p "build/$deb_file_name/etc/bash_completion.d"
cp tekstitv-completion.sh "build/$deb_file_name/etc/bash_completion.d/tekstitv"

# Finally create the actual .deb package
cd build
dpkg-deb --build --root-owner-group $deb_file_name

echo "build/$deb_file_name.deb build succesfully!"
