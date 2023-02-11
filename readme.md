## Building

Build uses cmake. 

```
mkdir build
cd build
cmake ..
# Use this for debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
```

Once cmake has generated the build make files. You also need to generate the lex source files. 
Requires flex to be available.

```
cd bison
make
```

Once the lexer source files have been generated, navigate back to build and run `make`


Flex
https://westes.github.io/flex/manual/
https://github.com/westes/flex



## Flex Notes

```
%option noyywrap

# This is required to have the scanner input object
%option reentrant

# Do not generate line directives. (This was causing debug errors)
%option noline
```

## Remaining features

In no particular order.

1. Proper write caching. All writes immediate write to disk.
2. Pager free list.
3. Traverse with early splitting and early merging.
4. Query Planner
5. Delete operation