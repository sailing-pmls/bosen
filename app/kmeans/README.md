#Quick start

Compile and generate datasets:
```
make -j2
mkdir -p datasets
cd datasets
python ../scripts/data_generate.py 100 100 10000 libsvm
```
then put them to HDFS.
```
hadoop fs -mkdir -p /dataset/kmeans
hadoop fs -put * /dataset/kmeans/
```
run kmeans without yarn:
```
cp scripts/run_kmeans.py.template scripts/run_kmeans.py
python scripts/run_kmeans.py
```
To run kmeans on yarn, build `src/yarn` under repo root first (if not done
already):
```
cd ../../src/yarn
gradle build
```
Then come back to `app/kmeans` and do
```
cp scripts/launch_yarn_job.py.template scripts/launch_yarn_job.py
cp scripts/run_kmeans_on_yarn.py.template scripts/run_kmeans_on_yarn.py
# Change the HDFS path in scripts/run_kmeans_on_yarn.py if you put data under
# a path different from the default (/dataset/kmeans)
python scripts/launch_yarn_job.py
```

The results should be in `output/` in local FS or `hdfs:///tmp/kmeans/` in
HDFS.
