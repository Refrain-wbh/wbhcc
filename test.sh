#!/bin/bash
assert() {
  expected="$1"
  input="$2"

  ./wcc "$input" 
  gcc -static -o tmp code.S
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 21 '5+20-4'
assert 41 ' 12 + 34 - 5 '
assert 47 '5+6*7'
assert 15 '5*(9-6)'
assert 4 '(3+5)/2'
assert 2 '(3+5)/2/2'

echo OK
