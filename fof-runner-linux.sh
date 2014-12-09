NVERT_VALUES=(1000 2000 5000 10000 20000 50000 100000)

function run() {
    echo "==========START EXPERIMENT==========" >> output_fof.log
    # Output git commit number
    git rev-parse HEAD >> output_fof.log

    echo "PARAM_NUM_VERTICES=$1" >> output_fof.log

    for m in {1..5}
    do
        ./bin/benchmark-query-creator -a 1 -n $1 -d db_twitter/

        echo "NO_MADVISE" >> output_fof.log
            echo "TRIAL $m" >> output_fof.log
            echo "==========BEFORE VM STAT==========" >> output_fof.log
        sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'
            echo "==========LLAMA OUTPUT==========" >> output_fof.log
            ./bin/benchmark-persistent --run friend_of_friends -d db_twitter/ >> output_fof.log
            echo "==========AFTER VM STAT==========" >> output_fof.log
            echo "==========END LLAMA OUTPUT==========" >> output-fof.log

        echo "WITH_MADVISE" >> output_fof.log
            echo "TRIAL $m" >> output_fof.log
            echo "==========BEFORE VM STAT==========" >> output_fof.log
        sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'
            echo "==========LLAMA OUTPUT==========" >> output_fof.log
            ./bin/benchmark-persistent-madvise --run friend_of_friends -d db_twitter/ >> output_fof.log
            echo "==========AFTER VM STAT==========" >> output_fof.log
            echo "==========END LLAMA OUTPUT==========" >> output_fof.log
    done

    rm temp.txt
}

make benchmark-persistent benchmark-persistent-madvise benchmark-query-creator

for k in ${NVERT_VALUES[@]}
do
    run $k
done

