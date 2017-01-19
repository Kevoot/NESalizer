#!/bin/bash
#

TOP=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)
if [ "${MARVELL_SDK_PATH}" = "" ]; then
	MARVELL_SDK_PATH="$(cd "${TOP}/../.." && pwd)"
fi
if [ "${MARVELL_ROOTFS}" = "" ]; then
	source "${MARVELL_SDK_PATH}/setenv.sh" || exit 1
fi
cd "${TOP}"

make $MAKE_J || exit 2


# All done!
echo "Build complete!"
echo
echo "Put the steamlink folder onto a USB drive, insert it into your Steam Link, and cycle the power to install."
