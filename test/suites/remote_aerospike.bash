SUITE_remote_aerospike_PROBE() {
    if ! $CCACHE --version | grep -Fq -- aerospike-storage &> /dev/null; then
        echo "aerospike-storage not available"
        return
    fi
    if ! command -v asd &> /dev/null; then
	echo "asd (aerospike-server) not found"
        return
    fi
    if ! command -v asinfo &> /dev/null; then
        echo "asinfo (aerospike-tools) not found"
        return
    fi
}

start_aerospike_server() {
    local port="$1"

    config=$(mktemp)
    cat <<EOF >$config
service {
        proto-fd-max 1024
        cluster-name ccache
}

network {
	service {
		address localhost
		port $port
	}

	heartbeat {
		mode mesh
		address local
		port $(($port+2))
	}

	fabric {
		address local
		port $(($port+1))
	}
}

namespace test {
	replication-factor 1
	storage-engine memory {
		data-size 1G
	}
}
EOF

    asd --config-file $config --foreground &
    # Wait for server start.
    i=0
    while [ $i -lt 100 ] && ! asinfo -p "${port}" -v version &>/dev/null; do
        sleep 0.1
        i=$((i + 1))
    done
    sleep 3
}

SUITE_remote_aerospike_SETUP() {
    unset CCACHE_NODIRECT

    generate_code 1 test.c
}

expect_number_of_aerospike_cache_entries() {
    local expected=$1
    local port=$2
    local actual

    namespace="test"
    actual=$(asinfo -p "$port" -v "sets/$namespace/redis" 2>/dev/null | cut -d: -f1 | sed -e s/objects=//)
    test -z "$actual" && return
    if [ "$actual" -ne "$expected" ]; then
        test_failed_internal "Found $actual (expected $expected) entries in localhost:$port"
    fi
}

SUITE_remote_aerospike() {
    # -------------------------------------------------------------------------
    TEST "Base case"

    port=3333
    aerospike_url="aerospike://localhost:${port}"
    export CCACHE_REMOTE_STORAGE="${aerospike_url}"

    start_aerospike_server "${port}"

    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 0
    expect_stat cache_miss 1
    expect_stat files_in_cache 2
    expect_number_of_aerospike_cache_entries 2 "$port" # result + manifest

    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 1
    expect_stat cache_miss 1
    expect_stat files_in_cache 2
    expect_number_of_aerospike_cache_entries 2 "$port" # result + manifest

    $CCACHE -C >/dev/null
    expect_stat files_in_cache 0
    expect_number_of_aerospike_cache_entries 2 "$port" # result + manifest

    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 2
    expect_stat cache_miss 1
    expect_stat files_in_cache 2 # fetched from remote
    expect_number_of_aerospike_cache_entries 2 "$port" # result + manifest

    # -------------------------------------------------------------------------
    TEST "Unreachable server"

    export CCACHE_REMOTE_STORAGE="aerospike://localhost:1"

    $CCACHE_COMPILE -c test.c
    expect_stat direct_cache_hit 0
    expect_stat cache_miss 1
    expect_stat files_in_cache 2
    expect_stat remote_storage_error 1
}
