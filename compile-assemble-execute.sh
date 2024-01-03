set -e

if [ ! $1 ]
then
    echo "please provide an argument to be passed to the assembler makefile ('run' or 'run-for-debug')"
    exit 1
fi

echo "building compiler"
cd compiler
if ! make -j`nproc`; then
    exit
fi

echo ""
echo "compiling files..."
# xargs -I {} sh -c "echo {}; ls -la {}"
rm -f testsrc/*.o
ls testsrc/*.ca | cut -d '.' -f1 | xargs -I {} sh -c "./cacc -i {}.ca -o {}.o -I testsrc || exit 255"
# if ! ./mcc testsrc ../assembler/main.asm; then
    # exit
# fi

echo "linking files"
cd testsrc
rm -f linked.o
touch linked.o
# xargs -I {} sh -c "echo {}; ls -la {}"
ls *.o | xargs -I {} sh -c "../cald -i linked.o -i {} -o linked.o || exit 255"

cd ..
./cald -i ./testsrc/linked.o -o ../assembler/linked.s -e


cd ../assembler
if ! make $1; then
    cd ..
    exit $?
fi

cd ..
