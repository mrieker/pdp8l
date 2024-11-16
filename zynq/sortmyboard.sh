#!/bin/bash
set -e
if [ SortJSon.jar -ot SortJSon.java ]
then
    rm -f SortJSon*.class SortJSon.jar
    javac SortJSon.java
    jar cf SortJSon.jar SortJSon*.class
    rm -f SortJSon*.class
fi
export CLASSPATH=SortJSon.jar
java SortJSon < myboard/myboard.bd > x.x
mv x.x myboard/myboard.bd
