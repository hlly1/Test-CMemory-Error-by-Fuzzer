#!/bin/sh

if [ -z ${TOOL_SUFFIX+x} ]
then
    # TOOL_SUFFIX not set
    # try to intelligently set TOOL_SUFFIX
    case $(uname) in
        Linux)
            TOOL_SUFFIX=-6.0
            ;;
        Darwin)
            TOOL_SUFFIX=
            ;;
        *)
            TOOL_SUFFIX=-6.0
    esac
fi


CLANG=clang${TOOL_SUFFIX}
LLVM_PROFDATA=llvm-profdata${TOOL_SUFFIX}
LLVM_COV=llvm-cov${TOOL_SUFFIX}

if [ -z "$(which ${CLANG})" ]
then
    echo "${CLANG} doesn't exist. Try setting TOOL_SUFFIX environment variable"
    exit 1
fi

if [ -z "$(which ${LLVM_PROFDATA})" ]
then
    echo "${LLVM_PROFDATA} doesn't exist. Try setting TOOL_SUFFIX environment variable"
    exit 1
fi

if [ -z "$(which ${LLVM_COV})" ]
then
    echo "${LLVM_COV} doesn't exist. Try setting TOOL_SUFFIX environment variable"
    exit 1
fi

echo "using ${CLANG}, ${LLVM_PROFDATA} and ${LLVM_COV}"

export LLVM_PROFILE_FILE="dc-%m.profraw"

if [ $# -lt 1 ]
then
    echo "Usage: $0 inputdir1 [inputdir2 ...]"
    exit 1
fi


rm -f dc*.profraw dc.profdata
echo "First re-building to make sure -DNDEBUG is turned on..."
BINARY=./bin/original/dc-cov
rm -f ${BINARY}
CLANG=${CLANG} CFLAGS="-DNDEBUG -DDC_FUZZ ${CFLAGS}" make ${BINARY}


for dir in "$@"
do
    for file in "$dir"/*
    do
        echo "Running \"${BINARY} $file\""
        ${BINARY} "$file"
    done
done

${LLVM_PROFDATA} merge -sparse dc*.profraw -o dc.profdata
${LLVM_COV} show ${BINARY} -instr-profile=dc.profdata
${LLVM_COV} report ${BINARY} -instr-profile=dc.profdata
