#include "widget.h"
#include "rendering.h"
#include "mouse.h"
#include "wm.h"
#include "wclass.h"

TEXTBOX* _focused_widget = nullptr;

/**************/
/* BASE CLASS */
/**************/

bool WIDGET::contains(int x, int y){
    bool top = y > this->y;
    bool bottom = y < (this->y + this->height);
    bool left = x > this->x;
    bool right = x < (this->x + this->width);

    return top && bottom && left && right;
}


/****************/
/* BUTTON CLASS */
/****************/


void BUTTON::draw(){
    bool swap = false;
    if (this->contains(mouseX - this->parent->x, mouseY - this->parent->y)) swap = true;

    for (int y = this->y; y < this->y + this->height; y++){
        for (int x = this->x; x < this->x + this->width; x++){
            if (x < 0 || y < 0 || x > this->parent->width || y > this->parent->height) continue;
            if (!insideCornerFull(x - this->x, y - this->y, this->width, this->height, 6)) continue;
            this->parent->setPix(x, y, swap ? 0xFFFFFF : 0xCECECE);
        }
    }

    for (int y = this->y + 1; y < this->y + this->height - 1; y++){
        for (int x = this->x + 1; x < this->x + this->width - 1; x++){
            if (x < 0 || y < 0 || x > this->parent->width || y > this->parent->height) continue;
            if (!insideCornerFull(x - this->x - 1, y - this->y - 1, this->width - 2, this->height - 2, 6)) continue;
            this->parent->setPix(x, y, swap ? 0xCECECE : 0xFFFFFF);
        }
    }

    size_t txt_width = this->width;
    int font_size = this->height - 4;

    while(true){
        if (font_size < 0) return;
        txt_width = renderer->getTextLength((wchar_t*)this->text, font_size);

        if (txt_width > this->width - 4){
            font_size -= 2;
        }else{
            break;
        }
    }

    int txt_x = (this->width - txt_width) / 2;
    int txt_y = (this->height - font_size) / 2;
    txt_x += this->x;
    txt_y += this->y;

    renderer->print(0x303030, this->text, txt_x, txt_y, this->x + this->width, this->parent->width, this->parent->buffer, font_size);

    this->parent->dirty_rects[this->parent->amount_of_dirty_rects].x = this->x;
    this->parent->dirty_rects[this->parent->amount_of_dirty_rects].y = this->y;
    this->parent->dirty_rects[this->parent->amount_of_dirty_rects].width = this->width;
    this->parent->dirty_rects[this->parent->amount_of_dirty_rects].height = this->height;
    this->parent->amount_of_dirty_rects++;
    this->parent->_Refresh = true;
}

void BUTTON::onClick(){
    MSG* msg = new MSG;
    msg->win = this->parent;
    msg->code = WM_COMMAND;
    msg->wc = this->parent->wc;
    msg->param1 = this->id;
    SendMessageINT(msg, this->parent->wc);
}

/*****************/
/* TEXTBOX CLASS */
/*****************/


void TEXTBOX::draw(){
    for (int y = this->y; y < this->y + this->height; y++){
        for (int x = this->x; x < this->x + this->width; x++){
            if (x < 0 || y < 0 || x > this->parent->width || y > this->parent->height) continue;
            if (!insideCornerFull(x - this->x, y - this->y, this->width, this->height, 6)) continue;
            this->parent->setPix(x, y, 0xCECECE);
        }
    }

    for (int y = this->y + 1; y < this->y + this->height - 1; y++){
        for (int x = this->x + 1; x < this->x + this->width - 1; x++){
            if (x < 0 || y < 0 || x > this->parent->width || y > this->parent->height) continue;
            if (!insideCornerFull(x - this->x - 1, y - this->y - 1, this->width - 2, this->height - 2, 6)) continue;
            this->parent->setPix(x, y, 0xFFFFFF);
        }
    }

    size_t txt_width = this->width;
    int font_size = 20;/*this->height - 4*/

    while(true){
        if (font_size < 0) return;
        txt_width = renderer->getTextLength((wchar_t*)this->text, font_size);

        if (txt_width > this->width - 4){
            font_size -= 2;
        }else{
            break;
        }
    }

    int txt_x = 2;
    int txt_y = (this->height - font_size) / 2;
    txt_x += this->x;
    txt_y += this->y;

    renderer->print(0x303030, this->text, txt_x, txt_y, this->x + this->width, this->parent->width, this->parent->buffer, font_size);



    size_t caret_offset = renderer->getTextLength(this->text, font_size);

    if (_focused_widget == this){
        int len = strlen(this->text);
        int caret_x = txt_x + caret_offset/*((caret_offset / (len > 0 ? len : 1)) * cursorPosition)*/;
        int caret_y = txt_y;
        int caret_h = font_size;

        for (int i = 0; i < caret_h; ++i) {
            int cx = caret_x;
            int cy = caret_y + i;

            if (cx >= this->x + 2 && cx < this->x + this->width - 2 &&
                cy >= this->y + 2 && cy < this->y + this->height - 2) {
                this->parent->setPix(cx, cy, 0x000000); // black caret pixel
            }
        }
    }
    

    this->parent->dirty_rects[this->parent->amount_of_dirty_rects].x = this->x;
    this->parent->dirty_rects[this->parent->amount_of_dirty_rects].y = this->y;
    this->parent->dirty_rects[this->parent->amount_of_dirty_rects].width = this->width;
    this->parent->dirty_rects[this->parent->amount_of_dirty_rects].height = this->height;
    this->parent->amount_of_dirty_rects++;
    this->parent->_Refresh = true;
}

void TEXTBOX::handleKey(wchar_t character){
    if (_focused_widget != this) return;
    if (_focused_widget == nullptr) return;
    if (this->type != WIDGET_TYPE::TYPE_TEXTBOX) return;

    if (this->text == nullptr){
        this->text = new wchar_t[1];
        this->text[0] = L'\0';
    }
    size_t len = this->text ? strlen(this->text) : 0;

    // Handle backspace
    if (character == L'\b') {
        if (this->cursorPosition == 0 || len == 0) return;

        wchar_t* tmp = new wchar_t[len]; // one char less
        memcpy(tmp, this->text, (this->cursorPosition - 1) * sizeof(wchar_t));
        memcpy(tmp + this->cursorPosition - 1, this->text + this->cursorPosition, (len - this->cursorPosition + 1) * sizeof(wchar_t));

        delete[] this->text;
        this->text = tmp;
        this->cursorPosition--;
        this->draw();
        return;
    }else if (character == L'\a'){
        //move the cursor to the left
        if (this->cursorPosition > 0){
            this->cursorPosition--;
        }
        return;
    }else if (character == L'\e'){
        //move the cursor to the right
        if (this->cursorPosition < strlen(this->text)){
            this->cursorPosition++;
        }
        return;
    }


    if (character < 32 || character > 126) return;

    wchar_t* tmp = new wchar_t[len + 2];

    memcpy(tmp, this->text, this->cursorPosition * sizeof(wchar_t));

    tmp[this->cursorPosition] = character;

    memcpy(tmp + this->cursorPosition + 1, this->text + this->cursorPosition, (len - this->cursorPosition + 1) * sizeof(wchar_t));

    delete[] this->text;
    this->text = tmp;
    this->cursorPosition++;
    this->draw();
}


void TEXTBOX::onClick(){
    _focused_widget = this;
}