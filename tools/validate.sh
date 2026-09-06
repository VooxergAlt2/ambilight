#!/usr/bin/env bash

set -u

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
timestamp="$(date +%Y%m%d-%H%M%S)"
output_dir="$repo_root/.artifacts/validation/$timestamp"

mkdir -p "$output_dir"

skip_native=0
skip_firmware=0

for arg in "$@"; do
    case "$arg" in
        --skip-native)
            skip_native=1
            ;;
        --skip-firmware)
            skip_firmware=1
            ;;
        *)
            echo "Unknown argument: $arg" >&2
            echo "Usage: $0 [--skip-native] [--skip-firmware]" >&2
            exit 2
            ;;
    esac
done

if command -v pio >/dev/null 2>&1; then
    pio_cmd=(pio)
elif command -v python3 >/dev/null 2>&1; then
    pio_cmd=(python3 -m platformio)
elif command -v python >/dev/null 2>&1; then
    pio_cmd=(python -m platformio)
else
    echo "PlatformIO launcher not found. Install PlatformIO or make pio/python available in PATH." >&2
    exit 2
fi

if command -v python3 >/dev/null 2>&1; then
    python_cmd=(python3)
elif command -v python >/dev/null 2>&1; then
    python_cmd=(python)
else
    echo "Python launcher not found; partition validation cannot run." >&2
    exit 2
fi

run_logged() {
    local log_file="$1"
    shift

    echo
    echo ">>> PlatformIO $*"

    "${pio_cmd[@]}" "$@" 2>&1 | tee "$log_file"
    local command_exit=${PIPESTATUS[0]}

    return "$command_exit"
}

run_python_logged() {
    local log_file="$1"
    shift

    echo
    echo ">>> Python $*"

    "${python_cmd[@]}" "$@" 2>&1 | tee "$log_file"
    local command_exit=${PIPESTATUS[0]}

    return "$command_exit"
}

cd "$repo_root"

partition_exit=0
native_exit=0
firmware_exit=0

run_python_logged "$output_dir/partition-check.log" tools/check_partition.py
partition_exit=$?

if [[ "$skip_native" -eq 0 ]]; then
    run_logged "$output_dir/native-test.log" test -e native
    native_exit=$?
fi

if [[ "$skip_firmware" -eq 0 ]]; then
    run_logged "$output_dir/firmware-build.log" run -e esp32-c6-devkitc-1
    firmware_exit=$?
fi

cat > "$output_dir/summary.txt" <<EOF
Ambilight local validation
timestamp=$timestamp
partition_exit=$partition_exit
native_skipped=$skip_native
native_exit=$native_exit
firmware_skipped=$skip_firmware
firmware_exit=$firmware_exit
output=$output_dir
EOF

echo
cat "$output_dir/summary.txt"

if [[ "$partition_exit" -ne 0 || "$native_exit" -ne 0 || "$firmware_exit" -ne 0 ]]; then
    exit 1
fi

exit 0
