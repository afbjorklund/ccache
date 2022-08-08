SUITE_remote_lmdb_PROBE() {
    if ! $CCACHE --version | fgrep -q -- lmdb-storage &> /dev/null; then
        echo "lmdb-storage not available"
        return
    fi
    if ! command -v mdb_stat &> /dev/null; then
        echo "mdb_stat not found"
        return
    fi
}

SUITE_remote_lmdb_SETUP() {
    unset CCACHE_NODIRECT

    generate_code 1 test.c
}

expect_number_of_mdb_database_entries() {
    local expected=$1
    local database=$2
    local actual

    if [ ! -e "$database" ]; then
        actual=0
    else
        actual=$(mdb_stat -n "$database" | grep '  Entries: ' | sed -e 's/  Entries: //')
    fi
    if [ "$actual" -ne "$expected" ]; then
        test_failed_internal "Found $actual (expected $expected) entries in $database"
    fi
}

SUITE_remote_lmdb() {
    # -------------------------------------------------------------------------
    TEST "Base case"

    database="$PWD/remote.mdb"
    lmdb_url="lmdb:${database}"
    export CCACHE_REMOTE_STORAGE="${lmdb_url}"

    # Compile and send result to local and remote storage.
    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 0
    expect_stat cache_miss 1
    expect_stat files_in_cache 2
    expect_stat local_storage_hit 0
    expect_stat local_storage_miss 1
    expect_stat local_storage_read_hit 0
    expect_stat local_storage_read_miss 2 # result + manifest
    expect_stat local_storage_write 2 # result + manifest
    expect_stat remote_storage_hit 0
    expect_stat remote_storage_miss 1
    expect_stat remote_storage_read_hit 0
    expect_stat remote_storage_read_miss 2 # result + manifest
    expect_stat remote_storage_write 2 # result + manifest
    expect_number_of_mdb_database_entries 3 "${database}" # mtime subdb + result + manifest

    # Get result from local storage.
    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 1
    expect_stat cache_miss 1
    expect_stat local_storage_hit 1
    expect_stat local_storage_miss 1
    expect_stat local_storage_read_hit 2 # result + manifest
    expect_stat local_storage_read_miss 2 # result + manifest
    expect_stat local_storage_write 2 # result + manifest
    expect_stat remote_storage_hit 0
    expect_stat remote_storage_miss 1
    expect_stat remote_storage_read_hit 0
    expect_stat remote_storage_read_miss 2
    expect_stat remote_storage_write 2
    expect_stat files_in_cache 2
    expect_number_of_mdb_database_entries 3 "${database}" # mtime subdb + result + manifest

    # Clear local storage.
    $CCACHE -C >/dev/null
    expect_stat files_in_cache 0
    expect_number_of_mdb_database_entries 3 "${database}" # mtime subdb + result + manifest

    # Get result from remote storage, copying it to local storage.
    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 2
    expect_stat cache_miss 1
    expect_stat local_storage_hit 1
    expect_stat local_storage_miss 2
    expect_stat local_storage_read_hit 2 # result + manifest
    expect_stat local_storage_read_miss 4 # 2 * (result + manifest)
    expect_stat local_storage_write 4 # 2 * (result + manifest)
    expect_stat remote_storage_hit 1
    expect_stat remote_storage_miss 1
    expect_stat remote_storage_read_hit 2 # result + manifest
    expect_stat remote_storage_read_miss 2 # result + manifest
    expect_stat remote_storage_write 2 # result + manifest
    expect_stat files_in_cache 2 # fetched from remote
    expect_number_of_mdb_database_entries 3 "${database}" # mtime subdb + result + manifest

    # Get result from local storage again.
    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 3
    expect_stat cache_miss 1
    expect_stat local_storage_hit 2
    expect_stat local_storage_miss 2
    expect_stat local_storage_read_hit 4 # 2 * (result + manifest)
    expect_stat local_storage_read_miss 4 # 2 * (result + manifest)
    expect_stat local_storage_write 4 # 2 * (result + manifest)
    expect_stat remote_storage_hit 1
    expect_stat remote_storage_miss 1
    expect_stat remote_storage_read_hit 2 # result + manifest
    expect_stat remote_storage_read_miss 2 # result + manifest
    expect_stat remote_storage_write 2 # result + manifest
    expect_stat files_in_cache 2 # fetched from remote
    expect_number_of_mdb_database_entries 3 "${database}" # mtime subdb + result + manifest

    # -------------------------------------------------------------------------
    TEST "Read-only"

    database="$PWD/remote.mdb"
    lmdb_url="lmdb:${database}"
    export CCACHE_REMOTE_STORAGE="${lmdb_url}"

    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 0
    expect_stat cache_miss 1
    expect_stat files_in_cache 2
    expect_number_of_mdb_database_entries 3 "${database}" # mtime subdb + result + manifest

    $CCACHE -C >/dev/null
    expect_stat files_in_cache 0
    expect_number_of_mdb_database_entries 3 "${database}" # mtime subdb + result + manifest

    CCACHE_REMOTE_STORAGE+="|read-only"

    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 1
    expect_stat cache_miss 1
    expect_stat files_in_cache 2 # fetched from remote
    expect_number_of_mdb_database_entries 3 "${database}" # mtime subdb + result + manifest

    echo 'int x;' >> test.c
    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 1
    expect_stat cache_miss 2
    expect_stat files_in_cache 4
    expect_number_of_mdb_database_entries 3 "${database}" # mtime subdb + result + manifest
}
