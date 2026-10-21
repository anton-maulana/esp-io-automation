#include "HttpQueue.h"
bool HttpQueue::enqueue(const String& method,const String& path,const String& body){if(_size>=8)return false;_items[_tail]={method,path,body,0};_tail=(_tail+1)%8;_size++;return true;}
void HttpQueue::tick(){if(!_size)return;_head=(_head+1)%8;_size--;}
uint8_t HttpQueue::size()const{return _size;}
