#pragma once

class io_buf{

public:
	io_buf(int size);

	void clear();

	void adjust();

	void copy(const io_buf * other);

	void pop(int len);
// 为什么不定义为私有呢？
// 因为后续要直接通过对象来访问这些属性值，如果定义为私有，只有当前类中的方法可以访问，私有的访问级别仅限于当前头文件作用域所包含的成分可访问，
// 故定义为public，理清public，private，protected三种修饰的访问控制
	char *data;
	int head;	
	int length;
	int capacity;
	io_buf *next;

};
