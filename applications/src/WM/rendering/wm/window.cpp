#include "window.h"
#include "rendering.h"

uint32_t window_internal_t::getPix(uint32_t px, uint32_t py){
    return *((uint32_t*)buffer + py * width + px);
}

void window_internal_t::putPix(uint32_t px, uint32_t py, uint32_t clr){
    *((uint32_t*)buffer + py * width + px) = clr;
}

WIDGET* window_internal_t::CreateWidget(WIDGET_TYPE type, const wchar_t* text, int identifier, int x, int y, uint32_t width, uint32_t height, uint32_t style){
    switch (type){
        case TYPE_BUTTON:{
            BUTTON* btn = new BUTTON;
            btn->type = TYPE_BUTTON;
            btn->x = x;
            btn->y = y;
            btn->width = width;
            btn->height = height;
            btn->style = style;
            btn->parent = (window_t*)this;
            btn->text = text;
            btn->id = identifier;
            widgets->add(btn);
            btn->draw();
            return btn;
        }
        case TYPE_TEXTBOX:{
            TEXTBOX* box = new TEXTBOX;
            box->type = TYPE_TEXTBOX;
            box->cursorPosition = 0;
            box->maxLength = identifier;
            box->x = x;
            box->y = y;
            box->width = width;
            box->height = height;
            box->style = style;
            box->text = nullptr;
            box->parent = (window_t*)this;
            widgets->add(box);
            box->text = new wchar_t[2];
            box->text[0] = L'\0';
            box->draw();
            return box;
        }
    }
    return nullptr;
}

