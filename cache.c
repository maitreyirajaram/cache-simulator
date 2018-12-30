#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>  

//cache block in set
struct block {
	unsigned long long int tag; 
	int validBit; 
	long int LRUaccessBit; 
	struct block *next;
}; 

//global vars
struct block** CACHE; 
//counters
long int memoryReadCounter = 0; 
long int memoryWriteCounter = 0;
long int cacheHits = 0; 
long int cacheMiss = 0;  
//prefetch/lru flags
int DO_PREFETCH = 0;//DO PREFETCH IS INITIALIZED TO FALSE 
int DO_LRU = 0; //DO LRU IS INITIALIZED TO FALSE 
long int lruCounter = 0; 

//methods for calculating bits
//blockoffset bits
int getBlockOffsetBits(int blocksize) {
	int blockOffsetBits = log2(blocksize); 
	return blockOffsetBits;
}
//set index bits
int getIndexBits(int numSets) {
	int indexBits = log2(numSets); 
	return indexBits;
}
//tag bits
int getTagBits(int blockOffset, int index) {
	int tagBits = 48 - (blockOffset + index); 
	return tagBits;
}


//extracting memory address parts 
unsigned long long int getBlockOffset(int offsetBits, unsigned long long int address){
	unsigned long long int blockOffset = address % (long)pow(2, offsetBits); 
	return blockOffset; 
}
unsigned long long int getSetIndex(int offsetBits, int setBits, unsigned long long int address) {
	if (setBits == 0) { //fully assoc, 1 set, setbits = 0, all blocks indexed to "0"
		return 0; 
	}
	address = address >> offsetBits; 
	unsigned long long int setIndex = address % (long)pow(2, setBits); 
	return setIndex; 
}
unsigned long long int getTag(int offsetBits, int setBits, unsigned long long int address){
	return address >> (offsetBits + setBits); 
}

//getting set number given assoc:n
int getNumSets(char * assoc){
	char* numberOfSets = (char*) malloc(strlen(assoc)); 
	int i; 
	for (i = 6; i < strlen(assoc); i++){
		numberOfSets[i-6] = assoc[i];
	}
	numberOfSets[i-6] = '\0';
	return atoi(numberOfSets);
}

//searching for tag in given set
int findInCacheSet(unsigned long long int tag, unsigned long long int setIndex, int prefetch){
	if (CACHE[setIndex] -> validBit == 0) { 
		return 1; //MISS no items in set yet
	}
	struct block* ptr = CACHE[setIndex];
	while(ptr -> next != NULL){ //LINKED LIST TRAVERSAL
		if (ptr -> tag == tag) {
		if (prefetch ==0){
			ptr -> LRUaccessBit = lruCounter; 
			lruCounter ++;}
			return 0; //hit
		}
		ptr = ptr->next;
	}
	if (ptr -> tag == tag) {
		if (prefetch ==0) {
		ptr -> LRUaccessBit = lruCounter; 
		lruCounter ++;}
		return 0; 
	}
	return 1; 
}

//writing to cache
int writeToCache(unsigned long long int address, int numSets, int numBlocksPerSet, int blockSize, int prefetch) { // 1 = miss, 0 = hit
	int blockOffsetBits = getBlockOffsetBits(blockSize);
	int indexBits = getIndexBits(numSets);
	unsigned long long int setIndex = getSetIndex(blockOffsetBits,indexBits, address); 
	unsigned long long int tag = getTag(blockOffsetBits,indexBits, address);  
	if (findInCacheSet(tag, setIndex, prefetch) == 0) {
		if (prefetch == 0) {
			cacheHits++;
		}
		return 0;//already present, no write required
		}
	struct block* newBlock = malloc(sizeof(struct block));
	newBlock -> tag = tag; newBlock -> next = NULL; newBlock -> validBit = 1; 
	struct block* ptr = CACHE[setIndex]; 
	if (ptr -> validBit == 0){ //first block in set
		newBlock -> LRUaccessBit = lruCounter; 
		lruCounter ++;
		CACHE[setIndex] = newBlock; 
		memoryReadCounter++;
		if (prefetch == 0) {
			cacheMiss++;
		}
		return 1; //successful write
	}
	for (int x =0; x <numBlocksPerSet-1; x++){ //fixed loop, cannot exceed num blocks/set
		if (ptr -> next == NULL){
			ptr -> next = newBlock; 
			ptr -> next -> LRUaccessBit = lruCounter; 
			lruCounter ++;
			memoryReadCounter++;
			if (prefetch == 0) {
				cacheMiss++;
			} 
			return 1; //successful write
		}
		ptr = ptr -> next; 
	}
	 //eviction required, ptr at last block
	if (DO_LRU == 1) {//eviction with lru 
		struct block* min = CACHE[setIndex];  
		ptr = CACHE[setIndex]; 
		while (ptr != NULL){
			if ((ptr -> LRUaccessBit) < (min -> LRUaccessBit)) {
				min = ptr; 
			}
			ptr = ptr-> next; 
		}
		//ptr is now minimum lru, ptr == null
		min -> tag= newBlock -> tag; //change tags
		min -> LRUaccessBit = lruCounter; 
		lruCounter ++;
	}
		 
	else{
	//must evict- FIFO
	ptr -> next = newBlock; 
	struct block* head = CACHE[setIndex];
	CACHE[setIndex] =  head-> next;
	 }//new head, front is evicted 
	
	//counters
	memoryReadCounter++;
	if (prefetch == 0) {
		cacheMiss++;
	}
	return 1; //write in
}

//read into cache
int readCache(unsigned long long int address, int numSets, int numBlocksPerSet, int blockSize){
	int blockOffsetBits = getBlockOffsetBits(blockSize);
	int indexBits = getIndexBits(numSets); 
	unsigned long long int setIndex = getSetIndex(blockOffsetBits,indexBits, address); 
	unsigned long long int tag = getTag(blockOffsetBits,indexBits, address);  
	struct block* ptr = CACHE[setIndex];
	if (ptr -> validBit == 0) { //no items in set yet
		writeToCache(address, numSets, numBlocksPerSet, blockSize, 0); //read from mem, write to cache
		return 1; //miss
	}
	while(ptr -> next != NULL){ //LINKED LIST TRAVERSAL
		if (ptr -> tag == tag) {
			ptr -> LRUaccessBit = lruCounter; 
			lruCounter ++; 
			cacheHits ++;
			return 0; //hit
		}
		ptr = ptr->next;
	}

	if (ptr -> tag == tag) {
		cacheHits ++;
		ptr -> LRUaccessBit = lruCounter; 
		lruCounter ++;
		return 0; //hit
	}
	writeToCache(address, numSets, numBlocksPerSet, blockSize, 0);
	return 1; //miss
}
//allocate cache and initialize valid bit
void initializeCache(int numSets){
	CACHE = (struct block**) malloc(sizeof(struct block*)* numSets); 
	for (int i  =0; i <numSets; i++){
		CACHE[i] = (struct block*) malloc(sizeof(struct block)); 
		CACHE[i] -> validBit = 0; CACHE[i] -> LRUaccessBit = 0; 
		CACHE[i] -> next = NULL; 
	}
}
	
//clean CACHE
void cleanCache(int numSets){
	for (int i  =0; i <numSets; i++){
		struct block* ptr = CACHE[i]; 
		struct block* ptrNext; 
		while(ptr != NULL) {
			ptrNext = ptr -> next;
			free(ptr); 
			ptr = ptrNext; 
		}
	}
	
	//clean counters
	memoryReadCounter = 0; memoryWriteCounter = 0;cacheHits = 0; cacheMiss = 0; lruCounter = 0; 
}

//prefetch method 
void prefetch(unsigned long long int address, int numSets, int numBlocksPerSet, int blockSize) { 
	if (DO_PREFETCH == 1) {
    	writeToCache(address + blockSize, numSets, numBlocksPerSet, blockSize, 1);
    }
}

int readFile(char* fileName,int numSets, int numBlocksPerSet, int blockSize ) {
	FILE *fp = fopen(fileName, "r"); 
	char* ignored = malloc(sizeof(char)*100); //PC in trace file, irrelevant to program
	char* readOrWrite = malloc(sizeof(char)*100); 
	unsigned long long int address=0; // 48 bits 
	while (fscanf(fp,"%s%s%llx", ignored, readOrWrite, &address) > 0){ 
	     if (strcmp(ignored, "#eof")==0) {
	     	break;
	     }
		//write
		if (strcmp(readOrWrite,"W") == 0) {
			memoryWriteCounter++;
			if (writeToCache(address, numSets, numBlocksPerSet, blockSize, 0) == 1){ //miss
				prefetch(address, numSets, numBlocksPerSet, blockSize);
			}	 
		}
		//read
		else if (strcmp(readOrWrite, "R") == 0) { 
			if (readCache(address, numSets, numBlocksPerSet, blockSize)==1){ //miss
				prefetch(address, numSets, numBlocksPerSet, blockSize); 
			}
		}
		else {
			printf("error");
			return 1;
		}
	}
	
	//PRINT RESULTS
	if (DO_PREFETCH == 0) {
		printf("no-prefetch\n");
	}
	else{
		printf("with-prefetch\n");
	}
	printf("Memory reads: %ld", memoryReadCounter);
	printf("\nMemory writes: %ld", memoryWriteCounter);
	printf("\nCache hits: %ld", cacheHits);
	printf("\nCache misses: %ld\n", cacheMiss);
	return 0;
}

//  INPUT FORMAT (command line): 
//  ./first <cache size><associativity><cache policy><block size><trace file>

int main(int argc, char** argv) {
	int cacheSize = atoi(argv[1]);  
	
	//determining associativity 
	char* assoc = argv[2];
	char* cachePolicy = argv[3];//FIFO or LRU 
	if (strcmp(cachePolicy, "lru") ==0) {
		DO_LRU = 1; //lru implement
	}
	int blockSize = atoi(argv[4]); 
	int numBlocksPerSet;
	int numSets;
	if (strcmp(assoc, "direct") == 0) {
		numSets = cacheSize/blockSize;//assoc = 1  
		numBlocksPerSet = 1;
	}
	else if (strcmp(assoc, "assoc") == 0){
		//fully assoc cache
		numSets = 1;
		numBlocksPerSet = cacheSize/blockSize;
	}
	else {
		//n way assoc cache
		numBlocksPerSet = getNumSets(assoc);
		numSets = cacheSize/(numBlocksPerSet* blockSize);
		
	}
	
	
	//TEST, print cache specifications 
	//printf("cache size = %d block size = %d numBlocks = %d num Sets = %d", cacheSize, blockSize, numBlocksPerSet, numSets);
	
	
	//initialize cache
	initializeCache(numSets); 
	//run 1 without prefetch
	if (readFile(argv[5], numSets, numBlocksPerSet, blockSize)==1){ //error in file
		return 1; 
	}
	//clean cache
	cleanCache(numSets);
	//initialize cache
	initializeCache(numSets);
	//run 2 with prefetch 
	DO_PREFETCH = 1;
	if (readFile(argv[5], numSets, numBlocksPerSet, blockSize)==1){
		return 1;
	}
	
	return 0; 
}


