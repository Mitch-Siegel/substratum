cd ./assembler
if ! make run; then
    cd ..
    exit $?
fi

cd ..


