/*
**
** File: ym2413.c - software implementation of YM2413
**                  FM sound generator type OPLL
**
** Copyright (C) 2002 Jarek Burczynski
**
** Version 1.0
**
*/

#pragma once

extern void YM2413Init(void);
extern void YM2413ResetChip(void);
extern void YM2413Update(int *buffer, int length);
extern void YM2413Write(unsigned int a, unsigned int v);
extern unsigned int YM2413Read(void);
extern unsigned char *YM2413GetContextPtr(void);
extern unsigned int YM2413GetContextSize(void);

