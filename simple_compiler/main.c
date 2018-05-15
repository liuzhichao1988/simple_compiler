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
enum TestLexE{ LEA, IMM, JMP, CALL, JZ, JNZ, ENT, ADJ, LEV,LI, LC, SI, SC, PUSH, OR, XOR, AND, EQ, NE, LT, GT, LE, GE, SHL, SHR, ADD, SUB, MUL, DIV, MOD, OPEN, READ, CLOS, PRTF, MALC, MSET, MCMP, EXIT};


// tokens and classes (operators last and in precedence order) */
enum TestLexE2{
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

int basetype;               // the base type of a declaration, make it global for convenience
int expr_type;              // the type of an expression

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

void match(int tk){
    if(token != tk){
        printf("expected %d(%c), got %d(%c)\n", tk, tk, token, token);
        exit(-1);
    }
    next();
}

void expression(int level){
    // do nothing...
}

void enum_declaration(){
    int i = 0;
    while(token != '}'){
        if(token != Id){
            printf("%d: bad enum identifier %d\n", line, token);
            exit(-1);
        }
        next();
        if(token == Assign){
            next();
            if(token != Num){
                printf("%d: bad enum initializer\n", line);
                exit(-1);
            }
            i = token_val;
            next();
        }
        current_id[Class] = Num;
        current_id[Type] = INT;
        current_id[Value] = i++;
        
        if(token == ','){
            next();
        }
    }
}
int index_of_bp;            // index of top pointer on stack

void function_paramter(){
    int type;
    int params = 0;
    while(token != ')'){
        // int name, ...
        type = INT;
        if(token == Int){
            match(Int);
        }else if(token == Char){
            type = CHAR;
            match(Char);
        }
        
        // pointer type
        while(token == '*'){
            match(Mul);
            type = type + PTR;
        }
        
        // parameter name
        if(token != Id){
            printf("%d: bad paramter declaration\n", line);
            exit(-1);
        }
        if(current_id[Class] == Loc){
            printf("%d: duplicate paramter declaration\n", line);
            exit(-1);
        }
        
        match(Id);
        
        // store the local variable
        current_id[BClass] = current_id[Class]; current_id[Class] = Loc;
        current_id[BType] = current_id[Type]; current_id[Type] = type;
        current_id[BValue] = current_id[Value]; current_id[Value] = params++;   // index of current paramter
    }
    
    index_of_bp = params + 1;
}

void statement(){
    int *a, *b;
    if(token == If){
        // if(...) <statement> [else <statement>]
        
        match(If);
        match('(');
        expression(Assign);
        
        *++text = JZ;
        b = ++text;
        statement();            // parse staement
        if(token == Else){      // parse else
            match(Else);
            // emit code for JMP B
            *b = (int)(text + 3);
            *++text = JMP;
            b = ++text;
            
            statement();
        }
        
        *b = (int)(text + 1);
    }
    else if(token == While){
        // while(<cond>) <statement>
        
        match(While);
        
        a = text + 1;
        
        match('(');
        expression(Assign);
        match(')');
        
        *++text = JZ;
        b = ++ text;
        
        statement();
        
        *++text = JMP;
        *++text = (int)a;
        *b = (int)(text+1);
    }
    else if(token == Return){
        // return [expression]
        
        match(Return);
        
        if(token != ';'){
            expression(Assign);
        }
        
        match(';');
        
        // emit code for return
        *++text = LEV;
    }
    else if(token == '{'){
        // { <statement> ... }
        
        match('{');
        
        while(token != '}'){
            statement();
        }
        
        match('}');
    }
    else if(token == ';'){
        // emptry statement
        match(';');
    }
}


void function_body(){
    // type func_name(...) {...}
    //                    | this part
    
    int pos_local;
    int type;
    pos_local = index_of_bp;
    
    while(token == Int || token == Char){
        // local variable declaration, just like global ones
        basetype = (token == Int) ? INT : CHAR;
        match(token);
        
        while(token != ';'){
            type = basetype;
            while(token == Mul){
                match(';');
                type = type + PTR;
            }
            
            if(token != Id){
                // invalid declaration
                printf("%d: bad local declaration\n", line);
                exit(-1);
            }
            
            if(current_id[Class] == Loc){
                // identifier exists
                printf("%d: dumplicate local declaration\n", line);
                exit(-1);
            }
            match(Id);
            
            //store the local variable
            current_id[BClass] = current_id[Class]; current_id[Class] = Loc;
            current_id[BType] = current_id[Type]; current_id[Type] = type;
            current_id[BValue] = current_id[Value]; current_id[Value] = ++pos_local;    //index of current parameter
            
            if(token == ','){
                match(',');
            }
        }
        match(';');
    }
    
    // save the stack size for local variable
    *++text = ENT;
    *++text = pos_local - index_of_bp;
    
    // statements
    while(token != '}'){
        statement();
    }
    
    // emit code for leaving the sub funciton
    *++text = LEV;
}


void funciton_declaration(){
    // variable_decl ::= type {'*'} id {',' {'*'} id} ';'
    // function_decl ::= type {'*'} id '(' parameter_decl ')' '{' body_decl '}'
    // paramter_decl ::= type {'*'} id {',' type {'*'} id}
    // body_decl ::= {variable_decl},{statement}
    // statement ::= non_empty_statement | empty_statement
    // non_empty_statement ::= if_statement | while_statement | '{' statement '}' | 'return' expression | expression ';'
    // if_statement ::= 'if' '(' expression ')' statement ['else' non_empty_statement]
    // while_statement ::= 'while' '(' epxression ')' non_empty_statement
    
    // type func_name (...) {...}
    //               | this part
    
    match('(');
    function_paramter();
    match(')');
    match('{');
    function_body();
//    match('}');
    
    current_id = symbols;
    while(current_id[Token]){
        if(current_id[Class] == Loc){
            current_id[Class] = current_id[BClass];
            current_id[Type] = current_id[BType];
            current_id[Value] = current_id[BValue];
        }
        current_id = current_id + IdSize;
    }
}




void global_declaration(){
    // global_declaration ::= enum_decl | variable_decl | function_decl
    // enum_decl ::= 'enum' [id] '{' id ['=' 'num'] {',' id ['=' 'num'] } '}'
    // variable_decl ::= type {'*'} id {',' {'*'} id } ';'
    // function_decl ::= type {'*'} id '(' paramter_decl ')' '{' body_decl '}'
    
    int type;               // tmp, actual type for variable
    int i;                  // tmp
    basetype = INT;
    
    // parse enum, this should be treated alone
    if(token == Enum){
        match(Enum);
        if(token != '{'){
            match(Id);
        }
        if(token == '{'){
            match('{');
            enum_declaration();
            match('}');
        }
        match(';');
        return;
    }
    
    // parse type informaiton
    if(token == Int){
        match(Int);
    }
    else if(token == Char){
        match(Char);
        basetype = CHAR;
    }
    
    // parse the comma seperated variable declaration
    while(token != ';' && token != '}'){
        type = basetype;
        //parse pointer type, not that there may exist 'int *****x'
        while(token == Mul){
            match(Mul);
            type = type + PTR;
        }
        if(token != Id){
            // invalid declaration
            printf("%d: bad global declaration\n", line);
            exit(-1);
        }
        if(current_id[Class]){
            // identifier exists
            printf("%d: dumplicate global declaration\n", line);
            exit(-1);
        }
        match(Id);
        current_id[Type] = type;
        
        if(token == '('){
            current_id[Class] = Fun;
            current_id[Value] = (int)(text + 1);    // the memory address of function
            funciton_declaration();
        }else{
            // variable declaration
            current_id[Class] = Glo;
            current_id[Value] = (int)data;          // assign memory address
            data = data + sizeof(int);
        }
        
        if(token == ','){
            match(',');
        }
    }
    next();
}

void program(){
    next();
    while(token > 0){
        global_declaration();
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

int TestEval(void);
int TestLex(void);

int main(int argc, const char * argv[]) {
    return TestLex();
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


#define ENUM_TO_NAME(x) case x: return(#x);
char* enumToName(int i){
    switch (i) {
            ENUM_TO_NAME(LEA)
            ENUM_TO_NAME(IMM)
            ENUM_TO_NAME(JMP)
            ENUM_TO_NAME(CALL)
            ENUM_TO_NAME(JZ)
            ENUM_TO_NAME(JNZ)
            ENUM_TO_NAME(ENT)
            ENUM_TO_NAME(ADJ)
            ENUM_TO_NAME(LEV)
            ENUM_TO_NAME(LI)
            ENUM_TO_NAME(LC)
            ENUM_TO_NAME(SI)
            ENUM_TO_NAME(SC)
            ENUM_TO_NAME(PUSH)
            ENUM_TO_NAME(OR)
            ENUM_TO_NAME(XOR)
            ENUM_TO_NAME(AND)
            ENUM_TO_NAME(EQ)
            ENUM_TO_NAME(NE)
            ENUM_TO_NAME(LT)
            ENUM_TO_NAME(GT)
            ENUM_TO_NAME(LE)
            ENUM_TO_NAME(GE)
            ENUM_TO_NAME(SHL)
            ENUM_TO_NAME(SHR)
            ENUM_TO_NAME(ADD)
            ENUM_TO_NAME(SUB)
            ENUM_TO_NAME(MUL)
            ENUM_TO_NAME(DIV)
            ENUM_TO_NAME(MOD)
            ENUM_TO_NAME(OPEN)
            ENUM_TO_NAME(READ)
            ENUM_TO_NAME(CLOS)
            ENUM_TO_NAME(PRTF)
            ENUM_TO_NAME(MALC)
            ENUM_TO_NAME(MSET)
            ENUM_TO_NAME(MCMP)
            ENUM_TO_NAME(EXIT)
            ENUM_TO_NAME(Num)
            ENUM_TO_NAME(Fun)
            ENUM_TO_NAME(Sys)
            ENUM_TO_NAME(Glo)
            ENUM_TO_NAME(Loc)
            ENUM_TO_NAME(Id)
            ENUM_TO_NAME(Char)
            ENUM_TO_NAME(Else)
            ENUM_TO_NAME(Enum)
            ENUM_TO_NAME(If)
            ENUM_TO_NAME(Int)
            ENUM_TO_NAME(Return)
            ENUM_TO_NAME(Sizeof)
            ENUM_TO_NAME(While)
            ENUM_TO_NAME(Cond)
            ENUM_TO_NAME(Assign)
            ENUM_TO_NAME(Lor)
            ENUM_TO_NAME(Lan)
            ENUM_TO_NAME(Or)
            ENUM_TO_NAME(Xor)
            ENUM_TO_NAME(And)
            ENUM_TO_NAME(Eq)
            ENUM_TO_NAME(Ne)
            ENUM_TO_NAME(Lt)
            ENUM_TO_NAME(Gt)
            ENUM_TO_NAME(Le)
            ENUM_TO_NAME(Ge)
            ENUM_TO_NAME(Shl)
            ENUM_TO_NAME(Shr)
            ENUM_TO_NAME(Add)
            ENUM_TO_NAME(Sub)
            ENUM_TO_NAME(Mul)
            ENUM_TO_NAME(Div)
            ENUM_TO_NAME(Mod)
            ENUM_TO_NAME(Inc)
            ENUM_TO_NAME(Dec)
            ENUM_TO_NAME(Brak)
    }
    return "other";
}

int TestLex(){
    
    line = 1;
    int fd;
    int i;
    char *argv = "/Users/neil/Documents/GitHub/Learning/compiler/simple_compiler/simple_compiler/test_lex.strings";
    
    if((fd = open(argv, O_RDONLY)) < 0){
        printf("could not open(%s)\n", argv);
        return -1;
    }
   
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
    
    if(!(symbols = malloc(poolsize))){
        
    }
    if(!(src = malloc(poolsize))){
        printf("could not malloc(%d) for source area\n", poolsize);
        return -1;
    }
    if((i = (int)read(fd, src, 2000)) <= 0){
        printf("read() returned %d\n", i);
        return -1;
    }
    src[i] = 0;
    
    while(*src){
        next();
        
        printf("token:%s, line:%d\n", enumToName(token), line);
    }
    
    printf("End....\n");
    
    return 0;
}













