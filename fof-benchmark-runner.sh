NVERT_VALUES=(50 100 200 500 1000 2000)

function run() {
    echo "==========START EXPERIMENT==========" >> output_fof.log
    # Output git commit number
    git rev-parse HEAD >> output_fof.log

    # Replace parameters in h file
    cp -f benchmark/benchmarks/query_creator.h benchmark/benchmarks/query_creator_tmp.h
    for param in NUM_VERTICES ALPHA
    do
        sed -i ".bak" "s/^#define $param .*$//g" benchmark/benchmarks/query_creator.h
    done
    sed -i ".bak" "s/ALPHA/1/g" benchmark/benchmarks/query_creator.h
    sed -i ".bak" "s/NUM_VERTICES/$1/g" benchmark/benchmarks/query_creator.h

    cp -f benchmark/benchmarks/friend_of_friends.h benchmark/benchmarks/friend_of_friends_tmp.h
    sed -i ".bak" "s/^#define PARAM_N .*$//g" benchmark/benchmarks/friend_of_friends.h
    sed -i ".bak" "s/PARAM_N/$1/g" benchmark/benchmarks/friend_of_friends.h

    make benchmark-persistent benchmark-persistent-madvise

    echo "PARAM_N=$1" >> output_fof.log

    for m in {1..5}
    do
        ./bin/benchmark-persistent --run query_creator -d bin/db/

        echo "NO_MADVISE" >> output_fof.log
            echo "TRIAL $m" >> output_fof.log
            echo "==========BEFORE VM STAT==========" >> output_fof.log
        sudo purge
            vm_stat >> output_fof.log
            echo "==========LLAMA OUTPUT==========" >> output_fof.log
            ./bin/benchmark-persistent --run friend_of_friends -d bin/db/ >> output_fof.log
            echo "==========AFTER VM STAT==========" >> output_fof.log
            vm_stat >> output_fof.log
            echo "==========END LLAMA OUTPUT==========" >> output_fof.log

        echo "WITH_MADVISE" >> output_fof.log
            echo "TRIAL $m" >> output_fof.log
            echo "==========BEFORE VM STAT==========" >> output_fof.log
        sudo purge
            vm_stat >> output_fof.log
            echo "==========LLAMA OUTPUT==========" >> output_fof.log
            ./bin/benchmark-persistent-madvise --run friend_of_friends -d bin/db/ >> output_fof.log
            echo "==========AFTER VM STAT==========" >> output_fof.log
            vm_stat >> output_fof.log
            echo "==========END LLAMA OUTPUT==========" >> output_fof.log
    done

    cp -f benchmark/benchmarks/query_creator_tmp.h benchmark/benchmarks/query_creator.h
    rm -f benchmark/benchmarks/query_creator_tmp.h

    cp -f benchmark/benchmarks/friend_of_friends_tmp.h benchmark/benchmarks/friend_of_friends.h
    rm -f benchmark/benchmarks/friend_of_friends_tmp.h

    rm temp.txt
}

for i in ${NVERT_VALUES[@]}
do
	run $i
done

