# wcc:编译原理课程项目
本项目来源于学校编译原理课程设计，主要借鉴了chibicc的设计思路，该C编译器采用一种增量的方式逐步构建符合C11标准的编译器，非常适合初学者学习。
项目计划分为四个部分实现：词法分析，语法分析，语义分析和中间代码生成，目标代码生成，中间代码优化部分由于时间问题暂时不考虑实现，但万一以后想实现呢？未可知。
原本准备从小项目逐步构建起整个框架，但由于需要生成中间代码和目标代码（课程要求）所以起步并不顺利，因为需要写太多代码用于基本的寄存器分配以及构建符号表。
## 支持的语法
 program := stmt*  
 stmt := expr ";"  
      | "return" expr ";"
expr := assign    
assign:=equality("=" assign)?   //右递归用于构建右结合运算符  
equality:=relational("==" relational | "!=" relational) *  
relational:=add(">=" add | "<=" add | ">" add | "<" add) *  
add:=mul("+" mul | "-" mul)*  
 mul := unary ("*" unary | "/" unary)*  
 unary := ("+" | "-")? unary  
       | primary  
primary:=num | "(" expr ")"  
## 使用方法
使用make指令进行编译，然后运行./test.sh进行测试，或者直接make test
## 参考
- [chibicc](https://github.com/rui314/chibicc): 采用增量方式构建的C编译器