#include <stdio.h>
#include <stdlib.h>
#include "disk.h"
#include "hw1.h"

#include "string.h"

void FileSysInit(void)
{
    DevCreateDisk();

    char* tmp = malloc(BLOCK_SIZE);
    memset(tmp, 0, BLOCK_SIZE);

    for(int i = 0; i < 512; i++)    //원래 i<7이었는데 i<512로 수정
        DevWriteBlock(i, tmp);

    free(tmp);
}

void SetInodeBytemap(int inodeno)
{
    char* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(1, pbuf);

    pbuf[inodeno] = 1;
    DevWriteBlock(1, pbuf);

    free(pbuf);
}


void ResetInodeBytemap(int inodeno)       
{
    char* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(1, pbuf);

    pbuf[inodeno] = 0;
    DevWriteBlock(1, pbuf);

    free(pbuf);
}


void SetBlockBytemap(int blkno)
{
    char* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(2, pbuf);

    pbuf[blkno] = 1;
    DevWriteBlock(2, pbuf);

    free(pbuf);
}


void ResetBlockBytemap(int blkno)
{
    char* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(2, pbuf);

    pbuf[blkno] = 0;
    DevWriteBlock(2, pbuf);

    free(pbuf);
}


void PutInode(int inodeno, Inode* pInode)
{
    char* pbuf = malloc(BLOCK_SIZE);
    int blkno = 3 + inodeno / 16;
    DevReadBlock(blkno, pbuf);

    int number = inodeno % 16;
    Inode* inodeBlk = (Inode*)pbuf;
    inodeBlk[number] = *pInode;

    DevWriteBlock(blkno, pbuf);

    free(pbuf);
}


void GetInode(int inodeno, Inode* pInode)
{
    char* pbuf = malloc(BLOCK_SIZE);
    int blkno = 3 + inodeno / 16;
    DevReadBlock(blkno, pbuf);

    int number = inodeno % 16;
    Inode* inodeBlk = (Inode*)pbuf;
    *pInode = inodeBlk[number];

    free(pbuf);
}


int GetFreeInodeNum(void)
{
    char* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(1, pbuf);

    int i;
    for(i = 0; ; i++)
    {   
        if(pbuf[i] == 0)
            break;
    }
    
    free(pbuf);
    return i;
}


int GetFreeBlockNum(void)
{
    char* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(2, pbuf);

    int i;
    for(i = 11; ; i++)
    {   
        if(pbuf[i] == 0)
            break;
    }
    
    free(pbuf);
    return i;
}

void PutIndirectBlockEntry(int blkno, int index, int number)
{
    int* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, (char*)pbuf);

    pbuf[index] = number;
    DevWriteBlock(blkno, (char*)pbuf);

    free(pbuf);
}

int GetIndirectBlockEntry(int blkno, int index)
{  
    int* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, (char*)pbuf);

    int result = pbuf[index];
    free(pbuf);
    return result;
}

void PutDirEntry(int blkno, int index, DirEntry* pEntry)
{
    DirEntry* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, (char*)pbuf);

    pbuf[index] = *pEntry;
    DevWriteBlock(blkno, (char*)pbuf);

    free(pbuf);
}

int GetDirEntry(int blkno, int index, DirEntry* pEntry)
{
    DirEntry* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, (char*)pbuf);

    *pEntry = pbuf[index];
    if(pbuf[index].inodeNum == INVALID_ENTRY){
        free(pbuf);
        return INVALID_ENTRY;
    }
    else{
        free(pbuf);
        return 1;
    }
}

void RemoveIndirectBlockEntry(int blkno, int index)
{
    int* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, (char*)pbuf);

    pbuf[index] = INVALID_ENTRY;
    DevWriteBlock(blkno, (char*)pbuf);

    free(pbuf);
}

void RemoveDirEntry(int blkno, int index)
{
    DirEntry* pbuf = malloc(BLOCK_SIZE);
    DevReadBlock(blkno, (char*)pbuf);

    pbuf[index].inodeNum = INVALID_ENTRY;
    DevWriteBlock(blkno, (char*)pbuf);

    free(pbuf);
}