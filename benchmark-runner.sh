ALPHA_VALUES=(0.01 0.05 0.1 0.2 0.5)
EPOCH_VALUES=(0 1 2 3 4)
NVERT_VALUES=(1000 5000 10000 20000 40000)
CACHE_VALUES=(50 100 200 500 1000 2000)


function run() {
# $1 is alpha, $2 is epoch, $3 is num_vertices, $4 is cache size
    echo "==========START EXPERIMENT==========" >> output.log
    # Output git commit number
    git rev-parse HEAD >> output.log

    # Replace parameters in h file
    cp -f benchmark/benchmarks/query_simulator.h asdftmp.h
    for param in PARAM_ALPHA PARAM_EPOCH_THRESHOLD PARAM_NUM_VERTICES PARAM_CACHE_SIZE
    do
        sed -i "s/^#define $param.*$//g" benchmark/benchmarks/query_simulator.h
    done

    sed -i "s/PARAM_ALPHA/$1/g" benchmark/benchmarks/query_simulator.h
    sed -i "s/PARAM_EPOCH_THRESHOLD/$2/g" benchmark/benchmarks/query_simulator.h
    sed -i "s/PARAM_NUM_VERTICES/$3/g" benchmark/benchmarks/query_simulator.h
    sed -i "s/PARAM_CACHE_SIZE/$4/g" benchmark/benchmarks/query_simulator.h

    make benchmark-persistent benchmark-persistent-madvise

    echo "PARAM_ALPHA=$1,PARAM_EPOCH_THRESHOLD=$2,PARAM_NUM_VERTICES=$3,PARAM_CACHE_SIZE=$4" >> output.og

    cd bin
    echo "NO_MADVISE" >> ../output.log
    for i in {1..10}
    do
        echo "TRIAL $i" >> ../output.log
        echo "==========LLAMA OUTPUT==========" >> ../output.log
        benchmark-persistent --run query_simulator -d db/ >> ../output.log
        echo "==========END LLAMA OUTPUT==========" >> ../output.log
    done

    echo "WITH_MADVISE" >> output.log
    for i in {1..10}
    do
        echo "TRIAL $i" >> ../output.log
        echo "==========LLAMA OUTPUT==========" >> ../output.log
        benchmark-persistent-madvise --run query_simulator -d db/ >> ../output.log
        echo "==========END LLAMA OUTPUT==========" >> ../output.log
    done
    cd ..

    cp -f asdftmp.h benchmark/benchmarks/query_simulator.h
    rm -f asdftmp.h
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

