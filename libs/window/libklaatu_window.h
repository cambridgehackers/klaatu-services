#ifndef __libkivinit_h_
#define __libkivinit_h_
#define VALUE_TO_STRING(x) #x
#define VALUE(x) VALUE_TO_STRING(x)
#define VAR_NAME_VALUE(var) #var "="  VALUE(var)
unsigned int klaatu_get_window(void);

#endif
