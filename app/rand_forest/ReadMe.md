#Quick start

the datasets are regenerated.
put datasets to HDFS.
```
hadoop fs -mkdir -p /dataset/rand_forest
hadoop fs -put datasets/* /dataset/rand_forest/
```
run rf without yarn:
```
cp scripts/run_rf.py.template scripts/run_rf.py
python scripts/run_rf.py
```
run rf on yarn:
```
cp scripts/launch_yarn_job.py.template scripts/launch_yarn_job.py
cp scripts/run_rf_on_yarn.py.tmplate scripts/run_rf_on_yarn.py
python scripts/launch_yarn_job.py
```
run rf_load without yarn:
```
cp scripts/run_rf_load.py.template scripts/run_rf_load.py
python scripts/run_rf_load.py
# the result will be stored in `output` in local FS
```

run rf_load on yarn:
```
cp scripts/launch_yarn_job.py.template scripts/launch_yarn_job.py
#change the launch_script_path to run_rf_load_on_yarn.py in scripts/launch_yarn_job.py
cp scripts/run_rf_load_on_yarn.py.tmplate scripts/run_rf_load_on_yarn.py
python scripts/launch_yarn_job.py
```
Note: 

1. Before launching the yarn job, the yarn module in PETUUM_ROOT/src/yarn must be built.
2. Result will be stored in `output` in local FS or `/tmp/rf_output/` in HDFS
