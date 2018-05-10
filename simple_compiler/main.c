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

int *text,                      // text segment
    *old_text,                  // for dump text segment
    *stack;                     // stack
char *data;                     // data segment

// virtual machine registers */
long *pc,                        // program counter, storing the next instruction to execute
     *bp,                        // base pointer, point to base of a function's memory on stack
     *sp,                        // pointer register, point to top of the stack
     ax;                         // general register

// instructions */
enum { LEA, IMM, JMP, CALL, JZ, JNZ, ENT, ADJ, LEV,LI, LC, SI, SC, PUSH, OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD, OPEN, READ, CLOS, PRTF, MALC, MSET, MCMP, EXIT};


// tokens and classes (operators last and in precedence order) */
enum {
    Num = 128, Fun, Sys, Glo, Loc, Id,
    Char, Else, Enum, If, Int, Return, Sizeof, While,
    Assign, Cond, Lor, Lan, Or, Xor, And, Eq, Ne, Lt, Gt, Le, Ge, Shl, Shr, Add, Sub, Mul, Div, Mod, Inc, Dec, Brak,
};

int token_val;                   // value of current token (mainly for number)
long *current_id;                // current parsed ID
long *symbols;                   // symbol table

//  fileds of identifier  */
enum { Token, Hash, Name, Type, Class, Value, BType, BClass, BValue, IdSize };

// types of variable/function
enum { CHAR, INT, PTR };
long *idmain;                    // the main function

void next(){
    char *last_pos;
    int hash;
    
    while((token = *src)){
        ++src;
        
        //  parse token here  */
        if(token == '\n'){
            ++line;
        }
        else if(token == '#'){
            // skip macro, because we will not support it
            while(*src != 0 && *src != '\n'){
                ++src;
            }
        }
        else if((token >= 'a' && token <= 'z') || (token >= 'A' && token <= 'Z') || (token == '_')){
            //parse identifier
            last_pos = src - 1;
            hash = token;
            
            while((*src >= 'a' && *src <= 'z') || (*src >= 'A' && *src <= 'Z') || (*src == '_')){
                hash = hash * 147 + *src;
                ++src;
            }
            
            // look for existing identifier, liner search
            current_id = symbols;
            while(current_id[Token]){
                if(current_id[Hash] == hash && !memcmp((char*)current_id[Name], last_pos, src - last_pos)){
                    // found one, return
                    token = (int)current_id[Token];
                    return;
                }
                current_id = current_id + IdSize;
            }
            
            //store new ID
            current_id[Name] = (long)last_pos;
            current_id[Hash] = hash;
            token = (int)(current_id[Token] = Id);
            return;
        }
        else if(token >= '0' && token <= '9'){
            // parse number, thress kinds: dec(123) hex(0x123) oct(017)
            token_val = token - '0';
            if(token_val > 0){
                // dec, starts with [1-9]
                while(*src >= '0' && *src <= '9'){
                    token_val = token_val * 10 + (*src++ - '0');
                }
            }
            else{
                // starts with number 0
                if(*src == 'x' || *src == 'X'){
                    // hex
                    token = *++src;
                    while((token >= '0' && token <='9') || (token >= 'a' && token <= 'f') || (token >= 'A' && token <= 'F')){
                        token_val = token_val * 16 + (token & 15) + ((token >= 'A' || token >= 'a') ? 9 : 0);
                        token = *++src;
                    }
                }
                else{
                    // oct
                    while(*src >= '0' && *src <= '7'){
                        token_val = token_val * 8 + (*src - '0');
                    }
                }
            }
            
            token = Num;
            return;
        }
        else if(token == '"' || token == '\''){
            // parse string literal, currently, the only supported escape character is '\n', store the string literal into data
            last_pos = data;
            while(*src != 0 && *src != token){
                token_val = *src++;
                if(token_val == '\\'){
                    // escape character
                    token_val = *src++;
                    if(token_val == 'n'){
                        token_val = '\n';
                    }
                }
                
                if(token == '"'){
                    *data++ = token_val;
                }
            }
            
            src++;
            // meeting single character, return Num token
            if(token == '"'){
                token_val = (int)last_pos;
            }else{
                token = Num;
            }
            
            return;
        }
        else if(token == '/'){
            if(*src == '/'){
                // skip comments
                while(*src != 0 && *src != '\n'){
                    ++src;
                }
            }
            else{
                // divide operator
                token = Div;
                return;
            }
        }
        else if(token == '='){
            // parse '==' and '='
            if(*src == '='){
                src++;
                token = Eq;
            }else{
                token = Assign;
            }
            return;
        }
        else if(token == '+'){
            // parse '+' and '++'
            if(*src == '+'){
                src++;
                token = Inc;
            }
            else{
                token = Add;
            }
            return;
        }
        else if(token == '-'){
            // parse '-' and '--'
            if(*src == '-'){
                src++;
                token = Dec;
            }else{
                token = Sub;
            }
        }
        else if(token == '!'){
            // parse and '!='
            if(*src == '='){
                src++;
                token = Ne;
            }
            return;
        }
        else if(token == '<'){
            // parse '<=', '<<' and '<'
            if(*src == '='){
                src++;
                token = Le;
            }
            else if(*src == '<'){
                src++;
                token = Shl;
            }
            else {
                token = Lt;
            }
            return;
        }
        else if(token == '>'){
            // parse '>=', '>>' and '>'
            if(*src == '='){
                src++;
                token = Ge;
            }
            else if(*src == '>'){
                src++;
                token = Shr;
            }else{
                token = Gt;
            }
            return;
        }
        else if(token == '|'){
            // parse '|' and '||'
            if(*src == '|'){
                src++;
                token = Lor;
            }
            else{
                token = Or;
            }
            return;
        }
        else if(token == '&'){
            // parse '&' and '&&'
            if(*src == '&'){
                src++;
                token = Lan;
            }else {
                token = And;
            }
            return;
        }
        else if(token == '^'){
            token = Xor;
            return;
        }
        else if(token == '%'){
            token = Mod;
            return;
        }
        else if(token == '*'){
            token = Mul;
            return;
        }
        else if(token == '['){
            token = Brak;
            return;
        }
        else if(token == '?'){
            token = Cond;
            return;
        }
        else if(token == '~' || token == ';' || token == '{' || token == '}' || token == '(' || token == ')' || token == ']' || token == ',' || token == ':'){
            // directly return the character as token
            return;
        }
    }
    
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


// virtual machine */
int eval(){
    long op = 0;
    long *tmp;
    while(1){
        op = *pc++;
        
        //  base instructions  */
        if(op == IMM)       { ax = *pc++; }                         // load immediater value to ax
        else if(op == LC)   { ax = *(char *)ax; }                   // load character to ax, address in ax
        else if(op == LI)   { ax = *(int *)ax; }                    // load integer to ax, address in ax
        else if(op == SC)   { ax = *(char*)*sp++ = ax; }            // save character to address, value in ax, address on stack  -- sp save the pointer, the pointed value change to ax's value, then stack pop...
        else if(op == SI)   { *(long*)*sp++ = ax; }                 // save integer to address, value in ax, address on stack
        else if(op == PUSH) { *--sp = ax; }                         // push the value of ax onto stack
        else if(op == JMP)  { pc = (long*)*pc; }                    // jump to the address  -- pc: instruction parameter ins par .... ...
        else if(op == JZ)   { pc = ax ? pc + 1 : (long*)*pc; }      // jump if ax is 0
        else if(op == JNZ)  { pc = ax ? (long*)*pc : pc + 1; }      // jump if ax is not 0
        
        
        //  call function instructions  */
        else if(op == CALL) { *--sp = (long)(pc+1); pc = (long*)pc; }
        
        // op                  pc
        // |                   |
        // CALL_INSTRUCTION    FUNCTION_LOCATION   NEXT_INSTRUCTION
        
        //else if(op == RET)  { pc = (long*)*sp++; }                  // return from sub function to next instruction of the super, but we use LEV instead of it
        else if(op == ENT)  { *--sp = (int)bp; bp = sp; sp = sp - *pc++; }  // make new stack frame and save stack for local variable
        else if(op == ADJ)  { sp = sp + *pc++; }                    // remove arguments from frame
        else if(op == LEV)  { sp = bp; bp = (long*)*sp++; pc = (long*)*sp++; }  //restore call frame and PC
        else if(op == LEA)  { ax = (long)(bp + *pc++); }            // load address for arguments
        
        
        /*  operator instructions  */
        else if(op == OR)   ax = *sp++ | ax;                        // bitwise OR
        else if(op == XOR)  ax = *sp++ ^ ax;                        // bitwise exclusive-OR
        else if(op == AND)  ax = *sp++ & ax;                        // bitwise AND
        else if(op == EQ)   ax = *sp++ == ax;                       // equal
        else if(op == NE)   ax = *sp++ != ax;                       // not equal
        else if(op == LT)   ax = *sp++ < ax;                        // little than
        else if(op == LE)   ax = *sp++ <= ax;                       // little or equal
        else if(op == GT)   ax = *sp++ > ax;                        // greater than
        else if(op == GE)   ax = *sp++ >= ax;                       // greater or equal
        else if(op == SHL)  ax = *sp++ < ax;                        // shift left
        else if(op == SHR)  ax = *sp++ > ax;                        // shift right
        else if(op == ADD)  ax = *sp++ + ax;                        // ADD
        else if(op == SUB)  ax = *sp++ - ax;                        // subtract/minus
        else if(op == MUL)  ax = *sp++ * ax;                        // multiply
        else if(op == DIV)  ax = *sp++ / ax;                        // divide
        else if(op == MOD)  ax = *sp++ % ax;                        // MOD
        
        
        //  built-in function  */
        else if(op == EXIT) { printf("exit(%ld)\n", *sp); return (int)*sp; }
        else if(op == OPEN) { ax = open((char*)sp[1], (int)sp[0]); }
        else if(op == CLOS) { ax = close((int)*sp); }
        else if(op == PRTF) { tmp = sp + (int)pc[1]; ax = printf((char*)tmp[-1], tmp[-2], tmp[-3], tmp[-4], tmp[-6], tmp[-6]); }
        else if(op == MALC) { ax = (long)malloc(*sp); }
        else if(op == MSET) { ax = (long)memset((char*)sp[2], sp[1], *sp); }
        else if(op == MCMP) { ax = memcmp((char*)sp[2], (char*)sp[1], *sp); }
        
        
        else{
            printf("unknown instruction:%ld\n", op);
            return -1;
        }
        
    }
    
    return 0;
}


int compile(int argc, const char **argv){
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
    
    // allocate memory for virtual machine
    if(!(text = old_text = malloc(poolsize))){
        printf("could not malloc(%d) for text area\n", poolsize);
        return -1;
    }
    
    if(!(data = malloc(poolsize))){
        printf("could not malloc (%d) for data area\n", poolsize);
        return -1;
    }
    
    if(!(stack = malloc(poolsize))){
        printf("could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }
    
    memset(text, 0, poolsize);
    memset(data, 0, poolsize);
    memset(stack, 0, poolsize);
    
    bp = sp = (long*)((long)(stack + poolsize));
    ax = 0;
    
    src = "char else enum if int return sizeof while "
    "open read close printf malloc memset memcmp exit viod main";
    
    // add keywords to symbol table
    i = Char;
    while(i <= While){
        next();
        current_id[Token] = i++;
    }
    
    // add library to symbol table
    i = OPEN;
    while(i <= EXIT){
        next();
        current_id[Class] = Sys;
        current_id[Type] = INT;
        current_id[Value] = i++;
    }
    
    next(); current_id[Token] = Sys;    // handle void type
    next(); idmain = current_id;        // keep track of main
    
    program();
    
    int ret = eval();
    
    
    
    return ret;
}


int main(int argc, const char * argv[]) {
    
    
}

// *********** Test Code
int TestEval(){
    ax = 0;
    
    if(!(stack = malloc(poolsize))){
        printf("could not malloc(%d) for stack area\n", poolsize);
        return -1;
    }
    
    memset(stack, 0, poolsize);
    
    bp = sp = (long*)((long)(stack + poolsize));
    
    
    long code[] = {IMM, 10, PUSH, IMM, 20, ADD, PUSH, EXIT};
    pc = code;
    
    eval();
    
    return 0;
}

int TestLex(){
    return 0;
}













