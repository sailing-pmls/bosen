DNN_para=$1
para_file=$2

cp kaldi-trunk/src/petuumdnn/set_num_layers ./
mv head.txt oldhead.txt
./set_num_layers oldhead.txt head.txt $para_file
cat head.txt $DNN_para tail.txt > final.mdl
rm tail.txt
rm oldhead.txt
rm head.txt
rm set_num_layers
cp final.mdl kaldi-trunk/egs/timit/s5/exp/petuum_dnn/
cd kaldi-trunk/egs/timit/s5/
scripts/adjust_prior.sh data/train data/lang exp/tri3_ali exp/petuum_dnn
cp scripts/timit_decode.sh ./
./timit_decode.sh
rm timit_decode.sh
