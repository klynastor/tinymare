#!/bin/sh

echo "#include \"externs.h\"

int main()
{" > sizeof.c

cat */*.h | grep ^struct | cut -f1 -d'{' | grep -v '*' |
awk '{ { printf "  printf(\"sizeof(%s) = %%d\\n\", sizeof(%s));\n", $0, $0 } }' >> sizeof.c

echo "
  return 0;
}" >> sizeof.c

gcc -O3 -Ihdrs/ sizeof.c -o sizeof
./sizeof
rm -f sizeof.c sizeof
