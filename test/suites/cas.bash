SUITE_cas_SETUP() {
    unset CCACHE_NODIRECT

    generate_code 1 test1.c
}

SUITE_cas() {
    # -------------------------------------------------------------------------
    TEST "CCACHE_CAS"

    $COMPILER -c -o reference_test1.o test1.c

    CCACHE_CAS=1 $CCACHE_COMPILE -c test1.c
    expect_stat direct_cache_hit 0
    expect_stat cache_miss 1
    expect_stat files_in_cache 3
    expect_equal_object_files reference_test1.o test1.o

    CCACHE_CAS=1 $CCACHE_COMPILE -c test1.c
    expect_stat direct_cache_hit 1
    expect_stat cache_miss 1
    expect_stat files_in_cache 3
    expect_equal_object_files reference_test1.o test1.o

    # Change (preprocessed) source, but not object
    echo "extern int foo;" >> test1.c

    CCACHE_CAS=1 $CCACHE_COMPILE -c test1.c
    expect_stat direct_cache_hit 1
    expect_stat cache_miss 2
    expect_stat files_in_cache 5
    expect_equal_object_files reference_test1.o test1.o
}
