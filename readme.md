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

## Design Notes

While writing the code for btree

1. Different struct for Cursor Index and InsertionIndex - call it LeftInsertionIndex.
2. Ran into a fun edge case, ff we allow payloads to take up more than 1/2 of a page's free space.
```
If we allow payloads to take up more than 1/2 of a page's free space, then we might end up in this situation below. Imagine a leaf page X with paylods a.2P (where .5P is 1/2 the useable size). and c.7P.

Like this X = [a.2P, b.7P]. Now imagine we want to insert b.9P. The normal algorithm is to split X, in X1 and X2.
X1 = [a.2P]
X2 = [c.7P]

Then try to insert b.9P into X1 or X2 depending on where b.9P is sorted.

But Size(X1 + b.9P) > P and Size(X2 + b.9P) > P, so we'eve split this page but we still can't fit b.9P.
Suppose we could recursively split the pages until we split a page with 1 element, and are left with a page with 1 element and an empty page, then we insert b.9P into the empty page. But that seems really complicated.
```
3. I don't like making assumptions about the memory layout of C structs. Instead of just mapping C structs from memory to disk, I want to manually serialize and deserialize.
4. Change stable parent split to be the normal split + a swap. I.e. split root page. What to do about page 1 header?
5. Cell pointers at the start of each node shouldn't contain the key, they should just be pointers in sorted order pointing to cells.