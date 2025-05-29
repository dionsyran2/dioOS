#pragma once
#include <stdint.h>
#include <stddef.h>
#include <libwm.h>

enum WIDGET_TYPE{
    TYPE_BUTTON,
    TYPE_TEXTBOX
};

/**************/
/* BASE CLASS */
/**************/

class WIDGET{
    public:
    WIDGET_TYPE type;

    int x, y;
    uint32_t width, height, style;
    window_t* parent;

    virtual bool contains(int x, int y);
    virtual void draw() = 0;
    virtual void onClick() = 0;
};


/****************/
/* BUTTON CLASS */
/****************/

class BUTTON : public WIDGET{
    public:
    int id;
    const wchar_t* text; 
    void draw();
    void onClick();
};


/*****************/
/* TEXTBOX CLASS */
/*****************/

class TEXTBOX : public WIDGET{
    public:
    wchar_t* text;
    size_t maxLength;
    size_t cursorPosition = 0;
    bool focused;

    void draw();
    void onClick();
    void handleKey(wchar_t character);
};




extern TEXTBOX* _focused_widget;