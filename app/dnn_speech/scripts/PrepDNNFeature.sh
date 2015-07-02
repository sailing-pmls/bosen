timit_path=$1

cp -r scripts kaldi-trunk/egs/timit/s5/
cp -r petuumdnn/ kaldi-trunk/src/
cd kaldi-trunk/src/petuumdnn/
make
cd ../../egs/timit/s5/
mv scripts/path.sh ./
mv scripts/timit_data_prep.sh ./
#mv scripts/timit.sh ./
cp scripts/petuum_dnn.sh ./
./timit_data_prep.sh $timit_path
#./timit.sh
cp Train.fea Train.label Train.para tail.txt head.txt ../../../../
rm Train.fea Train.label Train.para tail.txt head.txt
rm petuum_dnn.sh
rm timit_data_prep.sh
#rm timit.sh
