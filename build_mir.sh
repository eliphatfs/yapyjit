cd mir
gcc -Og -g mir.c mir-gen.c c2mir/c2mir.c c2mir/c2mir-driver.c -I. -o ../micc.exe
cd ..
