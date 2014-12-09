make benchmark-persistent benchmark-persistent-madvise benchmark-query-creator

echo "==========START EXPERIMENT==========" >> output_tc.log
# Output git commit number
git rev-parse HEAD >> output_tc.log

for m in {1..5}
do
  echo "NO_MADVISE" >> output_tc.log
  echo "TRIAL $m" >> output_tc.log
  echo "==========BEFORE VM STAT==========" >> output_tc.log
  sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'
  echo "==========LLAMA OUTPUT==========" >> output_tc.log
  ./bin/benchmark-persistent --run tc_od -d bin/db/ >> output_tc.log
  echo "==========AFTER VM STAT==========" >> output_tc.log
  echo "==========END LLAMA OUTPUT==========" >> output_tc.log

  echo "WITH_MADVISE" >> output_tc.log
  echo "TRIAL $m" >> output_tc.log
  echo "==========BEFORE VM STAT==========" >> output_tc.log
  sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'
  echo "==========LLAMA OUTPUT==========" >> output_tc.log
  ./bin/benchmark-persistent-madvise --run tc_od -d bin/db/ >> output_tc.log
  echo "==========AFTER VM STAT==========" >> output_tc.log
  echo "==========END LLAMA OUTPUT==========" >> output_tc.log
done
