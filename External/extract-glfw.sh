#!/bin/sh -e
here=$(cd "$(dirname "$0")" && pwd)
cd "$1"
if ! [ -d glfw-3.2.1 ]; then
    tmpdir=$(mktemp -d glew-extract.XXXXXX)
    unzip -d "$tmpdir" "$here/glfw-3.2.1.zip"
    patch -d "$tmpdir/glfw-3.2.1" -p1 <"$here/glfw-no-cmp0042-warning.patch"
    patch -d "$tmpdir/glfw-3.2.1" -p1 <"$here/glfw-no-vulkan.patch"
    patch -d "$tmpdir/glfw-3.2.1" -p1 <"$here/glfw-no-x11-msg.patch"
    mv "$tmpdir/glfw-3.2.1" .
    rmdir "$tmpdir"
fi
