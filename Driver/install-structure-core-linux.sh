#!/bin/bash -e
usage() {
    cat <<'EOF'
usage: install-structure-core-linux.sh [-h] [-v] [-f <name>] [-u <user>] [-g <group>] [-m <mode>] [<directory> ...]
-h/--help: show this message
-v/--verbose: verbose output
-f <name>/--filename=<name>: name of rules file to create if necessary (default: structure-core.rules)
-u <user>/--user=<user>: name of owning user for device nodes (default: <current user>)
-g <group>/--group=<group>: name of owning group for device nodes (default: <current group>)
-m <mode>/--mode=<mode>: octal permissions for device nodes (default: 0664)
<directory>: where to install rules or search for existing rules to update

Use this script to make Structure Core devices accessible to non-root users.
Structure Core devices do not require a kernel driver because all USB operations
are done in userspace through libusb; however, the corresponding USB device
nodes (/dev/bus/usb/<bus>/<device>) must have appropriate permissions.

The default directory in which to install rules (or update existing rules) is
/etc/udev/rules.d. If one or more <directory> arguments are given, those
directories will be used instead. Every Structure Core rules file found will be
updated. If no Structure Core rules files are found, a new rules file will be
installed into the first <directory> given (or /etc/udev/rules.d if none are
given).

Rule changes may not take effect on devices that are already connected until
they are removed and reconnected. Do not modify rules files installed by this
script.
EOF
}
# Unique token to identify rules installed by this script; do not change
ruleIdentifier='#OCCIPITAL-MAGIC:3786cbf2-1552-42ef-8683-4bcd9fe51087'
# Hexadecimal USB VID:PID pairs to match
vidPidPairs=(2959:3001)

verbose=0
ruleFilename=structure-core.rules
if [[ "$SUDO_UID" ]]; then
    deviceUser=$(id -n -u "$SUDO_UID")
    deviceGroup=$(id -n -g "$SUDO_UID")
else
    deviceUser=$(id -n -u)
    deviceGroup=$(id -n -g)
fi
devicePerm=0664
while getopts ":hvf:u:g:m:-:" opt; do
    case $opt in
        -) case $OPTARG in
               help) usage; exit 0 ;;
               verbose) verbose=1 ;;
               filename=*) ruleFilename=${OPTARG#*=} ;;
               user=*) deviceUser=${OPTARG#*=} ;;
               group=*) deviceGroup=${OPTARG#*=} ;;
               mode=*) devicePerm=${OPTARG#*=} ;;
               *)
                   >&2 echo "Option not understood: $OPTARG"
                   >&2 usage
                   exit 1
           esac ;;
        h) usage; exit 0 ;;
        v) verbose=1 ;;
        f) ruleFilename=$OPTARG ;;
        u) deviceUser=$OPTARG ;;
        g) deviceGroup=$OPTARG ;;
        m) devicePerm=$OPTARG ;;
        *)
            >&2 echo "Option not understood: $OPTARG"
            >&2 usage
            exit 1
    esac
done
shift $((OPTIND-1))
if x=$(printf '%04o' "0$devicePerm" 2>/dev/null) && (("8#$x" <= 07777)); then
    devicePerm=$x
else
    >&2 echo "Octal mode not valid: $devicePerm"
    exit 1
fi
ruleDirs=("$@")
if ((${#ruleDirs[@]} == 0)); then
    ruleDirs=(/etc/udev/rules.d)
fi
if ((verbose)); then
    set -x
fi

echo "Structure Core USB device nodes will be owned by $deviceUser:$deviceGroup (mode $devicePerm)"

writeRules() {
    local rules tmpfile vidpid vid pid
    rules=$1
    tmpfile=$(mktemp "$rules.XXXXXX")
    chmod 0644 -- "$tmpfile"
    exec 3>"$tmpfile"
    >&3 printf '%s\n' "$ruleIdentifier"
    >&3 printf '# This rules file is automatically generated. Do not modify.\n'
    for vidpid in "${vidPidPairs[@]}"; do
        vid=${vidpid%:*}
        pid=${vidpid#*:}
        >&3 printf 'SUBSYSTEM=="usb", ATTR{idVendor}=="%s", ATTR{idProduct}=="%s", OWNER="%s", GROUP="%s", MODE="%s"\n' "$vid" "$pid" "$deviceUser" "$deviceGroup" "$devicePerm"
    done
    exec 3>&-
    mv -- "$tmpfile" "$rules"
}

foundExistingRules=0
findDirs=()
for d in "${ruleDirs[@]}"; do
    echo "Searching directory: $d"
    # Ensure directory arguments are not interpreted as options to find(1)
    if [[ "${d#/}" != "$d" ]]; then
        findDirs+=("$d")
    else
        findDirs+=("./$d")
    fi
done
while IFS= read -r -d '' rules; do
    if grep -Fqx -e "$ruleIdentifier" -- "$rules"; then
        echo "Updating Structure Core rules file: $rules"
        foundExistingRules=1
        writeRules "$rules"
    else
        echo "Skipping irrelevant rules file: $rules"
    fi
done < <(find -P "${findDirs[@]}" -mindepth 1 -maxdepth 1 -type f -name '*.rules' -print0)

if ((!foundExistingRules)); then
    f=${ruleDirs[0]}/$ruleFilename
    echo "No existing Structure Core rules found, creating new rules file: $f"
    writeRules "$f"
fi
