#!/bin/sh -e
here=$(cd "$(dirname "$0")" && pwd)
cd "$1"
if ! [ -d glew-2.1.0 ]; then
    tmpdir=$(mktemp -d glew-extract.XXXXXX)
    tar -C "$tmpdir" -xvf "$here/glew-2.1.0.tgz"
    patch -d "$tmpdir/glew-2.1.0" -p1 <"$here/glew-no-cmp0072-warning.patch"
    mv "$tmpdir/glew-2.1.0" .
    rmdir "$tmpdir"
fi
