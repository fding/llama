make benchmark-persistent benchmark-persistent-madvise

echo "==========START EXPERIMENT==========" >> output_pagerank_no_premadvise.log
# Output git commit number
git rev-parse HEAD >> output_pagerank_no_premadvise.log

for m in {1..5}
do
  echo "NO_MADVISE" >> output_pagerank_no_premadvise.log
  echo "TRIAL $m" >> output_pagerank_no_premadvise.log
  echo "==========BEFORE VM STAT==========" >> output_pagerank_no_premadvise.log
  sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'
  echo "==========LLAMA OUTPUT==========" >> output_pagerank_no_premadvise.log
  ./bin/benchmark-persistent --run pagerank_push -d db_twitter/ >> output_pagerank_no_premadvise.log
  echo "==========AFTER VM STAT==========" >> output_pagerank_no_premadvise.log
  echo "==========END LLAMA OUTPUT==========" >> output_pagerank_no_premadvise.log

  echo "WITH_MADVISE" >> output_pagerank_no_premadvise.log
  echo "TRIAL $m" >> output_pagerank_no_premadvise.log
  echo "==========BEFORE VM STAT==========" >> output_pagerank_no_premadvise.log
  sudo sh -c 'sync && echo 3 > /proc/sys/vm/drop_caches'
  echo "==========LLAMA OUTPUT==========" >> output_pagerank_no_premadvise.log
  ./bin/benchmark-persistent-madvise --run pagerank_push -d db_twitter/ >> output_pagerank_no_premadvise.log
  echo "==========AFTER VM STAT==========" >> output_pagerank_no_premadvise.log
  echo "==========END LLAMA OUTPUT==========" >> output_pagerank_no_premadvise.log
done
