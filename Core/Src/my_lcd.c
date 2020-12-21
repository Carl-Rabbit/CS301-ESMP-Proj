#include "my_lcd.h"

const uint16_t MARGIN_X = 10;
const uint16_t MARGIN_Y = 10;
const uint16_t PADDING_X = 10;
const uint16_t PADDING_Y = 10;

const uint16_t MSG_GAP = 6;

const uint16_t ROW_GAP = 4;
const uint16_t ROW_MARGIN = 4;
const uint16_t ROW_WIDTH = 160;
const uint16_t ROW_HEIGHT = 16;

const uint16_t MAX_CHAR_LEN = 20;

const MsgType SEND_TYPE = 1;
const MsgType RESP_TYPE = 0;

const uint8_t FONT_SIZE = 16;

Window wnd;

unsigned int GetRowNum(const uint8_t *msg) {
	unsigned int StrLen = strlen((char *) msg) - 2; // remove /r/r/n
	unsigned int RowNum = StrLen / MAX_CHAR_LEN;
	if (RowNum * MAX_CHAR_LEN < StrLen) {
		++RowNum;       // if has rest chars
	}
	return RowNum;
}

void FreeMsg(int idx) {
	Message msg = wnd.MsgArr[idx];
	for (int i = 0; i < msg.RowCnt; ++i) {
		free(msg.Rows[i]);
	}
	free(msg.Rows);
}

void FillRows(Message * MsgPtr, const uint8_t *Str) {
	const uint8_t * StrPtr = Str;
	for (int i = 0; i < MsgPtr->RowCnt; ++i) {
		MsgPtr->Rows[i] = malloc((MAX_CHAR_LEN + 1) * sizeof(uint8_t));
		strncpy((char *)MsgPtr->Rows[i], (char *) StrPtr, MAX_CHAR_LEN);
		MsgPtr->Rows[i][MAX_CHAR_LEN] = '\0';
		StrPtr += MAX_CHAR_LEN;
	}
}

void InitWindow() {
	wnd.MsgSize = 0;
}

void AddLast(const uint8_t *str, MsgType type) {
	if (wnd.MsgSize >= MAX_MSG_NUMBER) {
		// free first one
		FreeMsg(0);
		for (int i = 0; i < wnd.MsgSize - 1; ++i) {
			wnd.MsgArr[i] = wnd.MsgArr[i + 1];
		}
		--wnd.MsgSize;
	}
	unsigned int RowNum = GetRowNum(str);
	Message * NewMsgPtr = &wnd.MsgArr[wnd.MsgSize++];
	NewMsgPtr->Rows = malloc(RowNum * sizeof(uint8_t *));
	NewMsgPtr->RowCnt = RowNum;

	FillRows(NewMsgPtr, str);

	NewMsgPtr->Type = type;
}

void RefreshWindow() {
	uint16_t RespTypeX = MARGIN_X + PADDING_X;
	uint16_t SendTypeX = lcddev.width - RespTypeX - ROW_WIDTH - ROW_MARGIN * 2;
	uint16_t x;
	uint16_t y = lcddev.height - MARGIN_Y - PADDING_Y + MSG_GAP;
	uint16_t MsgWidth = ROW_WIDTH + ROW_MARGIN * 2;
	uint16_t MsgHeight;
	uint16_t MsgBGColor;

	LCD_Clear(WHITE);

	POINT_COLOR = BLACK;
	LCD_DrawRectangle(MARGIN_X, MARGIN_Y, lcddev.width - MARGIN_X, lcddev.height - MARGIN_Y);

	for (int i = wnd.MsgSize - 1; i >= 0; --i) {
		Message msg = wnd.MsgArr[i];
		MsgHeight = ROW_GAP * (1 + msg.RowCnt) + ROW_HEIGHT * msg.RowCnt;
		y -= MSG_GAP + MsgHeight;
		if (y < MARGIN_Y + PADDING_Y) {
			break;
		}
		if (msg.Type == SEND_TYPE) {
			x = SendTypeX;
			MsgBGColor = YELLOW;
		} else {
			x = RespTypeX;
			MsgBGColor = BLUE;
		}
		BACK_COLOR = MsgBGColor;
		LCD_Fill(x, y, x + MsgWidth, y + MsgHeight, MsgBGColor);

		uint16_t TmpX = x + ROW_MARGIN;
		uint16_t TmpY = y + ROW_GAP;
		for (int RowNo = 0; RowNo < msg.RowCnt - 1; ++RowNo) {
			LCD_ShowString(TmpX, TmpY, ROW_WIDTH, ROW_HEIGHT, FONT_SIZE, msg.Rows[RowNo]);
			TmpY += ROW_HEIGHT + ROW_GAP;
		}
		if (msg.Type == SEND_TYPE) {
			TmpX += ROW_WIDTH;
			int cnt = 0;
			uint8_t * str = msg.Rows[msg.RowCnt - 1];
			while (str[cnt] != '\r' && str[cnt] != '\0') {
				++cnt;
			}
			TmpX -= cnt * (ROW_WIDTH / MAX_CHAR_LEN);
		}
		LCD_ShowString(TmpX, TmpY, ROW_WIDTH, ROW_HEIGHT, FONT_SIZE, msg.Rows[msg.RowCnt - 1]);
	}
}