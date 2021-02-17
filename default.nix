{ stdenv, lib, catch2 }:
stdenv.mkDerivation {
  name = "patch-shebangs";
  version = "git";
  src = ./.;

  buildPhase = ''
    mkdir -p $out/bin
    c++ -Wall -std=c++17 -c patch-shebangs.cpp -o patch-shebangs.o
    c++ -Wall -std=c++17 -c main.cpp -o main.o
    c++ -v -Wall -std=c++17 -lstdc++fs *.o -o $out/bin/patch-shebangs
  '';

  installPhase = "true";

  nativeCheckInputs = [ catch2 ];

  checkPhase = ''
    testPath=$(mktemp -d)
    touch $testPath/executable
    chmod +x $testPath/executable
    touch $testPath/non-executable
    mkdir -p $testPath/subdir/anothersub
    echo "#!/bin/sh " > $testPath/subdir/something
    chmod +x $testPath/subdir/something

    sed -e "s;@testPath@;$testPath;g" -i tests.cpp
    c++ -Wall -I${lib.getDev catch2}/include -std=c++17 -c tests.cpp -o tests.o
    c++ -Wl,-rpath,${lib.getLib catch2}/lib -std=c++17 -lstdc++fs -L${lib.getLib catch2}/lib -lcatch2 patch-shebangs.o tests.o -o tests

    ./tests -s -v high

    # test the build with cmdline args
    cd $(mktemp -d)
    echo "#!/bin/sh" >> test.sh
    chmod +x test.sh
    $out/bin/patch-shebangs /bin/sh=$SHELL test.sh
    cat test.sh
    grep $SHELL test.sh
    ./test.sh

    # test the build with environment variables
    cd $(mktemp -d)
    echo "#!/usr/bin/env $(basename $SHELL)" >> test.sh
    chmod +x test.sh
    PATH=$(dirname $SHELL) $out/bin/patch-shebangs test.sh
    cat test.sh
    grep $SHELL test.sh
    ./test.sh
  '';
}
