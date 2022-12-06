#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "mdadm.h"
#include "jbod.h"

//Global variables used to interact with jbod_operation
int operation;
int currentlyMounted = 0;

//Function to create an op code
int createOp(int command, int diskid, int reserved, int blockid){

  uint32_t cShift;
	uint32_t dShift;
	uint32_t rShift;
	uint32_t bShift;
	// shift bits for every specific part of the command block
	cShift =  (uint32_t)command<<26;
	dShift = (uint32_t) diskid<<22 | cShift;
	
	rShift = (uint32_t) reserved<<8 | dShift;
	bShift = (uint32_t) blockid | rShift;
  return bShift;
  
}

int mdadm_mount(void) {
  //checks to see if jbod is already mounted 
  if(currentlyMounted == 0){    
    operation = jbod_operation(JBOD_MOUNT, NULL);
    currentlyMounted = 1;
    return 1;
  }
  return -1;
}

int mdadm_unmount(void) {
  uint32_t op;
  //checks to see if jbod is mounted already
  if(currentlyMounted == 1){
    op = createOp(JBOD_UNMOUNT,0,0,0);
    operation = jbod_operation(op, NULL);
    currentlyMounted = 0;
    return 1;
  }
  return -1;
}

int mdadm_read(uint32_t addr, uint32_t len, uint8_t *buf) {
  //test cases that should fail
  if(currentlyMounted == 0){
    return -1;
  }
  if(addr > 0xfffff){
    return -1;

  }
  if(addr + len > 0xfffff){
    return -1;
  }
  if(len > 1024){
    return -1;
  }
  if(buf == NULL){
    if(len != 0){
      return -1;
    } else{
      return 0;
    }
  }
  //local data to mimick the disk and blocks
  int disk = addr/(256*256);
  int currAddr;
  
  //locates the address and data to the necessary disk
  if(disk > 0){
    currAddr = addr%(256*256);
  } else {
    currAddr = addr;
  }

  //variables used to keep track of what data has been stored and updates the location of the address
  int remainder = len;
  int bytesCounted = 0;
  int block = addr/256;
  int remblock = currAddr%256;
  //buffer to store the data
  uint8_t b[256]; 

  //jbod operation to seek to the disk
  uint32_t op = createOp(JBOD_SEEK_TO_DISK, disk, 0, 0);
  operation = jbod_operation(op, NULL);

  //if statement to decide if we must traverse multiple blocks or disks
  if(remblock + remainder < 256){
    op = createOp(JBOD_READ_BLOCK, 0, 0, block);
    operation = jbod_operation(op, b);
    int i;
    for(i=0; i < remainder; i++ ){
      buf[bytesCounted] = b[currAddr%256];
      currAddr++;
      bytesCounted++;
    }  
  } else {

    //while to determine the amount of space left in the block or disk, and stores what it can into the buffer
    while (remblock + remainder >= 256){
      //jbod operation to copy the data to the temporary buffer
      op = createOp(JBOD_SEEK_TO_BLOCK, 0, 0, block);
      operation = jbod_operation(op, NULL);
      op = createOp(JBOD_READ_BLOCK, 0, 0, block);
      operation = jbod_operation(op, b);
      int j;
      int we = 256-remblock;
      //storing the temp buf to given buf
      for(j=0; j < we; j++ ){
        buf[bytesCounted] = b[currAddr%256];
        currAddr++;
        bytesCounted++;
        remainder--;
      }

      //updated address and remaining bytes to read
      remblock = currAddr%256;
      block = currAddr/256;
      int tempd = disk;
      int dk = addr + len - remainder;
      disk = dk/(256*256);
      //checks to see if we must switch disks
      if(tempd<disk){
        op = createOp(JBOD_SEEK_TO_DISK, disk, 0, block);
        operation = jbod_operation(op, NULL);
      }
    }
    //final read done with the remaining space on our block
    block = currAddr/256;
    op = createOp(JBOD_SEEK_TO_BLOCK, 0, 0, block);
    operation = jbod_operation(op, NULL);
    op = createOp(JBOD_READ_BLOCK, 0, 0, block);
    operation = jbod_operation(op, b);
    int r;
    for(r=0; r < remainder; r++ ){
      buf[bytesCounted] = b[currAddr%256];
      currAddr++;
      bytesCounted++;
    }  
  }
  //returns the length of read bytes
  return len;
}

int mdadm_write(uint32_t addr, uint32_t len, const uint8_t *buf) {
  if(currentlyMounted == 0){
    return -1;
  }
  if(addr > 0xfffff){
    return -1;

  }
  if(addr + len > 0xfffff+1){
    //printf("DAMN");
    return -1;
  }
  if(len > 1024){
    return -1;
  }
  if(buf == NULL){
    if(len != 0){
      return -1;
    } else{
      return 0;
    }
  }
  //local data to mimick the disk and blocks
  int disk = addr/(256*256);
  int currAddr;
  
  //locates the address and data to the necessary disk
  if(disk > 0){
    currAddr = addr%(256*256);
  } else {
    currAddr = addr;
  }

  //variables used to keep track of what data has been stored and updates the location of the address
  int remainder = len;
  int bytesCounted = 0;
  int block = currAddr/256;
  int remblock = currAddr%256;
  //buffer to store the data
  uint8_t b[256]; 

  //jbod operation to seek to the disk
  uint32_t op = createOp(JBOD_SEEK_TO_DISK, disk, 0, 0);
  operation = jbod_operation(op, NULL);

  //if statement to decide if we must traverse multiple blocks or disks
  if(remblock + remainder < 256){
    op = createOp(JBOD_SEEK_TO_BLOCK, 0, 0, block);
    operation = jbod_operation(op, NULL);
    op = createOp(JBOD_READ_BLOCK, 0, 0, block);
    operation = jbod_operation(op, b);
    int i;
    for(i=0; i < remainder; i++ ){
      b[currAddr%256] = buf[bytesCounted];
        currAddr++;
        bytesCounted++;
    }  

    //write operation performed
    op = createOp(JBOD_SEEK_TO_BLOCK, 0, 0, block);
    operation = jbod_operation(op, NULL);
    op = createOp(JBOD_WRITE_BLOCK, 0, 0, block);
    operation = jbod_operation(op, b);
  } else {

    //while to determine the amount of space left in the block or disk, and stores what it can into the buffer
    while (remblock + remainder >= 256){
      //jbod operation to copy the data to the temporary buffer
      op = createOp(JBOD_SEEK_TO_BLOCK, 0, 0, block);
      operation = jbod_operation(op, NULL);
      op = createOp(JBOD_READ_BLOCK, 0, 0, block);
      operation = jbod_operation(op, b);
      int j;
      int we = 256-remblock;
      //storing the temp buf to given buf
      for(j=0; j < we; j++ ){
        b[currAddr%256] = buf[bytesCounted];
        currAddr++;
        bytesCounted++;
        remainder--;
      }
      op = createOp(JBOD_SEEK_TO_BLOCK, 0, 0, block);
      operation = jbod_operation(op, NULL);
      op = createOp(JBOD_WRITE_BLOCK, 0, 0, block);
      operation = jbod_operation(op, b);
      //updated address and remaining bytes to write
      remblock = currAddr%256;
      block = currAddr/256;
      int tempd = disk;
      int dk = addr + len - remainder;
      disk = dk/(256*256);
      //checks to see if we must switch disks
      if(tempd<disk){
        op = createOp(JBOD_SEEK_TO_DISK, disk, 0, 0);
        operation = jbod_operation(op, NULL);
      }
    }
    //final write done with the remaining space on our block
    op = createOp(JBOD_SEEK_TO_BLOCK, 0, 0, block);
    operation = jbod_operation(op, NULL);
    op = createOp(JBOD_READ_BLOCK, 0, 0, block);
    operation = jbod_operation(op, b);
    int r;
    for(r=0; r < remainder; r++ ){
      b[currAddr%256] = buf[bytesCounted];
      currAddr++;
      bytesCounted++;
    } 
    op = createOp(JBOD_SEEK_TO_BLOCK, 0, 0, block);
    operation = jbod_operation(op, NULL);
    op = createOp(JBOD_WRITE_BLOCK, 0, 0, block);
    operation = jbod_operation(op, b); 
  }
  //returns the length of written bytes
  return len;
}