// This program converts a set of images to a lmdb/leveldb by storing them
// as Datum proto buffers.
// Usage:
//   convert_imageset [FLAGS] ROOTFOLDER/ LISTFILE DB_NAME
//
// where ROOTFOLDER is the root folder that holds all the images, and LISTFILE
// should be a list of files as well as their labels, in the format as
//   subfolder1/file1.JPEG 7
//   ....

#include <gflags/gflags.h>
#include <glog/logging.h>
#include <leveldb/db.h>
#include <leveldb/write_batch.h>
#include <lmdb.h>
#include <sys/stat.h>

#include <algorithm>
#include <fstream>  // NOLINT(readability/streams)
#include <iostream>  // NOLINT(readability/streams)
#include <string>
#include <utility>
#include <vector>

using std::string;

DEFINE_string(backend, "lmdb", "The backend for storing the result");
DEFINE_int32(num_partitions, 2, "The number of partitions");

int main(int argc, char** argv) {
  ::google::InitGoogleLogging(argv[0]);

#ifndef GFLAGS_GFLAGS_H_
  namespace gflags = google;
#endif

  gflags::SetUsageMessage("Partition a leveldb/lmdb into multiple leveldbs/lmdbs\n"
        "format used as input for Caffe.\n"
        "Usage:\n"
        "    partition_data [FLAGS] DB_PATH\n");
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  if (argc != 2) {
    gflags::ShowUsageWithFlagsRestrict(argv[0], "tools/partition_data");
    return 1;
  }

  const string& db_backend = FLAGS_backend;
  const int num_partitions = FLAGS_num_partitions;
  const char* db_path = argv[1];

  if (db_backend == "leveldb") {  // leveldb
    // Open db
    leveldb::DB* db;
    leveldb::Options options;
    options.max_open_files = 100;
    options.create_if_missing = false;

    LOG(INFO) << "Opening leveldb " << db_path;
    leveldb::Status status = leveldb::DB::Open(
        options, db_path, &db);
    CHECK(status.ok()) << "Failed to open leveldb " << db_path;

    // Create new dbs
    leveldb::Options new_options;
    new_options.error_if_exists = true;
    new_options.create_if_missing = true;
    new_options.write_buffer_size = 268435456;
    leveldb::WriteBatch *batch[num_partitions];
    leveldb::DB *new_db[num_partitions];
    char pid_str[50];
    for (int pidx = 0; pidx < num_partitions; ++pidx) {
      snprintf(pid_str, 50, "_%d", pidx);
      const char* new_db_path = (string(db_path) + pid_str).c_str();
      leveldb::Status status = leveldb::DB::Open(
          new_options, new_db_path, &(new_db[pidx]));
      CHECK(status.ok()) << "Failed to open leveldb " << new_db_path
          << ". Is it already existing?";
      batch[pidx] = new leveldb::WriteBatch();
    }
     
    leveldb::Iterator* iter
      = db->NewIterator(leveldb::ReadOptions());
    iter->SeekToFirst();
    
    int num_it = 0, count = 0;
    while (iter->Valid()) {
      for (int pidx = 0; pidx < num_partitions; ++pidx) {
        batch[pidx]->Put(iter->key().ToString(), iter->value().ToString());
        count++;
        iter->Next();
        if (!iter->Valid()) {
          break;
        }
      }
      num_it++;
      if (num_it % 1000 == 0 || !iter->Valid()) {
        for (int pidx = 0; pidx < num_partitions; ++pidx) {
          new_db[pidx]->Write(leveldb::WriteOptions(), batch[pidx]);
          delete batch[pidx];
          batch[pidx] = new leveldb::WriteBatch();
        }
        if (num_it % 1000 == 0) {
          std::cout << "Processed " << (num_it * num_partitions) 
              << " files. " << std::endl;
        } else {
          std::cout << "Processed " << count
              << " files. " << std::endl;
          std::cout << "Done." << std::endl;
        }
      }
    }
    
    delete db;
    for (int pidx = 0; pidx < num_partitions; ++pidx) {
      delete new_db[pidx];
      delete batch[pidx];
    }
  } else if (db_backend == "lmdb") {  // lmdb
    LOG(FATAL) << "Do not support lmdb yet ";

    // Open db
    // lmdb
    MDB_env *mdb_env;
    MDB_dbi mdb_dbi;
    MDB_val mdb_key, mdb_value;
    MDB_txn *mdb_txn;
    MDB_cursor* mdb_cursor;

    CHECK_EQ(mdb_env_create(&mdb_env), MDB_SUCCESS) << "mdb_env_create failed";
    CHECK_EQ(mdb_env_set_mapsize(mdb_env, 1099511627776), MDB_SUCCESS);  // 1TB
    CHECK_EQ(mdb_env_open(mdb_env, db_path,
             MDB_RDONLY|MDB_NOTLS, 0664), MDB_SUCCESS) << "mdb_env_open failed";
    CHECK_EQ(mdb_txn_begin(mdb_env, NULL, MDB_RDONLY, &mdb_txn), MDB_SUCCESS)
        << "mdb_txn_begin failed";
    CHECK_EQ(mdb_open(mdb_txn, NULL, 0, &mdb_dbi), MDB_SUCCESS)
        << "mdb_open failed";
    CHECK_EQ(mdb_cursor_open(mdb_txn, mdb_dbi, &mdb_cursor), MDB_SUCCESS)
        << "mdb_cursor_open failed";
    LOG(INFO) << "Opening lmdb " << db_path;
    CHECK_EQ(mdb_cursor_get(mdb_cursor, &mdb_key, &mdb_value, MDB_FIRST),
        MDB_SUCCESS) << "mdb_cursor_get failed ";
  } else {
    LOG(FATAL) << "Unknown db backend " << db_backend;
  }

  return 0;
}
