//
//  main.c
//  simple_compiler
//
//  Created by 刘志超 on 2018/5/9.
//  Copyright © 2018年 刘志超. All rights reserved.
//

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>


int token;                      // current token
char *src, *old_src;            // pointer to source code string
int poolsize;                   // default size of test/data/stack
int line;                       // line number

void next(){
    token = *src++;
}

void expression(int level){
    // do nothing...
}

void program(){
    next();
    while(token > 0){
        printf("token is: %c\n", token);
        next();
    }
}

int eval(){
    return 0;
}




int main(int argc, const char * argv[]) {
    int i, fd;
    
    argc--;
    argv++;
    
    poolsize = 256*1024;
    line = 1;
    
    if((fd = open(*argv, 0)) < 0){
        printf("could not open(%s)\n", *argv);
        return -1;
    }
    
    if(!(src = old_src = malloc(poolsize))){
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }
    
    if((i = (int)read(fd, src, poolsize -1)) <= 0){
        printf("read() returned %d\n", i);
        return -1;
    }
    
    src[i] = 0;
    close(fd);
    
    program();
    
    int ret = eval();
    
    
    
    return ret;
}

















