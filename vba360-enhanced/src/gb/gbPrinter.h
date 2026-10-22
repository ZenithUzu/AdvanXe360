#ifndef GBPRINTER_H
#define GBPRINTER_H

#include "../System.h"

u8 gbPrinterSend(u8 b);
void gbPrinterReset();
extern int gbPrinterDataCount;   // bytes of picture received for the print

#endif // GBPRINTER_H
