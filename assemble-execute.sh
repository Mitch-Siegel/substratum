
if ["$1" -eq "" ]
then
    echo "An argument for what to assemble is required!"
    exit 1
fi

cd ./assembler
if ! customasm ./"$1"; then
    exit
fi

cd ../emu
if ! make; then
    exit
fi

BINFILE="$(echo "$1" | cut -f 1 -d '.').bin"
echo $BINFILE

time ./emu ../assembler/$BINFILE
cd ..
