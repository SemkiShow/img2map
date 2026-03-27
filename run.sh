#!/bin/bash

set -e

executable_name="img2map"
build_type="RelWithDebInfo"
build_dir="build"
cmake_flags=""
run_wrapper=""
is_windows=false
use_perf=false
found_args=false
args=""

print_help() {
    echo "Usage: $0 [OPTION]..."
    echo "Compile and run $executable_name"
    echo ""
    echo "With no OPTION, compile and run the release build"
    echo ""
    echo "-h, --help           Print help"
    echo "-d, --debug          Compile the debug build and run it with gdb"
    echo "-w, --windows        Compile the Windows build and run it with Wine"
    echo "-p, --profile        Compile the profile build, profile it with perf and display the data with hotspot"
    echo "-m, --memory-leak    Compile the memory leak build and run it"
    exit 0
}

debug_build() {
    build_type="Debug"
    build_dir="build_debug"
    run_wrapper="gdb -ex run --args"
}

windows_build() {
    build_dir="build_windows"
    is_windows=true
    cmake_flags+="-DCMAKE_TOOLCHAIN_FILE=$(pwd)/mingw-w64-x86_64.cmake"
}

profile_build() {
    build_dir="build_profile"
    use_perf=true
    cmake_flags+="-DCMAKE_CXX_FLAGS='-fno-omit-frame-pointer'"
}

memory_leak_build() {
    build_dir="build_memory"
    cmake_flags+="-DCMAKE_CXX_FLAGS='-fsanitize=address'"
}

while [[ $# -gt 0 ]]; do
    if [ "$found_args" = true ]; then args+=" $1"; shift; continue; fi
    
    case "$1" in
        --*) # Handle long flags
            case "$1" in
                --help)         print_help ;;
                --debug)        debug_build ;;
                --windows)      windows_build ;;
                --profile)      profile_build ;;
                --memory-leak)  memory_leak_build ;;
                --)             found_args=true ;;
                *) echo "Unknown option: $1"; print_help ;;
            esac
            shift
            ;;
        -*) # Handle short flags
            if [[ "$1" == "-" ]]; then shift; continue; fi
            
            # Extract flags string (everything after the dash)
            flags="${1#-}"
            for (( i=0; i<${#flags}; i++ )); do
                char="${flags:i:1}"
                case "$char" in
                    h) print_help ;;
                    d) debug_build ;;
                    w) windows_build ;;
                    p) profile_build ;;
                    m) memory_leak_build ;;
                    M) mods_build ;;
                    *) echo "Unknown flag: -$char"; print_help ;;
                esac
            done
            shift
            ;;
        *) # Unrecognized
            echo "Unknown argument: $1"
            print_help
            ;;
    esac
done

clear
cmake -B "$build_dir" -DCMAKE_BUILD_TYPE="$build_type" $cmake_flags
cmake --build "$build_dir" -j$(nproc)

binary_path="./$build_dir/bin/$executable_name"
[ "$is_windows" = true ] && binary_path="${binary_path}.exe"

if [ "$use_perf" = true ]; then
    perf record --call-graph dwarf "$binary_path"
    hotspot perf.data
elif [ "$is_windows" = true ]; then
    wine "$binary_path"
else
    $run_wrapper "$binary_path" $args
fi
