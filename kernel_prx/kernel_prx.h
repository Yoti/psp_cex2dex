int prxIdStorageDeleteLeaf(u16 key);
int prxIdStorageLookup(u16 key, u32 offset, void *buf, u32 len);
int prxIdStorageReadLeaf(u16 key, void *buf);
int prxIdStorageWriteLeaf(u16 key, void *buf);
u64 prxSysregGetFuseId(void);
