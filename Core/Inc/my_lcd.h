#ifndef LAB9_LCD_MY_LCD_H
#define LAB9_LCD_MY_LCD_H
#include "main.h"

#define MAX_MSG_NUMBER 20

typedef char MsgType;

typedef struct{
	uint8_t ** Rows;
	unsigned int RowCnt;
	MsgType Type;
}Message;

typedef struct {
	Message MsgArr[MAX_MSG_NUMBER];
	unsigned int MsgSize;
}Window;

extern Window wnd;

void InitWindow();
void AddLast(const uint8_t *, MsgType);
void Handle_LED();


#endif //LAB9_LCD_MY_LCD_H
