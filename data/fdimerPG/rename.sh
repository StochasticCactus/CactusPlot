#!/bin/bash

for file in $(ls *.TXT); do

  full_name=$(basename -- $file)
  file_name=${full_name%.*}

  tail +30 $file > "$file_name.xvg"

done
