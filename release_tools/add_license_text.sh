#!/bin/bash
# Treat unset variables as an error when performing parameter expansion.
set -u

if [ $# -ne 1 ]; then
echo "usage: $0 <search path>"
  exit
fi

target_files=`find $1 \( -name "*.cpp" -o -name "*.hpp" -o -name "*.c" -o -name "*.h" \)`
script_path=`readlink -f $0`
script_dir=`dirname $script_path`
project_root=`dirname $script_dir`

for src_code in $target_files # or whatever other pattern...
do
if ! grep -q Copyright $src_code
  then
echo Adding copyright to $src_code
    cat $project_root/release_tools/copyright.txt $src_code > \
      $src_code.tmp && mv $src_code.tmp $src_code
  fi
done
