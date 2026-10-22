#include "HttpQueue.h"

bool HttpQueue::enqueue(const String& method, const String& path, const String& body) {
    if (_size >= CapacityLimits::MaxHttpQueue || method.length() > 16 ||
        path.length() > CapacityLimits::MaxHttpPathLength ||
        body.length() > CapacityLimits::MaxHttpBodyLength) {
        _rejectedCount++;
        return false;
    }
    Item& item = _items[_tail];
    item.method = method;
    item.path = path;
    item.body = body;
    item.retries = 0;
    _tail = (_tail + 1) % CapacityLimits::MaxHttpQueue;
    _size++;
    return true;
}

void HttpQueue::tick() {
    if (!_size) return;
    _items[_head].method = String();
    _items[_head].path = String();
    _items[_head].body = String();
    _head = (_head + 1) % CapacityLimits::MaxHttpQueue;
    _size--;
}

uint8_t HttpQueue::size() const { return _size; }
uint32_t HttpQueue::rejectedCount() const { return _rejectedCount; }
