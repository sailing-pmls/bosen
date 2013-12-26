sync_targets="apps datasets docs machinefiles Makefile README.md scripts src
third_party/third_party.mk .gitignore"

for target in $sync_targets; do
  echo $target
  rsync -avh ../petuum/$target .
done
