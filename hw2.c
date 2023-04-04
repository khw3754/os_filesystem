#include <stdio.h>
#include <stdlib.h>
#include "disk.h"
#include "hw1.h"
#include "hw2.h"

#include "string.h"

FileDescTable* pFileDescTable;
FileSysInfo* pFileSysInfo;

FileTable* pFileTable;          //여기 임의 추가
int openCall = 0;

Directory* saveDir;
int saveIndex;


int LogicalBlock(int i, DirEntry* dirBlock, Inode* pInode);
int AddBlock(Inode* pInode, int inodeNum);


int OpenFile(const char* name, OpenFlag flag)
{

    //파일 생성(1)
    int freeInode = GetFreeInodeNum();

    //파일 생성(2),(3)
    //path가 '/'로 시작하지 않으면 fail
    if(name[0] != '/')
        return -1;

    
    char nameArr[100];  //name을 동적 문자열로 변환해줌 ---------------- @@@@@@@@@@@경로 제한 생김
    strcpy(nameArr, name);

    char* path = strtok(nameArr, "/");
    int inodeNum = 0;
    Inode* pInode = malloc(sizeof(Inode));
    DirEntry* dirBlock = malloc(BLOCK_SIZE);
    int find, blknum;
    while(path != NULL)
    {
        //먼저 path에 해당하는 file을 찾음
        find = 0;
        GetInode(inodeNum, pInode);
        for(int i = 0; i < pInode->allocBlocks && find == 0; i++){
            blknum = LogicalBlock(i, dirBlock, pInode);
            for(int j = 0; j < 8; j++){
                if(strcmp(dirBlock[j].name, path) == 0 && dirBlock[j].inodeNum != INVALID_ENTRY){
                    inodeNum = dirBlock[j].inodeNum;
                    find = 1;
                    break;
                }
            }
        }
        char* nextPath = strtok(NULL, "/");


        //찾지 못했고 다음 path가 없고 flag가 OPEN_FLAG_CREATE라면, 즉 Path의 file을 추가해야 하는 경우
        if(find == 0 && nextPath == NULL && flag == OPEN_FLAG_CREATE){
            //빈 entry를 찾음
            for(int i = 0; i < 8; i++){
                if(dirBlock[i].inodeNum == INVALID_ENTRY)        //빈 entry를 찾으면 그곳에 name, inodeNum을 넣고 
                {
                    strcpy(dirBlock[i].name, path);
                    dirBlock[i].inodeNum = freeInode;
                    DevWriteBlock(blknum, (char*)dirBlock);     //저장
                    break;
                }
                else if(i == 7){       //빈 entry가 없으면 block을 추가하고 거기에 directory 추가
                    int addblocknum = AddBlock(pInode, inodeNum);     //블록 추가
                    DevReadBlock(addblocknum, (char*)dirBlock);
                    dirBlock[0].inodeNum = freeInode;
                    strcpy(dirBlock[0].name, path);

                    DevWriteBlock(addblocknum, (char*)dirBlock);    //저장
                }
            }

            
            //파일 생성(4)
            GetInode(freeInode, pInode);
            pInode->allocBlocks = 0;
            pInode->size = 0;
            pInode->type = FILE_TYPE_FILE;
            PutInode(freeInode, pInode);

            SetInodeBytemap(freeInode);

    
            //파일 생성(5)
            FileSysInfo* superBlock = malloc(BLOCK_SIZE);
            DevReadBlock(0, (char*)superBlock);
            superBlock->numAllocInodes++;
            DevWriteBlock(0, (char*)superBlock);


            inodeNum = freeInode;
        }
        //찾지 못 했는데 flag가 OPEN_FLAG_CREATE가 아닐 경우 - 실패
        else if(find == 0 && nextPath == NULL && flag != OPEN_FLAG_CREATE)
            return -1;
        //찾지 못 했는데 그게 중간 path인 경우 - 실패
        else if(find == 0 && nextPath != NULL)
            return -1;
        //이미 그 file이 있는 경우 --------------------------------여기 새로 작성
        else if(find == 1 && nextPath == NULL){
            //flag가 TRUNCATE면 파일의 size를 0으로 만들고 block들을 모두 반환함
            if(flag == OPEN_FLAG_TRUNCATE){
                Inode* truncInode = malloc(sizeof(Inode));
                GetInode(inodeNum, truncInode);
                
                DirEntry* t = malloc(BLOCK_SIZE);
                for(int i = 0; i < truncInode->allocBlocks; i++){
                    int blk = LogicalBlock(i, t, truncInode);
                    ResetBlockBytemap(blk);
                }
                //@@@@@@@@@@@ 추가: indirect도 있으면 그것도 반환 @@@@@@@@@@
                if(truncInode->allocBlocks > 4){
                    ResetBlockBytemap(truncInode->indirectBlockPtr);

                    FileSysInfo* superBlock = malloc(BLOCK_SIZE);
                    DevReadBlock(0, (char*)superBlock);
                    superBlock->numAllocBlocks--;
                    superBlock->numFreeBlocks++;
                    DevWriteBlock(0, (char*)superBlock);
                    free(superBlock);
                }

                FileSysInfo* superBlock = malloc(BLOCK_SIZE);
                DevReadBlock(0, (char*)superBlock);
                superBlock->numAllocBlocks -= truncInode->allocBlocks;
                superBlock->numFreeBlocks += truncInode->allocBlocks;
                DevWriteBlock(0, (char*)superBlock);
                free(superBlock);

                truncInode->allocBlocks = 0;
                truncInode->size = 0;
                PutInode(inodeNum, truncInode);
                free(truncInode);
            }
        }
        

        path = nextPath;
    }


    //파일 생성(6) / 파일 열기
    //open을 처음 호출한다면 table들 생성
    if(openCall == 0){
        pFileDescTable = malloc(sizeof(FileDescTable));
        pFileTable = malloc(sizeof(FileTable));

        pFileDescTable->numUsedDescEntry = 0;
        for(int i = 0; i < DESC_ENTRY_NUM; i++)
            pFileDescTable->pEntry[i].bUsed = 0;
        pFileTable->numUsedFile = 0;
        for(int i = 0; i < MAX_FILE_NUM; i++)
            pFileTable->pFile[i].bUsed = 0;

        openCall = 1;
    }

    //먼저 각 table의 빈 entry를 찾는다
    int freeDiscriptor, freeFileTable;
    for(int i = 0; i < DESC_ENTRY_NUM; i++){
        if(pFileDescTable->pEntry[i].bUsed == 0){
            freeDiscriptor = i;
            break;
        }
    }
    for(int i = 0; i < MAX_FILE_NUM; i++){
        if(pFileDescTable->pEntry[i].bUsed == 0){
            freeFileTable = i;
            break;
        }
    }

    File* pfile = malloc(sizeof(File));
    pfile->bUsed = 1;
    pfile->fileOffset = 0;
    pfile->inodeNum = inodeNum;                                  
    memcpy(&(pFileTable->pFile[freeFileTable]), pfile, sizeof(File));
    pFileTable->numUsedFile++;
    free(pfile);
    
    pFileDescTable->pEntry[freeDiscriptor].bUsed = 1;
    pFileDescTable->pEntry[freeDiscriptor].fileTableIndex = freeFileTable;
    pFileDescTable->numUsedDescEntry++;

    return freeDiscriptor;
}


int WriteFile(int fileDesc, char* pBuffer, int length)
{
    //파일 쓰기(2)
    Inode* fileInode = malloc(sizeof(Inode));
    int fileInodeNum;
    if(pFileDescTable->pEntry[fileDesc].bUsed == 0)
        return -1;
    int ftindex = pFileDescTable->pEntry[fileDesc].fileTableIndex;
    if(pFileTable->pFile[ftindex].bUsed == 0)
        return -1;
    fileInodeNum = pFileTable->pFile[ftindex].inodeNum;
    GetInode(fileInodeNum, fileInode);

    //아직 file에 할당된 block이 없으면 할당하고 처리
    if(fileInode->allocBlocks == 0){
        int freeBlock = GetFreeBlockNum();                                 
        fileInode->dirBlockPtr[0] = freeBlock;
        fileInode->allocBlocks++;
        fileInode->size = 512;
        SetBlockBytemap(freeBlock);

        FileSysInfo* superBlock = malloc(BLOCK_SIZE);
        DevReadBlock(0, (char*)superBlock);
        superBlock->numAllocBlocks++;
        superBlock->numFreeBlocks--;
        DevWriteBlock(0, (char*)superBlock);
        free(superBlock);

        PutInode(fileInodeNum, fileInode);
    }


    //파일 쓰기(3)
    int originalOffset = pFileTable->pFile[ftindex].fileOffset;

    //pBuf의 내용을 file의 block에 복사하는 과정
    //만약 새 block 할당이 필요하다면 AddBlock 함수를 활용해서 새 block을 할당함                    
    if(((double)originalOffset + (double)length) / (double)BLOCK_SIZE > fileInode->allocBlocks){
        //2개이상 추가 구현
        int ceil = ((double)originalOffset + (double)length) / (double)BLOCK_SIZE;
        if(ceil != ((double)originalOffset + (double)length) / (double)BLOCK_SIZE )
            ceil++;
        for(int k = fileInode->allocBlocks; k < ceil; k++){
            int newBlockNum = AddBlock(fileInode, fileInodeNum);
            char* newBlock = malloc(BLOCK_SIZE);
            DevReadBlock(newBlockNum, newBlock);
            memset(newBlock, 0, BLOCK_SIZE);
            DevWriteBlock(newBlockNum, newBlock);
        }
    }


    //
    //수정버전
    //
    char* writeBlock = malloc(BLOCK_SIZE);
    int logicalNum = originalOffset / BLOCK_SIZE;
    int writeBlockNum = LogicalBlock(logicalNum, (DirEntry*)writeBlock, fileInode);
    //printf("num: %d\n", writeBlockNum);
    int writeBlockOffset = originalOffset % BLOCK_SIZE;
    //printf("offset: %d\n", writeBlockOffset);

    
    if(length <= BLOCK_SIZE)
        memcpy(writeBlock + writeBlockOffset, pBuffer, length);
    else
        memcpy(writeBlock + writeBlockOffset, pBuffer, BLOCK_SIZE);

    DevWriteBlock(writeBlockNum, writeBlock);

    pFileTable->pFile[ftindex].fileOffset += length;
    return length;
}


int ReadFile(int fileDesc, char* pBuffer, int length)
{
    //파일 쓰기(2)
    Inode* fileInode = malloc(sizeof(Inode));
    int fileInodeNum;
    if(pFileDescTable->pEntry[fileDesc].bUsed == 0)
        return -1;
    int ftindex = pFileDescTable->pEntry[fileDesc].fileTableIndex;
    if(pFileTable->pFile[ftindex].bUsed == 0)
        return -1;
    fileInodeNum = pFileTable->pFile[ftindex].inodeNum;
    GetInode(fileInodeNum, fileInode);

    //아직 file에 할당된 block이 없으면 - 0 반환
    if(fileInode->allocBlocks == 0){
        memset(pBuffer, 0, length);
        return 0;
    }


    //파일 쓰기(3)
    int originalOffset = pFileTable->pFile[ftindex].fileOffset;

    //file의 내용을 pBuf에 복사하는 과정
    //만약 offset이 파일의 크기보다 크면 - 0 반환
    if(originalOffset >= fileInode->size){
        memset(pBuffer, 0, length);
        return 0;
    }


    //
    //수정버전
    //
    char* readBlock = malloc(BLOCK_SIZE);
    int logicalNum = originalOffset / BLOCK_SIZE;
    int readBlockNum = LogicalBlock(logicalNum, (DirEntry*)readBlock, fileInode);
    //printf("num: %d\n", readBlockNum);
    int readBlockOffset = originalOffset % BLOCK_SIZE;
    //printf("offset: %d\n", writeBlockOffset);

    memcpy(pBuffer, readBlock + readBlockOffset, length);       


    pFileTable->pFile[ftindex].fileOffset += length;
    return length;
}

int CloseFile(int fileDesc)
{
    int closeNum = pFileDescTable->pEntry[fileDesc].fileTableIndex;
    pFileTable->pFile[closeNum].bUsed = 0;
    pFileTable->numUsedFile--;

    pFileDescTable->pEntry[fileDesc].bUsed = 0;
    pFileDescTable->numUsedDescEntry--;

    return 0;
}

int RemoveFile(char* name)
{
    //path가 '/'로 시작하지 않으면 fail
    if(name[0] != '/')
        return -1;

    
    char nameArr[100];  //name을 동적 문자열로 변환해줌 ---------------- @@@@@@@@@@@@@@@경로 제한 생김
    strcpy(nameArr, name);

    char* path = strtok(nameArr, "/");
    int inodeNum = 0;
    Inode* pInode = malloc(sizeof(Inode));
    DirEntry* dirBlock = malloc(BLOCK_SIZE);
    int find, blknum, parentInode = 0;
    while(path != NULL)
    {
        //먼저 path에 해당하는 dir를 찾음
        find = 0;
        GetInode(inodeNum, pInode);
        for(int i = 0; i < pInode->allocBlocks && find == 0; i++){
            blknum = LogicalBlock(i, dirBlock, pInode);
            for(int j = 0; j < 8; j++){
                if(strcmp(dirBlock[j].name, path) == 0 && dirBlock[j].inodeNum != INVALID_ENTRY){
                    parentInode = inodeNum;                            
                    inodeNum = dirBlock[j].inodeNum;
                    find = 1;
                    break;
                }
            }
        }
        char* nextPath = strtok(NULL, "/");


        //찾지 못한 경우 - 실패
        if(find == 0)
            return -1;
        //그 file을 찾은 경우, 즉 그 file을 삭제해야 하는 경우
        else if(find == 1 && nextPath == NULL){
            Inode* targetInode = malloc(sizeof(Inode));
            int targetblk;
            
            //그 target의 inode와 block을 targetInode/ targetdir로 가져옴
            int j = 0;
            for(j = 0; j < 8; j++){
                if(strcmp(dirBlock[j].name, path) == 0){
                    GetInode(dirBlock[j].inodeNum, targetInode);
                    break;
                }
            }


            
            //target의 inode와 block을 반환함
            ResetInodeBytemap(dirBlock[j].inodeNum);
            DirEntry* t = malloc(BLOCK_SIZE);
            for(int i = 0; i < targetInode->allocBlocks; i++){
                targetblk = LogicalBlock(i, t, targetInode);
                ResetBlockBytemap(targetblk);
            }
            if(targetInode->allocBlocks > 4){
                ResetBlockBytemap(targetInode->indirectBlockPtr);
                FileSysInfo* superBlock = malloc(BLOCK_SIZE);
                DevReadBlock(0, (char*)superBlock);
                superBlock->numAllocBlocks--;
                superBlock->numFreeBlocks++;
                DevWriteBlock(0, (char*)superBlock);
                free(superBlock);
            }


            //상위 dir에서 invalid 처리하고 저장                               
            dirBlock[j].inodeNum = INVALID_ENTRY;      
            

            //삭제하고 나서 그 block의 뒤 entry들을 한 칸 씩 앞으로 땡기고 block 저장
            for(int i = j + 1; i < 8; i++){
                dirBlock[i-1].inodeNum = dirBlock[i].inodeNum;
                strcpy(dirBlock[i-1].name, dirBlock[i].name);
            }
            dirBlock[7].inodeNum = INVALID_ENTRY;
            DevWriteBlock(blknum, (char*)dirBlock);

            DirEntry* tmp = malloc(BLOCK_SIZE);
            int logicalBlockNum;
            for(int i = 0; i < pInode->allocBlocks; i++){
                if(blknum == LogicalBlock(i, tmp, pInode)){
                    logicalBlockNum = i;
                    break;
                }
            }
            

            DirEntry* tmp2 = malloc(sizeof(DirEntry));
            while(logicalBlockNum + 1 < pInode->allocBlocks){
                logicalBlockNum++;
                GetDirEntry(LogicalBlock(logicalBlockNum, tmp, pInode), 0, tmp2);
                PutDirEntry(LogicalBlock(logicalBlockNum - 1, tmp, pInode), 7, tmp2);
                tmp2->inodeNum = INVALID_ENTRY;
                PutDirEntry(LogicalBlock(logicalBlockNum, tmp, pInode), 0, tmp2);
                
                int blk = LogicalBlock(logicalBlockNum, tmp, pInode);
                for(int i = 1; i < 8; i++){
                    tmp[i-1].inodeNum = tmp[i].inodeNum;
                    strcpy(tmp[i-1].name, tmp[i].name);
                }
                tmp[7].inodeNum = INVALID_ENTRY;

                DevWriteBlock(blk, (char*)tmp);
            }


            //모두 땡겼고 마지막 block이 비었는지 확인하고 비었으면 반환
            int blk = LogicalBlock(pInode->allocBlocks - 1, tmp, pInode);
            if(tmp[0].inodeNum == INVALID_ENTRY){
                ResetBlockBytemap(blk);
                pInode->allocBlocks--;
                pInode->size -= 512;


                //여기 새로 추가 : indirect가 모두 비었으면 그것도 반환
                if(pInode->allocBlocks == 4){
                    ResetBlockBytemap(pInode->indirectBlockPtr);

                    FileSysInfo* superBlock = malloc(BLOCK_SIZE);
                    DevReadBlock(0, (char*)superBlock);
                    superBlock->numAllocBlocks--;
                    superBlock->numFreeBlocks++;
                    DevWriteBlock(0, (char*)superBlock);
                    free(superBlock);
                }


                PutInode(parentInode, pInode);

                //super block 처리
                FileSysInfo* superBlock = malloc(BLOCK_SIZE);
                DevReadBlock(0, (char*)superBlock);
                superBlock->numAllocBlocks--;
                superBlock->numFreeBlocks++;
                DevWriteBlock(0, (char*)superBlock);
            }
            
            

            FileSysInfo* superBlock = malloc(BLOCK_SIZE);
            DevReadBlock(0, (char*)superBlock);
            superBlock->numAllocBlocks -= targetInode->allocBlocks;
            superBlock->numFreeBlocks += targetInode->allocBlocks;
            superBlock->numAllocInodes--;
            DevWriteBlock(0, (char*)superBlock);

            return 0;
            
        }

        path = nextPath;
    }
}


int MakeDirectory(char* name)
{
    //디렉토리 생성(1)
    //생성할 디렉토리의 block과 inode를 저장할 공간을 탐색
    int freeBlock = GetFreeBlockNum();
    int freeInode = GetFreeInodeNum();


    //디렉토리 생성(2)
    //path가 '/'로 시작하지 않으면 fail
    if(name[0] != '/')
        return -1;

    
    char nameArr[100];  //name을 동적 문자열로 변환해줌 ---------------- @@@@@@@@@@@@@@@경로 제한 생김
    strcpy(nameArr, name);

    char* path = strtok(nameArr, "/");
    int inodeNum = 0;
    Inode* pInode = malloc(sizeof(Inode));
    DirEntry* dirBlock = malloc(BLOCK_SIZE);
    int find, blknum, parentInode = 0;
    while(path != NULL)
    {
        //먼저 path에 해당하는 dir를 찾음
        find = 0;
        GetInode(inodeNum, pInode);
        for(int i = 0; i < pInode->allocBlocks && find == 0; i++){
            blknum = LogicalBlock(i, dirBlock, pInode);
            for(int j = 0; j < 8; j++){
                if(strcmp(dirBlock[j].name, path) == 0 && dirBlock[j].inodeNum != INVALID_ENTRY){
                    inodeNum = dirBlock[j].inodeNum;
                    parentInode = inodeNum;
                    find = 1;
                    break;
                }
            }
        }
        char* nextPath = strtok(NULL, "/");


        //찾지 못했고 다음 path가 없으면, 즉 Path의 directory를 추가해야 하는 경우
        if(find == 0 && nextPath == NULL){
            //빈 entry를 찾음
            for(int i = 0; i < 8; i++){
                if(dirBlock[i].inodeNum == INVALID_ENTRY)        //빈 entry를 찾으면 그곳에 name, inodeNum을 넣고 
                {
                    strcpy(dirBlock[i].name, path);
                    dirBlock[i].inodeNum = freeInode;
                    DevWriteBlock(blknum, (char*)dirBlock);     //저장
                    break;
                }
                else if(i == 7){       //빈 entry가 없으면 block을 추가하고 거기에 directory 추가
                    int addblocknum = AddBlock(pInode, inodeNum);     //블록 추가
                    DevReadBlock(addblocknum, (char*)dirBlock);
                    freeBlock = GetFreeBlockNum();
                    dirBlock[0].inodeNum = freeInode;
                    strcpy(dirBlock[0].name, path);

                    DevWriteBlock(addblocknum, (char*)dirBlock);    //저장
                }
            }
        }
        //찾지 못 했는데 그게 중간 path인 경우 - 실패
        else if(find == 0 && nextPath != NULL)
            return -1;
        //이미 그 dir가 있는 경우 - 실패
        else if(find == 1 && nextPath == NULL)
            return -1;


        path = nextPath;
    }


    //디렉토리 생성(3)
    DirEntry* newDirBlock = malloc(BLOCK_SIZE);
    newDirBlock[0].inodeNum = freeInode;
    strcpy(newDirBlock[0].name, ".");
    newDirBlock[1].inodeNum = parentInode;        //부모의 inode 번호
    strcpy(newDirBlock[1].name, "..");
    //나머지 모든 entry를 invalid 처리
        for(int i = 2; i < 8; i++)
            newDirBlock[i].inodeNum = INVALID_ENTRY;

    DevWriteBlock(freeBlock, (char*)newDirBlock);


    //디렉토리 생성(4)
    GetInode(freeInode, pInode);
    pInode->dirBlockPtr[0] = freeBlock;
    pInode->allocBlocks = 1;
    pInode->size = 512;
    pInode->type = FILE_TYPE_DIR;
    PutInode(freeInode, pInode);

    SetBlockBytemap(freeBlock);
    SetInodeBytemap(freeInode);


    //디렉토리 생성(5)
    FileSysInfo* superBlock = malloc(BLOCK_SIZE);
    DevReadBlock(0, (char*)superBlock);
    superBlock->numAllocBlocks++;
    superBlock->numFreeBlocks--;
    superBlock->numAllocInodes++;
    DevWriteBlock(0, (char*)superBlock);


    //free해줌
    free(superBlock);
    free(pInode);
    free(newDirBlock);
    free(dirBlock);

    return 0;
}

//
//logical block num i를 받아서 해당 physical block을 넣어주는 함수
//block num을 반환함
int LogicalBlock(int i, DirEntry* dirBlock, Inode* pInode)
{
    if(i < 4){
        DevReadBlock(pInode->dirBlockPtr[i], (char*)dirBlock);
        return pInode->dirBlockPtr[i];
    }
    
    int j = i - 4;
    //indirectblock을 가져온다
    int* indirectBlock = malloc(BLOCK_SIZE);
    DevReadBlock(pInode->indirectBlockPtr, (char*)indirectBlock);

    DevReadBlock(indirectBlock[j], (char*)dirBlock);

    int result = indirectBlock[j];
    free(indirectBlock);

    return result;
}

//
//block을 하나 추가해주는 함수
//추가한 block의 번호를 반환함
int AddBlock(Inode* pInode, int inodeNum)
{
    int freeblock = GetFreeBlockNum();
    if(pInode->allocBlocks < 4){
        pInode->dirBlockPtr[pInode->allocBlocks] = freeblock;
        pInode->allocBlocks++;
        pInode->size += 512;
        

        DirEntry* tmp = malloc(BLOCK_SIZE);
        DevReadBlock(freeblock, (char*)tmp);
        //나머지 모든 entry를 invalid 처리
        for(int i = 0; i < 8; i++)
            tmp[i].inodeNum = INVALID_ENTRY;
        DevWriteBlock(freeblock, (char*)tmp);

        SetBlockBytemap(freeblock);
        FileSysInfo* superBlock = malloc(BLOCK_SIZE);
        DevReadBlock(0, (char*)superBlock);
        superBlock->numFreeBlocks--;
        superBlock->numAllocBlocks++;
        DevWriteBlock(0, (char*)superBlock);
        free(superBlock);

        PutInode(inodeNum, pInode);
        
        return freeblock;
    }
    else if(pInode->allocBlocks >= 4){
        int i = pInode->allocBlocks - 4;
        //indirectBlock을 만들어야하면 만듦
        if(pInode->allocBlocks == 4){
            pInode->indirectBlockPtr = freeblock;   //freeBlock을 indirectBlock에 할당
            SetBlockBytemap(freeblock);
            FileSysInfo* superBlock = malloc(BLOCK_SIZE);
            DevReadBlock(0, (char*)superBlock);
            superBlock->numAllocBlocks++;
            superBlock->numFreeBlocks--;
            DevWriteBlock(0, (char*)superBlock);
            free(superBlock);
        }
        
        freeblock = GetFreeBlockNum();
        int* indirentry = malloc(BLOCK_SIZE);
        DevReadBlock(pInode->indirectBlockPtr, (char*)indirentry);
        indirentry[i] = freeblock;
        DevWriteBlock(pInode->indirectBlockPtr, (char*)indirentry);
        pInode->allocBlocks++;
        pInode->size += 512;

        SetBlockBytemap(freeblock);

        DirEntry* tmp = malloc(BLOCK_SIZE);
        DevReadBlock(freeblock, (char*)tmp);
        //나머지 모든 entry를 invalid 처리
        for(int i = 0; i < 8; i++)
            tmp[i].inodeNum = INVALID_ENTRY;
        DevWriteBlock(freeblock, (char*)tmp);

        PutInode(inodeNum, pInode);

        FileSysInfo* superBlock = malloc(BLOCK_SIZE);
        DevReadBlock(0, (char*)superBlock);
        superBlock->numAllocBlocks++;
        superBlock->numFreeBlocks--;
        DevWriteBlock(0, (char*)superBlock);

        free(superBlock);
        free(indirentry);

        return freeblock;
    }
}

int RemoveDirectory(char* name)
{
    //path가 '/'로 시작하지 않으면 fail
    if(name[0] != '/')
        return -1;

    
    char nameArr[100];  //name을 동적 문자열로 변환해줌 ---------------- @@@@@@@@@@@@@@@경로 제한 생김
    strcpy(nameArr, name);

    char* path = strtok(nameArr, "/");
    int inodeNum = 0;
    Inode* pInode = malloc(sizeof(Inode));
    DirEntry* dirBlock = malloc(BLOCK_SIZE);
    int find, blknum, parentInode = 0;
    while(path != NULL)
    {
        //먼저 path에 해당하는 dir를 찾음
        find = 0;
        GetInode(inodeNum, pInode);
        for(int i = 0; i < pInode->allocBlocks && find == 0; i++){
            blknum = LogicalBlock(i, dirBlock, pInode);
            for(int j = 0; j < 8; j++){
                if(strcmp(dirBlock[j].name, path) == 0 && dirBlock[j].inodeNum != INVALID_ENTRY){
                    parentInode = inodeNum;                            
                    inodeNum = dirBlock[j].inodeNum;
                    find = 1;
                    break;
                }
            }
        }
        char* nextPath = strtok(NULL, "/");


        //찾지 못했고 다음 path가 없으면 - 실패
        if(find == 0 && nextPath == NULL)
            return -1;
        //찾지 못 했는데 그게 중간 path인 경우 - 실패
        else if(find == 0 && nextPath != NULL)
            return -1;
        //그 dir를 찾은 경우, 즉 그 dir를 삭제해야 하는 경우
        else if(find == 1 && nextPath == NULL){
            Inode* targetInode = malloc(sizeof(Inode));
            DirEntry* targetdir = malloc(BLOCK_SIZE);
            
            //그 target의 inode와 block을 targetInode/ targetdir로 가져옴
            int j = 0;
            for(j = 0; j < 8; j++){
                if(strcmp(dirBlock[j].name, path) == 0){
                    GetInode(dirBlock[j].inodeNum, targetInode);
                    break;
                }
            }
            DevReadBlock(targetInode->dirBlockPtr[0], (char*)targetdir);

            //target 디렉토리의 direct[0]이 비어있는지 검사
            int empty = 1;
            if(targetdir[2].inodeNum != INVALID_ENTRY)
                empty = 0;
            
            //만약 target이 비어있지 않다면 - 실패
            if(empty == 0)               
                return -1;

            //
            //target의 block이 비어있는 상태라면 
            //
            //target의 inode와 block을 반환함
            ResetInodeBytemap(dirBlock[j].inodeNum);
            ResetBlockBytemap(targetInode->dirBlockPtr[0]);

            //상위 dir에서 invalid 처리하고 저장                                
            dirBlock[j].inodeNum = INVALID_ENTRY;      
            

            //삭제하고 나서 그 block의 뒤 entry들을 한 칸 씩 앞으로 땡기고 block 저장
            for(int i = j + 1; i < 8; i++){
                dirBlock[i-1].inodeNum = dirBlock[i].inodeNum;
                strcpy(dirBlock[i-1].name, dirBlock[i].name);
            }
            dirBlock[7].inodeNum = INVALID_ENTRY;
            DevWriteBlock(blknum, (char*)dirBlock);

            DirEntry* tmp = malloc(BLOCK_SIZE);
            int logicalBlockNum;
            for(int i = 0; i < pInode->allocBlocks; i++){
                if(blknum == LogicalBlock(i, tmp, pInode)){
                    logicalBlockNum = i;
                    break;
                }
            }
            

            DirEntry* tmp2 = malloc(sizeof(DirEntry));
            while(logicalBlockNum + 1 < pInode->allocBlocks){
                logicalBlockNum++;
                GetDirEntry(LogicalBlock(logicalBlockNum, tmp, pInode), 0, tmp2);
                PutDirEntry(LogicalBlock(logicalBlockNum - 1, tmp, pInode), 7, tmp2);
                tmp2->inodeNum = INVALID_ENTRY;
                PutDirEntry(LogicalBlock(logicalBlockNum, tmp, pInode), 0, tmp2);
                
                int blk = LogicalBlock(logicalBlockNum, tmp, pInode);
                for(int i = 1; i < 8; i++){
                    tmp[i-1].inodeNum = tmp[i].inodeNum;
                    strcpy(tmp[i-1].name, tmp[i].name);
                }
                tmp[7].inodeNum = INVALID_ENTRY;

                DevWriteBlock(blk, (char*)tmp);
            }


            //모두 땡겼고 마지막 block이 비었는지 확인하고 비었으면 반환
            int blk = LogicalBlock(pInode->allocBlocks - 1, tmp, pInode);
            if(tmp[0].inodeNum == INVALID_ENTRY){
                ResetBlockBytemap(blk);
                pInode->allocBlocks--;
                pInode->size -= 512;

                //여기 새로 추가 : indirect가 모두 비었으면 그것도 반환
                if(pInode->allocBlocks == 4){
                    ResetBlockBytemap(pInode->indirectBlockPtr);

                    FileSysInfo* superBlock = malloc(BLOCK_SIZE);
                    DevReadBlock(0, (char*)superBlock);
                    superBlock->numAllocBlocks--;
                    superBlock->numFreeBlocks++;
                    DevWriteBlock(0, (char*)superBlock);
                    free(superBlock);
                }
                

                PutInode(parentInode, pInode);

                //super block 처리
                FileSysInfo* superBlock = malloc(BLOCK_SIZE);
                DevReadBlock(0, (char*)superBlock);
                superBlock->numAllocBlocks--;
                superBlock->numFreeBlocks++;
                DevWriteBlock(0, (char*)superBlock);
                free(superBlock);
            }
            
            
            
            FileSysInfo* superBlock = malloc(BLOCK_SIZE);
            DevReadBlock(0, (char*)superBlock);
            superBlock->numAllocBlocks--;
            superBlock->numFreeBlocks++;
            superBlock->numAllocInodes--;
            DevWriteBlock(0, (char*)superBlock);

            return 0;
        }

        path = nextPath;
    }

}


void CreateFileSystem(void)
{
    //block0~511까지 0으로 초기화
    FileSysInit();

    //free blcok/inode 검색하여 변수에 저장
    int freeBlock = GetFreeBlockNum();
    int freeInode = GetFreeInodeNum();

    //rootDirBlock이라는 block크기의 공간 할당받아서 내용 넣고 freeBlock에 저장
    DirEntry* rootDirBlock = malloc(BLOCK_SIZE);
    strcpy(rootDirBlock[0].name, ".");
    rootDirBlock[0].inodeNum = freeInode;
    //나머지 모든 entry를 invalid 처리
    for(int i = 1; i < 8; i++){
        rootDirBlock[i].inodeNum = INVALID_ENTRY;
    }

    DevWriteBlock(freeBlock, (char*)rootDirBlock);


    //FileSysInfo로 쓸 공간 할당받아서 초기 내용 세팅
    FileSysInfo* superBlock = malloc(BLOCK_SIZE);
    superBlock->blocks = 512;
    superBlock->rootInodeNum = 0;
    superBlock->diskCapacity = 512*512;
    superBlock->numAllocBlocks = 0;
    superBlock->numFreeBlocks = 501;
    superBlock->numAllocInodes = 0;
    superBlock->blockBytemapBlock = 2;
    superBlock->inodeBytemapBlock = 1;
    superBlock->inodeListBlock = 3;
    superBlock->dataRegionBlock = 11;
    //할당한 부분 변경
    superBlock->numAllocBlocks++;
    superBlock->numFreeBlocks--;
    superBlock->numAllocInodes++;
    //blcok 0에 저장
    DevWriteBlock(0, (char*)superBlock);


    //bytemap 수정
    SetBlockBytemap(freeBlock);
    SetInodeBytemap(freeInode);

    //freeinode에 해당하는 inode를 가져와서 data세팅하고 저장
    Inode* rootDirInode = malloc(sizeof(Inode));
    GetInode(freeInode, rootDirInode);
    rootDirInode->dirBlockPtr[0] = 11;
    rootDirInode->allocBlocks = 1;
    rootDirInode->size = 512;
    rootDirInode->type = FILE_TYPE_DIR;
    PutInode(freeInode, rootDirInode);



    free(rootDirBlock);
    free(superBlock);
    free(rootDirInode);
}
void OpenFileSystem(void)
{
    DevOpenDisk();
}

void CloseFileSystem(void)
{
    DevCloseDisk();
}

Directory* OpenDirectory(char* name)
{
    //path가 '/'로 시작하지 않으면 fail
    if(name[0] != '/')
        return NULL;

    
    char nameArr[100];  //name을 동적 문자열로 변환해줌 ---------------- @@@@@@@@@@@@@@@경로 제한 생김
    strcpy(nameArr, name);

    char* path = strtok(nameArr, "/");
    int inodeNum = 0;
    Inode* pInode = malloc(sizeof(Inode));
    DirEntry* dirBlock = malloc(BLOCK_SIZE);
    int find, blknum, parentInode = 0;
    while(path != NULL)
    {
        //먼저 path에 해당하는 dir를 찾음
        find = 0;
        GetInode(inodeNum, pInode);
        for(int i = 0; i < pInode->allocBlocks && find == 0; i++){
            blknum = LogicalBlock(i, dirBlock, pInode);
            for(int j = 0; j < 8; j++){
                if(strcmp(dirBlock[j].name, path) == 0 && dirBlock[j].inodeNum != INVALID_ENTRY){
                    inodeNum = dirBlock[j].inodeNum;
                    parentInode = inodeNum;
                    find = 1;
                    break;
                }
            }
        }
        char* nextPath = strtok(NULL, "/");


        //찾지 못했으면 - 실패
        if(find == 0)
            return NULL;
        //찾은 디렉토리가 open 하려는 디렉토리일때
        else if(find == 1 && nextPath == NULL){
            Directory* return_ptr = malloc(sizeof(Directory));
            return_ptr->inodeNum = inodeNum;
            return return_ptr;
        }

        path = nextPath;
    }

}

FileInfo* ReadDirectory(Directory* pDir)
{  
    
    FileInfo* return_ptr = malloc(sizeof(FileInfo));
    Inode* inode = malloc(sizeof(Inode));
    DirEntry* dirBlock = malloc(BLOCK_SIZE);

    GetInode(pDir->inodeNum, inode);
    if(saveDir != pDir){
        saveIndex = 0;
        saveDir = pDir;
    }

    //@@@수정한 버전
    int index = saveIndex % 8;
    int lgblknum = saveIndex / 8;

    LogicalBlock(lgblknum, dirBlock, inode);


    saveIndex++;
    

    if(dirBlock[index].inodeNum != INVALID_ENTRY){
        GetInode(dirBlock[index].inodeNum, inode);
        strcpy(return_ptr->name, dirBlock[index].name);
        return_ptr->inodeNum = dirBlock[index].inodeNum;
        return_ptr->filetype = inode->type;
        return_ptr->numBlocks = inode->allocBlocks;
        return_ptr->size = inode->size;

        return return_ptr;
    }
    else
        return NULL;
}

int CloseDirectory(Directory* pDir)
{
    free(pDir); 
    return 0;
}
