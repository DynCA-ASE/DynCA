#pragma once
#include <algorithm>
#include <cassert>
#include <cstring>
#include <stack>
#include <utility>

template <class T, size_t block_size = 64 * 1024 * 1024>
class large_unsynchronized_pool_allocator {
    void *current_allocated_ptr = nullptr;
    size_t current_alloacted_unused = 0;
    std::stack<void *> available_stack;
    std::stack<void *> allocated_ever;

 public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using is_always_equal = std::false_type;

    large_unsynchronized_pool_allocator() = default;
    ~large_unsynchronized_pool_allocator() { release_all(); }
    large_unsynchronized_pool_allocator(const large_unsynchronized_pool_allocator &) = delete;
    auto &operator=(const large_unsynchronized_pool_allocator &) = delete;

    void release_all() {
        while (!allocated_ever.empty()) {
            void *top = allocated_ever.top();
            free(top);
            allocated_ever.pop();
        }
        std::stack<void *>{}.swap(available_stack);
        current_allocated_ptr = nullptr;
        current_alloacted_unused = 0;
    }

    void *allocate() {
        if (!available_stack.empty()) {
            void *ptr = available_stack.top();
            available_stack.pop();
            return ptr;
        }
        if (!current_alloacted_unused) {
            current_allocated_ptr = malloc(sizeof(T) * block_size);
            allocated_ever.push(current_allocated_ptr);
            current_alloacted_unused = block_size;
        }
        void *ptr = current_allocated_ptr;
        current_allocated_ptr = static_cast<T *>(current_allocated_ptr) + 1;
        current_alloacted_unused -= 1;
        return ptr;
    }
    void deallocate(void *ptr) { available_stack.push(ptr); }
};

struct IntrusiveListNode {
    int val;
    IntrusiveListNode *pre, *nxt;
    IntrusiveListNode() : pre(nullptr), nxt(nullptr) {}
    IntrusiveListNode(int x) {
        val = x;
        pre = nxt = nullptr;
    }

    void *operator new(size_t size) {
        assert(size == sizeof(IntrusiveListNode));
        return pool.allocate();
    }
    void operator delete(void *ptr) noexcept { pool.deallocate(ptr); }
    void *operator new(size_t size, size_t align) = delete;
    void *operator new[](size_t size) = delete;
    void operator delete[](void *ptr) noexcept = delete;

    static void recreate_pool() { pool.release_all(); }

 private:
    static inline large_unsynchronized_pool_allocator<IntrusiveListNode> pool{};
};

class IntrusiveList {
 public:
    IntrusiveList() { head = tail = nullptr, size_ = 0; }
    // The deconstructor indentionally leak the memory.
    // Use IntrusiveListNode::recreate_pool to free all memory allocated by list.
    ~IntrusiveList() = default;

    IntrusiveList(const IntrusiveList &) = delete;
    IntrusiveList(IntrusiveList &&other) noexcept
        : head(std::exchange(other.head, nullptr)),
          tail(std::exchange(other.tail, nullptr)),
          size_(std::exchange(other.size_, 0)) {}
    IntrusiveList &operator=(const IntrusiveList &) = delete;
    IntrusiveList &operator=(IntrusiveList &&other) noexcept {
        if (this == &other) return *this;
        std::swap(other.head, head);
        std::swap(other.tail, tail);
        std::swap(other.size_, size_);
        return *this;
    }

    void insert(int x) {
        IntrusiveListNode *t = new IntrusiveListNode;
        t->val = x, t->pre = tail, t->nxt = nullptr;
        if (head == nullptr)
            head = tail = t;
        else
            tail->nxt = t, tail = t;
        size_++;
    }

    void erase(IntrusiveListNode *t) {
        IntrusiveListNode *pre = t->pre, *nxt = t->nxt;
        if (size_ == 1)
            head = tail = nullptr;
        else if (t == head)
            head = t->nxt;
        else if (t == tail)
            pre->nxt = nullptr, tail = pre;
        else
            pre->nxt = nxt, nxt->pre = pre;

        t->pre = t->nxt = nullptr;
        delete t;
        size_--;
    }

    void merge(IntrusiveList &other) {
        if (other.size_ == 0) return;
        if (head == tail && tail == nullptr) {
            *this = std::move(other);
            return;
        }
        tail->nxt = other.head;
        other.head->pre = tail;
        tail = other.tail;
        size_ += other.size_;
        other.size_ = 0;
        other.head = other.tail = nullptr;
    }

    int size() { return size_; }

    int &front() { return head->val; }
    int &back() { return tail->val; }

    void clear() {
        for (IntrusiveListNode *p = head, *q; p != nullptr; p = q) {
            q = p->nxt;
            delete p;
        }
        head = tail = nullptr, size_ = 0;
    }

    IntrusiveListNode *get_pre(IntrusiveListNode *t) { return t->pre; }
    IntrusiveListNode *get_nxt(IntrusiveListNode *t) { return t->nxt; }

    IntrusiveListNode *head, *tail;

 private:
    int size_;
};
