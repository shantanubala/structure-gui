#!/bin/bash -e
usage() {
    cat <<'EOF'
usage: build.sh [-h] [-v] [-D|-R] [other options...] [<target> ...]
-h/--help: show this message
-v/--verbose: verbose output
-a <arch>/--arch=<arch>: target architecture for build (default: autodetect)
-c <arg>/--cmake=<arg>: additional argument for cmake, may specify multiple times
-j <jobs>/--jobs=<jobs>: number of parallel build jobs (default: output of `nproc`)
-m <arg>/--make=<arg>: additional argument for make, may specify multiple times
-D/--debug: build Debug configuration
-R/--release: build Release configuration (default)
--fresh: clean build directory before building
<target>: target(s) to build (default if none specified: Samples)
EOF
}
sdkDir=$(cd "$(dirname "$0")/.." && pwd)

verbose=0
arch=''
extraCmakeArgs=()
extraMakeArgs=()
jobs=$(nproc)
config=Release
fresh=0
while getopts ':hva:c:j:m:DR-:' opt; do
    case $opt in
        -) case $OPTARG in
               help) usage; exit 0 ;;
               verbose) verbose=1 ;;
               arch=*) arch=${OPTARG#*=} ;;
               cmake=*) extraCmakeArgs+=("${OPTARG#*=}") ;;
               jobs=*) jobs=${OPTARG#*=} ;;
               make=*) extraMakeArgs+=("${OPTARG#*=}") ;;
               debug) config=Debug ;;
               release) config=Release ;;
               fresh) fresh=1 ;;
               *)
                   >&2 echo "Option not understood: $OPTARG"
                   >&2 usage
                   exit 1
           esac ;;
        h) usage; exit 0 ;;
        v) verbose=1 ;;
        a) arch=$OPTARG ;;
        c) extraCmakeArgs+=("$OPTARG") ;;
        j) jobs=$OPTARG ;;
        m) extraMakeArgs+=("$OPTARG") ;;
        D) config=Debug ;;
        R) config=Release ;;
        *)
            >&2 echo "Option not understood: $OPTARG"
            >&2 usage
            exit 1
    esac
done
shift $((OPTIND-1))
if (($#)); then
    targets=("$@")
else
    targets=(Samples)
fi
if ((verbose)); then
    set -x
fi

arch=${arch:-$(uname -m)}
case $arch in
    x86_64) targetArch=x86_64 ;;
    arm64|aarch64) targetArch=arm64 ;;
    # Sanity check. If you really are building on something else, add a clause above
    *)
        >&2 echo "Unexpected architecture: $arch"
        exit 1
esac
cmakeArgs=(
    -DCMAKE_BUILD_TYPE="$config"
    -DLINUX_TARGET_ARCH="$targetArch"
)
if ((verbose)); then
    cmakeArgs+=(--debug-output)
fi
makeArgs=(
    -j"$jobs"
)
if ((verbose)); then
    makeArgs+=(VERBOSE=1)
fi

buildDir=$sdkDir/Builds/$(printf '%s' "$config" | tr A-Z a-z)-$arch
mkdir -p "$buildDir"
if ((fresh)); then
    find -P "$buildDir" -xdev -mindepth 1 -delete
fi
(
    set -e
    cd "$buildDir"
    cmake "${cmakeArgs[@]}" "${extraCmakeArgs[@]}" "$sdkDir"
    make "${makeArgs[@]}" "${extraMakeArgs[@]}" "${targets[@]}"
)
