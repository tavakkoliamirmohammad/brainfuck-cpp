#!/bin/bash
make
rm -rf res/*
for file in "./benches"/*
do
  if [ -f "$file" ]; then
    echo "Processing $file"
    bash ./native_timing.sh "$file"  # Run your script or command on the file
  fi
done
