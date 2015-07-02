First put `input.txt` on HDFS. For example

```
hadoop fs -put input.txt /input.txt
```

Then

```
# Create run script from template
cp script/run.py.template script/run.py

chmod +x script/run.py

# Make sure the input and output path on HDFS are correct in run.py.
./script/run.py
```
