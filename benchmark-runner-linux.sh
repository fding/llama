ALPHA_VALUES=(0.5 1)
EPOCH_VALUES=(4)
NVERT_VALUES=(1000 5000 10000 20000 50000 100000 200000 500000 1000000)
CACHE_VALUES=(1000)

function run() {
# $1 is alpha, $2 is epoch, $3 is num_vertices, $4 is cache size
    echo "==========START EXPERIMENT==========" >> output.log
    # Output git commit number
    git rev-parse HEAD >> output.log

    echo "PARAM_ALPHA=$1,PARAM_EPOCH_THRESHOLD=$2,PARAM_NUM_VERTICES=$3,PARAM_CACHE_SIZE=$4" >> output.log

    for m in {1..5}
    do
        ./bin/benchmark-query-creator -a $1 -n $3 -d db_twitter/

        echo "NO_MADVISE" >> output.log
            echo "TRIAL $m" >> output.log
            echo "==========BEFORE VM STAT==========" >> output.log
        sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'
            echo "==========LLAMA OUTPUT==========" >> output.log
            ./bin/benchmark-persistent --run query_simulator -d db_twitter/ >> output.log
            echo "==========AFTER VM STAT==========" >> output.log
            echo "==========END LLAMA OUTPUT==========" >> output.log

        echo "WITH_MADVISE" >> output.log
            echo "TRIAL $m" >> output.log
            echo "==========BEFORE VM STAT==========" >> output.log
        sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'
            echo "==========LLAMA OUTPUT==========" >> output.log
            ./bin/benchmark-persistent-madvise --run query_simulator -d db_twitter/ >> output.log
            echo "==========AFTER VM STAT==========" >> output.log
            echo "==========END LLAMA OUTPUT==========" >> output.log
    done

    rm temp.txt
}

make benchmark-persistent benchmark-persistent-madvise benchmark-query-creator

for i in ${ALPHA_VALUES[@]}
do
    for j in ${EPOCH_VALUES[@]}
    do
        for k in ${NVERT_VALUES[@]}
        do
            for l in ${CACHE_VALUES[@]}
            do
                run $i $j $k $l
            done
        done
    done
done

