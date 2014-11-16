ALPHA_VALUES=(0.5)
EPOCH_VALUES=(4)
NVERT_VALUES=(60000 100000 150000 200000)
CACHE_VALUES=(1000)

function run() {
# $1 is alpha, $2 is epoch, $3 is num_vertices, $4 is cache size
    echo "==========START EXPERIMENT==========" >> output.log
    # Output git commit number
    git rev-parse HEAD >> output.log

    # Replace parameters in h file
    cp -f benchmark/benchmarks/query_creator.h benchmark/benchmarks/query_creator_tmp.h
    for param in ALPHA NUM_VERTICES CACHE_SIZE
    do
        sed -i ".bak" "s/^#define $param .*$//g" benchmark/benchmarks/query_creator.h
    done

    cp -f benchmark/benchmarks/query_simulator.h benchmark/benchmarks/query_simulator_tmp.h
    for param in PARAM_ALPHA PARAM_EPOCH_THRESHOLD PARAM_NUM_VERTICES PARAM_CACHE_SIZE
    do
        sed -i ".bak" "s/^#define $param .*$//g" benchmark/benchmarks/query_simulator.h
    done

    sed -i ".bak" "s/ALPHA/$1/g" benchmark/benchmarks/query_creator.h
    sed -i ".bak" "s/NUM_VERTICES/$3/g" benchmark/benchmarks/query_creator.h
    sed -i ".bak" "s/CACHE_SIZE/$4/g" benchmark/benchmarks/query_creator.h

    sed -i ".bak" "s/PARAM_ALPHA/$1/g" benchmark/benchmarks/query_simulator.h
    sed -i ".bak" "s/PARAM_EPOCH_THRESHOLD/$2/g" benchmark/benchmarks/query_simulator.h
    sed -i ".bak" "s/PARAM_NUM_VERTICES/$3/g" benchmark/benchmarks/query_simulator.h
    sed -i ".bak" "s/PARAM_CACHE_SIZE/$4/g" benchmark/benchmarks/query_simulator.h

    make benchmark-persistent benchmark-persistent-madvise

    echo "PARAM_ALPHA=$1,PARAM_EPOCH_THRESHOLD=$2,PARAM_NUM_VERTICES=$3,PARAM_CACHE_SIZE=$4" >> output.log

    for m in {1..5}
    do
        ./bin/benchmark-persistent --run query_creator -d bin/db/

        echo "NO_MADVISE" >> output.log
            echo "TRIAL $m" >> output.log
            echo "==========BEFORE VM STAT==========" >> output.log
        sudo purge
            vm_stat >> output.log
            echo "==========LLAMA OUTPUT==========" >> output.log
            ./bin/benchmark-persistent --run query_simulator -d bin/db/ >> output.log
            echo "==========AFTER VM STAT==========" >> output.log
            vm_stat >> output.log
            echo "==========END LLAMA OUTPUT==========" >> output.log

        echo "WITH_MADVISE" >> output.log
            echo "TRIAL $m" >> output.log
            echo "==========BEFORE VM STAT==========" >> output.log
        sudo purge
            vm_stat >> output.log
            echo "==========LLAMA OUTPUT==========" >> output.log
            ./bin/benchmark-persistent-madvise --run query_simulator -d bin/db/ >> output.log
            echo "==========AFTER VM STAT==========" >> output.log
            vm_stat >> output.log
            echo "==========END LLAMA OUTPUT==========" >> output.log
    done

    cp -f benchmark/benchmarks/query_creator_tmp.h benchmark/benchmarks/query_creator.h
    rm -f benchmark/benchmarks/query_creator_tmp.h

    cp -f benchmark/benchmarks/query_simulator_tmp.h benchmark/benchmarks/query_simulator.h
    rm -f benchmark/benchmarks/query_simulator_tmp.h

    rm temp.txt
}

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

