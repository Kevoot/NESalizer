#!/bin/bash
#
echo "Checking For SDK..."
TOP=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
if [ "${MARVELL_SDK_PATH}" = "" ]; then
	echo "Setting up SDK..."
	MARVELL_SDK_PATH="$(cd "${TOP}/../.." && pwd)"
fi
if [ "${MARVELL_ROOTFS}" = "" ]; then
	echo "Setting up SDK..."
	source "${MARVELL_SDK_PATH}/setenv.sh" || exit 1
fi
cd "${TOP}"
echo "Building..."
make $MAKE_J || exit 2
echo "Build complete!"
