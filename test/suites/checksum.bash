SUITE_checksum_SETUP() {
    generate_code 1 test.c
}

SUITE_checksum() {

    for CHECKSUM in ${CHECKSUMS:-md4}
    do

    # -------------------------------------------------------------------------
    TEST "Test $CHECKSUM"

    CCACHE_CHECKSUM=$CHECKSUM $CCACHE_COMPILE -c test.c
    expect_stat 'cache hit (direct)' 0
    expect_stat 'cache hit (preprocessed)' 0
    expect_stat 'cache miss' 1

    CCACHE_CHECKSUM=$CHECKSUM $CCACHE_COMPILE -c test.c
    expect_stat 'cache hit (direct)' 0
    expect_stat 'cache hit (preprocessed)' 1
    expect_stat 'cache miss' 1

    done
}
