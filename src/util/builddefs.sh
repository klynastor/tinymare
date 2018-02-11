#!/bin/sh

cd `dirname $0`/..
files=`echo comm/*.c db/*.c game/*.c io/*.c prog/*.c mare/*.c`

# Build definitions file */
echo "/* hdrs/defs.h */" > defs.h
echo "/* External function declarations */" >> defs.h

for i in $files; do
  echo >> defs.h
  echo "/* $i */" >> defs.h
  grep '^[A-Za-z].*(.*[),]$' $i | grep -v '^static' | grep -v // | \
    sed 's/^/extern /' | sed 's/$/;/' | sed 's/^extern int/extern int /' | \
    sed 's/()/(void)/' >> defs.h

  # Check if definitions are not referenced in any other .c file
  funcs=`grep '^[a-z].*(.*[),]$' $i | grep -v '^static' | \
         sed 's/.*[ *]\(.*\)(.*/\1/'`

  files2=`echo */*.[ch] | sed "s#$i##" | sed "s#hdrs/defs.h ##"`
  for j in $funcs; do
    if [ -z "`grep $j $files2`" ]; then
      echo "Warning: Function \`$j' is only found in $i."
    fi
  done
done
