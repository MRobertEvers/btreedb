## Building

Build uses cmake. 

```
mkdir build
cd build
cmake ..
# Use this for debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

Once cmake has generated the build make files.

```
make
```

Flex
https://westes.github.io/flex/manual/
https://github.com/westes/flex

## Design Notes

BTree Layer Usage
It is the responsibility of the layer above the BTree Layer to define how data is serialized into the tree and how it is compared.

## Flex Notes

```
%option noyywrap

# This is required to have the scanner input object
%option reentrant

# Do not generate line directives. (This was causing debug errors)
%option noline
```