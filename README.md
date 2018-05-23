//
//  textcode.c
//  simple_compiler
//
//  Created by 刘志超 on 2018/5/21.
//  Copyright © 2018年 刘志超. All rights reserved.
//

#include <stdio.h>

int main(int argc, char** argv){
    
    int a = 100;
    int b = a * 20;
    
    int* c = &a;
    printf("%p,  %d", c, b);
    
    
    
    return 0;
}