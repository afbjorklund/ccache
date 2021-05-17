SUITE_masquerading_PROBE() {
    local compiler_binary
    if $HOST_OS_WINDOWS || $HOST_OS_CYGWIN; then
        echo "symlinks not supported on $(uname -s)"
        return
    fi
    if [ "$(dirname $COMPILER_BIN)" != . ]; then
        echo "compiler ($COMPILER_BIN) not taken from PATH"
        return
    fi
}

SUITE_masquerading_SETUP() {
    ln -s "$CCACHE" $COMPILER_BIN
    generate_code 1 test1.c
}

SUITE_masquerading() {
    # -------------------------------------------------------------------------
    TEST "Masquerading via symlink, relative path"

    $REAL_COMPILER -c -o reference_test1.o test1.c

    ./$COMPILER_BIN $COMPILER_ARGS -c test1.c
    expect_stat 'cache hit (preprocessed)' 0
    expect_stat 'cache miss' 1
    expect_stat 'files in cache' 1
    expect_equal_object_files reference_test1.o test1.o

    ./$COMPILER_BIN $COMPILER_ARGS -c test1.c
    expect_stat 'cache hit (preprocessed)' 1
    expect_stat 'cache miss' 1
    expect_stat 'files in cache' 1
    expect_equal_object_files reference_test1.o test1.o

    # -------------------------------------------------------------------------
    TEST "Masquerading via symlink, absolute path"

    $REAL_COMPILER -c -o reference_test1.o test1.c

    $PWD/$COMPILER_BIN $COMPILER_ARGS -c test1.c
    expect_stat 'cache hit (preprocessed)' 0
    expect_stat 'cache miss' 1
    expect_stat 'files in cache' 1
    expect_equal_object_files reference_test1.o test1.o

    $PWD/$COMPILER_BIN $COMPILER_ARGS -c test1.c
    expect_stat 'cache hit (preprocessed)' 1
    expect_stat 'cache miss' 1
    expect_stat 'files in cache' 1
    expect_equal_object_files reference_test1.o test1.o
}
