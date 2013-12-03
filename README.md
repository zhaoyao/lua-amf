

Internals
---
1. Untyped tables are encoded as anonymous dynamic object, and do not keep reference for the traits.

2. with key __class typed tables are encoded as typed object in AMF3

3. typed objects are decoded as typed tables with key __class to store the full class name

Todo:
---
1. External table.
2. Compile flag: AMF_ASSERT
3. Encode/decode trace
