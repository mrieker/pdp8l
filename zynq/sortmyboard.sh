#!/bin/bash
set -e
if [ SortJSon.class -ot SortJSon.java ]
then
    rm -f SortJSon*.class
    javac SortJSon.java
fi
java SortJSon < myboard/myboard.bd > x.x
mv x.x myboard/myboard.bd
