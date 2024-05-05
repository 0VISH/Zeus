#if(DBG)
#include <typeinfo>
#endif

//TODO: optimize these data structures

//array whose length is known at compiletime
template<typename T, u32 len>
struct StaticArray {
    T mem[len];

    T& operator[](u32 index) {
#if(DBG == true)
	if (index >= len) {
	    printf("\n[ERROR]: abc(static_array) failed for type %s. index = %d\n", typeid(T).name(), index);
	};
#endif
	return mem[index];
    };
};

//array whose length is not known at comptime
template<typename T>
struct Array {
    T *mem;
    u32 len;
    u32 count;

    Array(u32 length){
	count = 0;
	len = length;
	if(length == 0){
	    mem = nullptr;
	}else{
	    mem = (T*)mem::alloc(sizeof(T)*len);
	};
    };
    void uninit(){
	mem::free(mem);
    };
    T& operator[](u32 index) {
#if(DBG == true)
	if (index >= len) {
	    printf("\n[ERROR]: abc(array) failed for type %s. index = %d\n", typeid(T).name(), index);
	};
#endif
	return mem[index];
    };
    void push(const T &t){
	mem[count] = t;
	count += 1;
    };
};

//string
struct String {
    char *mem;
    u32 len;

    char operator[](u32 index) {
#if(DBG == true)
	if (index >= len) {
	    printf("\n[ERROR]: abc(string) failed. mem: %p, len: %d, index = %d\n", mem, len, index);
	};
#endif
	return mem[index];
    };
};
bool cmpString(String str1, String str2) {
    if (str1.len != str2.len) { return false; };
    //TODO: SIMD??
    return memcmp(str1.mem, str2.mem, str1.len) == 0;
};
bool cmpString(String str1, char *str2){
    u32 len = strlen(str2);
    if(str1.len != len){return false;};
    return memcmp(str1.mem, str2, len) == 0;
};

//dynamic array
template<typename T>
struct DynamicArray {
    T *mem;
    u32 count;
    u32 len;

    void zero(){
	count = 0;
	len = 0;
    }
    void realloc(u32 newCap) {
	void *newMem = mem::alloc(sizeof(T) * newCap);
	memcpy(newMem, mem, sizeof(T) * len);
	mem::free(mem);
	mem = (T*)newMem;
	len = newCap;
    };
    T &getElement(u32 index) {
#if(DBG == true)
	if (index >= len) {
	    printf("\n[ERROR]: abc(dynamic_array) failed for type %s. index = %d\n", typeid(T).name(), index);
	};
#endif
	return mem[index];
    };
    T &operator[](u32 index) { return getElement(index); };
    void init(u32 startCount = 5) {
	count = 0;
	len = startCount;
	mem = (T*)mem::alloc(sizeof(T) * startCount);
    };
    void uninit() { mem::free(mem); };
    void push(const T &t) {
	if (count == len) { realloc(len + len / 2 + 1); };
	mem[count] = t;
	count += 1;
    };
    T pop(){
	count -= 1;
	return mem[count];
    };
    T& newElem(){
	if (count == len) { realloc(len + (len/2) + 1); };
	count += 1;
	return mem[count-1];
    };
    void reserve(u32 rCount){
	if(count+rCount >= len){
	    realloc(count+rCount);
	}
    }
#if(DBG)
    void dumpStat() {
	printf("\n[DYNAMIC_ARRAY] mem: %p; count: %d; len: %d\n", mem, count, len);
    };
#endif
};

struct HashmapStr{
    String *keys;
    u32    *values;
    bool   *status;
    u32     count;
    u32     len;
    
    void init(u32 initialCapacity=10){
	len = initialCapacity;
	count = 0;
	keys = (String*)mem::alloc(sizeof(String)*len);
	values = (u32*)mem::alloc(sizeof(u32)*len);
	status = (bool*)mem::alloc(sizeof(bool)*len);
	memset(status, false, sizeof(bool)*len);
    };
    void uninit(){
	mem::free(keys);
	mem::free(values);
	mem::free(status);
    };
    u32 hashFunc(const String &key){
	//fnv_hash_1a_32
	u32 h = 0x811c9dc5;
	for(u32 i=0; i<key.len; i+=1){h = (h^key.mem[i]) * 0x01000193;};
	return h;
    };
    bool getValue(const String &key, u32 *value){
	u32 startHash = hashFunc(key) % len;
	u32 hash = startHash;
	while(status[hash] == true){
	    if(cmpString(key, keys[hash])){
		*value = values[hash];
		return true;
	    };
	    hash += 1;
	    if(hash >= len){hash = 0;};
	    if (hash == startHash){ return false; };
	};
	return false;
    };
    bool _insert_value(String &key, u32 value, u32 length, bool *statusArr, String *keysArr, u32 *valuesArr){
	u32 hash = hashFunc(key) % length;
	while(statusArr[hash]){
	    hash += 1;
	    if(hash >= length){hash = 0;};
	};
	statusArr[hash] = true;
	keysArr[hash] = key;
	valuesArr[hash] = value;
	return true;
    };
    bool insertValue(String key, u32 value){
	if(count == len){
	    u32 newLen = len + (u32)(len/2) + 10;
	    String *newKeys = (String*)mem::alloc(sizeof(String)*newLen);
	    u32 *newValues = (u32*)mem::alloc(sizeof(u32)*newLen);
	    bool *newStatus = (bool*)mem::alloc(sizeof(bool)*newLen);
	    memset(newStatus, false, sizeof(bool)*newLen);
	    for(u32 x=0; x<len; x+=1){
		String tempKey = keys[x];
		u32 val;
		getValue(tempKey, &val);
		_insert_value(tempKey, val, newLen, newStatus, newKeys, newValues);
	    };
	    len = newLen;
	    keys = newKeys;
	    values = newValues;
	    status = newStatus;
	};
	if(_insert_value(key, value, len, status, keys, values)){
	    count += 1;
	    return true;
	};
	return false;
    };
};
//NOTE: only for int types
template <typename T, typename J>
struct Hashmap{
    T    *keys;
    J    *values;
    bool *status;
    u32   count;
    u32   len;
    
    void init(u32 initialCapacity=10){
	len = initialCapacity;
	count = 0;
	keys = (T*)mem::alloc(sizeof(T)*len);
	values = (J*)mem::alloc(sizeof(J)*len);
	status = (bool*)mem::alloc(sizeof(bool)*len);
	memset(status, false, sizeof(bool)*len);
    };
    void uninit(){
	mem::free(keys);
	mem::free(values);
	mem::free(status);
    };
    u32 hashFunc(char *key){
	//fnv_hash_1a_32
	u32 h = 0x811c9dc5;
	for(u32 i=0; i<sizeof(T); i+=1){h = (h^key[i]) * 0x01000193;};
	return h;
    };
    bool getValue(T key, J *value){
	u32 startHash = hashFunc((char*)&key) % len;
	u32 hash = startHash;
	while(status[hash] == true){
	    if(key == keys[hash]){
		*value = values[hash];
		return true;
	    };
	    hash += 1;
	    if(hash >= len){hash = 0;};
	    if (hash == startHash){ return false; };
	};
	return false;
    };
    bool _insert_value(T key, J value, u32 length, bool *statusArr, T *keysArr, u32 *valuesArr){
	u32 hash = hashFunc((char*)&key) % length;
	while(statusArr[hash]){
	    hash += 1;
	    if(hash >= length){hash = 0;};
	};
	statusArr[hash] = true;
	keysArr[hash] = key;
	valuesArr[hash] = value;
	return true;
    };
    bool insertValue(T key, J value){
	if(count == len){
	    u32 newLen = len + (u32)(len/2) + 10;
	    T *newKeys = (T*)mem::alloc(sizeof(T)*newLen);
	    J *newValues = (J*)mem::alloc(sizeof(u32)*newLen);
	    bool *newStatus = (bool*)mem::alloc(sizeof(bool)*newLen);
	    memset(newStatus, false, sizeof(bool)*newLen);
	    for(u32 x=0; x<len; x+=1){
		T tempKey = keys[x];
		J val;
		getValue(tempKey, &val);
		_insert_value(tempKey, val, newLen, newStatus, newKeys, newValues);
	    };
	    len = newLen;
	    keys = newKeys;
	    values = newValues;
	    status = newStatus;
	};
	if(_insert_value(key, value, len, status, keys, values)){
	    count += 1;
	    return true;
	};
	return false;
    };
};