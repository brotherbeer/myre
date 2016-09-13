#pragma once
#include <string>
#include <climits>
#include <cstdlib>


#define _RE_DEBUG

#if ULONG_MAX == 0xffffffffUL
#define __WORD_BYTES 4
#elif ULONG_MAX == 0xffffffffffffffffUL
#define __WORD_BYTES 8
#else
#error ULONG_MAX unknown
#endif

#if __GNUC__ >= 3
#define __exp1(x) __builtin_expect(!!(x), 1)
#define __exp0(x) __builtin_expect(!!(x), 0)
#else
#define __exp1(x) (x)
#define __exp0(x) (x)
#endif

#ifdef __GNUC__
#define __must_inline(x) __attribute__((always_inline)) x
#elif defined(_MSC_VER)
#define __must_inline(x) __forceinline x
#else
#define __must_inline(x) inline x 
#endif

#define SPACES "\t\n\v\f\r "
#define WORD_CHARS "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"
#define BOUND_CHARS "\a\b\t\n\v\f\r !\"#$%&'()*+,-./:;<=>?@[\\]^`{|}~\x7f"


namespace myre {

const long MATCH_BEGIN = 1;
const long MATCH_END = 2;
const long MATCH_MIN = 4;
const long MATCH_ICASE = 8;
const long MATCH_WLBDY = 16;
const long MATCH_WRBDY = 32;
const long MATCH_WORD = 48;
const long SEARCH_ALL = -1;
const long SEARCH_FIRST = 1;
const long BAD_CHAR_OPT = 1024;

typedef signed int error_t;
typedef unsigned short id_t;
typedef unsigned char input_t;
typedef unsigned char item_type_t;
typedef unsigned short position_t;
typedef unsigned char rechar_t;
typedef unsigned short U16;
typedef unsigned int U32;
typedef unsigned long long U64;
typedef unsigned long word_t;  // 不同编译器unsigned long 有不同定义，后续落实

const size_t THR = 128;

// common char
const item_type_t CHAR = 0;
const item_type_t ECHAR = 1;

// char set
const item_type_t SET_d = 2;   // \d
const item_type_t SET_D = 3;   // \D
const item_type_t SET_w = 4;   // \w
const item_type_t SET_W = 5;   // \W
const item_type_t SET_s = 6;   // \s
const item_type_t SET_S = 7;   // \S
const item_type_t SET_ANY = 8; // .
const item_type_t SET = 9;     // [x-y]

const item_type_t OK = 10;

// operators
const item_type_t STAR = 11;  // *
const item_type_t PLUS = 12;  // +
const item_type_t QUST = 13;  // ?
const item_type_t RNG  = 14;  // {x,y}
const item_type_t BOE  = 15;  // begin of the expression
const item_type_t LP   = 16;  // (
const item_type_t OR   = 17;  // |
const item_type_t CAT  = 18;  // cat char or set, no actual character correspondence
const item_type_t RP   = 19;  // )
const item_type_t LB   = 20;  // [
const item_type_t RB   = 21;  // ]

// errors
const error_t NO_ERR = 0;
const error_t ERR = -1000;
const error_t ERR_SET = ERR - 1;  // error in set
const error_t ERR_RNG = ERR - 2;  // error in set
const error_t ERR_ESC = ERR - 3;  // bad escape char
const error_t ERR_HEX = ERR - 4;  // bad \x<0-f><0-f> escape
const error_t ERR_PAT = ERR - 5;  // parentheses donot match 
const error_t ERR_SYN = ERR - 6;  // syntax error
const error_t ERR_EMP = ERR - 7;  // empty pattern
const error_t NOT_SUPPORT = ERR - 100;
const error_t ERR_TOO_MUCH_STATUS = NOT_SUPPORT - 1;
const error_t ERR_RNG_NESTED = NOT_SUPPORT - 2;

const int RESULTS_ENOUGH = 1;
const int RESULTS_NOT_ENOUGH = 0;

const word_t A = (word_t)(1) << (sizeof(word_t) * 8 - 1);
#if __WORD_BYTES == 8
const word_t X = 6;
const word_t Y = 63;
#else
const word_t X = 5;
const word_t Y = 31;
#endif

template <class T, class U> __must_inline(T*) cond_ptr(T* p, U b) {return (T*)(size_t(p) * (!!b));}

struct default_memory_allocator_t
{
    static void* allocate(size_t s)
    {
        return malloc(s);
    }

    static void* reallocate(size_t s, void* p)
    {
        return realloc(p, s);
    }

    static void deallocate(void* p)
    {
        free(p);
    }
};

template <class MEM> struct _set_t
{
    word_t* _dat;
    size_t _cap;

    _set_t(): _dat(NULL), _cap(0)
    {}

    _set_t(const _set_t<MEM>& another): _dat(NULL), _cap(another._cap)
    {
        if (_cap)
        {
            _dat = (word_t*)MEM::allocate(_cap * sizeof(word_t));
            word_t* p = _dat;
            word_t* e = _dat + _cap;
            word_t* q = another._dat;
            while (p != e)
            {
                *p++ = *q++;
            }
        }
    }

    ~_set_t()
    {
        MEM::deallocate(_dat);
        _dat = NULL;
        _cap = 0;
    }

    void add(size_t x)
    {
        if (__exp1(__in_capacity(x)))
        {
            __add(x);
        }
        else
        {
            __expand((x >> X) + 1);
            __add(x);
        }
    }

    bool has(size_t x) const
    {
        if (__in_capacity(x))
        {
            return (_dat[x >> X] & (A >> (x & Y))) != 0;
        }
        return false;
    }

    bool empty() const
    {
        return _dat == NULL;
    }

    void merge(const _set_t<MEM>& another)
    {
        if (_cap >= another._cap)
        {
            __merge(another);
        }
        else
        {
            __expand(another._cap);
            __merge(another);
        }
    }

    bool equal(const _set_t<MEM>* another)
    {
        if (_cap == another->_cap)
        {
            word_t* e = _dat - 1;
            word_t* p = e + _cap;
            word_t* q = another->_dat - 1 + _cap;
            while (p != e && *p == *q)
            {
                p--;
                q--;
            }
            return p == e;
        }
        return false;
    }

#if __GNUC__ > 3
    int first() const
    {
        word_t* p = _dat;
        while (!*p)
        {
            p++;
        }
        return ((p - _dat) << X) + __builtin_clzl(*p);
    }

    int last() const
    {
        return (_cap << X) - __builtin_ffsl(*(_dat + _cap - 1));
    }
#else
    int first() const
    {
        word_t* p = _dat;
        while (!*p)
        {
            p++;
        }
        int r = (p - _dat) << X;
        while (!has(r))
        {
            r++;
        }
        return r;
    }

    int last() const
    {
        int r = (_cap << X) - 1;
        while (!has(r))
        {
            r--;
        }
        return r;
    }
#endif

    __must_inline(void) __add(size_t x)
    {
        _dat[x >> X] |= A >> (x & Y);
    }

    void __merge(const _set_t<MEM>& another)
    {
        word_t* p = _dat;
        word_t* q = another._dat;
        word_t* e = p + another._cap;
        while (p != e)
        {
            *p++ |= *q++;
        }
    }

    void __expand(size_t newcap)
    {
        _dat = (word_t*)MEM::reallocate(newcap * sizeof(word_t), _dat);
        word_t* p = _dat + _cap;
        word_t* e = _dat + newcap;
        while (p != e)
        {
            *p++ = 0;
        }
        _cap = newcap;
    }

    __must_inline(bool) __in_capacity(size_t x) const
    {
        return (x >> X) < _cap;
    }
};

template <class MEM> struct _char_set_t
{
#if __WORD_BYTES == 8
    static const size_t _cap = 4;
#else
    static const size_t _cap = 8;
#endif
    word_t _dat[_cap];

    _char_set_t()
    {
        clear();
    }

    void clear()
    {
        *((U64*)_dat + 0) = 0;
        *((U64*)_dat + 1) = 0;
        *((U64*)_dat + 2) = 0;
        *((U64*)_dat + 3) = 0;
    }

    static _char_set_t* create()
    {
        _char_set_t* p = (_char_set_t*)MEM::allocate(sizeof(_char_set_t));
        ::new (p) _char_set_t();
        return p;
    }

    static void release(_char_set_t* p)
    {
        MEM::deallocate(p);
    }

    void copy(const _char_set_t<MEM>* another)
    {
        *((U64*)_dat + 0) = *((U64*)another->_dat + 0);
        *((U64*)_dat + 1) = *((U64*)another->_dat + 1);
        *((U64*)_dat + 2) = *((U64*)another->_dat + 2);
        *((U64*)_dat + 3) = *((U64*)another->_dat + 3);
    }

    void add(unsigned char x)
    {
        _dat[x >> X] |= A >> (x & Y);
    }

    void add(const _char_set_t<MEM>* another)
    {
        *((U64*)_dat + 0) |= *((U64*)(another->_dat) + 0);
        *((U64*)_dat + 1) |= *((U64*)(another->_dat) + 1);
        *((U64*)_dat + 2) |= *((U64*)(another->_dat) + 2);
        *((U64*)_dat + 3) |= *((U64*)(another->_dat) + 3);
    }

    void add(const char* s)
    {
        while (*s)
        {
            add(*s++);
        }
    }

    void add_range(unsigned char a, unsigned char b)
    {
        for (unsigned char x = a; x <= b; x++)
        {
            add(x);
        }
    }

    void invert()
    {
        *((U64*)_dat + 0) = ~*((U64*)_dat + 0);
        *((U64*)_dat + 1) = ~*((U64*)_dat + 1);
        *((U64*)_dat + 2) = ~*((U64*)_dat + 2);
        *((U64*)_dat + 3) = ~*((U64*)_dat + 3);
    }

    bool has(unsigned char x) const
    {
        return (_dat[x >> X] & (A >> (x & Y))) != 0;
    }

    void ignore_case()
    {
        U32* AZ = (U32*)_dat + 3;
        U32* az = (U32*)_dat + 4;
        *AZ |= *az & 0x7FFFFFE0;
        *az |= *AZ & 0x7FFFFFE0;
    }

#if __GNUC__ > 3
    unsigned char first() const
    {
        word_t* p = (word_t*)_dat;
        while (!*p)
        {
            p++;
        }
        return ((p - _dat) << X) + __builtin_clzl(*p);
    }

    unsigned char last() const
    {
        word_t* p = (word_t*)_dat + _cap - 1;
        while (!*p)
        {
            p--;
        }
        return ((p - _dat + 1) << X) - __builtin_ffsl(*p);
    }
#else
    unsigned char first() const
    {
        unsigned char r;
        word_t* p = (word_t*)_dat;
        while (!*p)
        {
            p++;
        }
        r = (p - _dat) << X;
        while (!has(r))
        {
            r++;
        }
        return r;
    }

    unsigned char last() const
    {
        unsigned char r;
        word_t* p = (word_t*)_dat + _cap - 1;
        while (!*p)
        {
            p--;
        }
        r = (unsigned char)(((p - _dat) << X) + Y);
        while (!has(r))
        {
            r--;
        }
        return r;
    }
#endif
};

template <class T, class MEM> struct array_t
{
    typedef T value_t;
    typedef T* iterator_t;
    typedef const T* const_iterator_t;
    typedef T& reference_t;
    typedef const T& const_reference_t;

    T *_dat, *_end, *_lim;

    array_t(size_t cap = 0): _dat(NULL), _end(NULL), _lim(NULL)
    {
        if (cap)
        {
            _dat = _end = (T*)MEM::allocate(cap * sizeof(T));
            _lim = _dat + cap;
        }
    }

    ~array_t()
    {
        clear();
        MEM::deallocate(_dat);
        _dat = _end = _lim = NULL;
    }

    static array_t* create(size_t cap = 0)
    {
        array_t* p = (array_t*)MEM::allocate(sizeof(array_t));
        ::new (p) array_t(cap);
        return p;
    }

    static void release(array_t* p)
    {
        if (p)
        {
            p->~array_t();
            MEM::deallocate(p);
        }
    }

    void reserve(size_t cap)
    {
        if (size_t(_lim - _dat) < cap)
        {
            size_t size = _end - _dat;
            _dat = (T*)MEM::reallocate(cap * sizeof(T), _dat);
            _lim = _dat + cap;
            _end = _dat + size;
        }
    }

    void clear()
    {
        for (T* p = _dat; p != _end; p++)
        {
            p->~T();
        }
        _end = _dat;
    }

    iterator_t begin()
    {
        return _dat;
    }

    iterator_t end()
    {
        return _end;
    }

    const_iterator_t begin() const
    {
        return _dat;
    }

    const_iterator_t end() const
    {
        return _end;
    }

    bool empty() const
    {
        return _end == _dat;
    }

    bool not_empty() const
    {
        return _end != _dat;
    }

    reference_t first()
    {
        return *_dat;
    }

    const_reference_t first() const
    {
        return *_dat;
    }

    void append(const T& v)
    {
        if (__exp1(_end != _lim))
        {
            ::new (_end++) T(v);
        }
        else
        {
            __expand();
            ::new (_end++) T(v);
        }
    }

    void append(const_iterator_t p, const_iterator_t e)
    {
        ptrdiff_t x = e - p;
        if (_lim - _end < x)
        {
            __expand(x);
        }
        while (p != e)
        {
            ::new (_end++) T(*p++);
        }
    }

    void append(const array_t<T, MEM>* another)
    {
        append(another->begin(), another->end());
    }

    void remove(iterator_t p)
    {
        iterator_t q = p + 1;
        p->~T();
        while (q != _end)
        {
            ::new (p++) T(*q);
            q++->~T();
        }
        _end = p;
    }

    void pop_back()
    {
        (--_end)->~T();
    }

    void pop_front()
    {
        remove(_dat);
    }

    reference_t last()
    {   
        return *(_end - 1);
    }

    const_reference_t last() const
    {
        return *(_end - 1);
    }

    reference_t operator [](size_t i)
    {
        return _dat[i];
    }

    const_reference_t operator [](size_t i) const
    {
        return _dat[i];
    }

    size_t size() const
    {
        return _end - _dat;
    }

    size_t capacity() const
    {
        return _lim - _dat;
    }

    void __expand()
    {
        size_t size = _end - _dat;
        size_t cap = (_lim - _dat) << 1;
        if (__exp0(!cap))
        {
            cap = sizeof(void*);
        }
        _dat = (T*)MEM::reallocate(cap * sizeof(T), _dat);
        _end = _dat + size;
        _lim = _dat + cap;
    }

    void __expand(size_t x)
    {
        size_t size = _end - _dat;
        size_t cap = _lim - _dat + x;
        _dat = (T*)MEM::reallocate(cap * sizeof(T), _dat);
        _end = _dat + size;
        _lim = _dat + cap;
    }
};

template <class T, class MEM> struct stack_t: public array_t<T, MEM>
{
    inline void push(const T& v)
    {
        this->append(v);
    }

    inline void pop()
    {
        this->pop_back();
    }

    inline T& top()
    {
        return this->last();
    }
};

template <class MEM> struct _exp_item_t 
{
    typedef _char_set_t<MEM> char_set_t;

    item_type_t type;
    union
    {
        char ch;
        char_set_t* charset;
        struct {unsigned int a, b;};
    };

    _exp_item_t(item_type_t _type_): type(_type_)
    {
        switch (type)
        {
            case SET_d:
                charset = char_set_t::create();
                charset->add_range('0', '9');
                break;
            case SET_D:
                charset = char_set_t::create();
                charset->add_range('0', '9');
                charset->invert();
                break;
            case SET_w:
                charset = char_set_t::create();
                charset->add(WORD_CHARS);
                break;
            case SET_W:
                charset = char_set_t::create();
                charset->add(WORD_CHARS);
                charset->invert();
                break;
            case SET_s:
                charset = char_set_t::create();
                charset->add(SPACES);
                break;
            case SET_S:
                charset = char_set_t::create();
                charset->add(SPACES);
                charset->invert();
                break;
            case SET_ANY:
                charset = char_set_t::create();
                charset->add_range(0, 255);
                break;
        }
    }

    _exp_item_t(char _ch_): type(CHAR)
    {
        ch = _ch_;
    }

    _exp_item_t(item_type_t _type_, char _ch_): type(_type_)
    {
        ch = _ch_;
    }

    _exp_item_t(char_set_t* _charset_): type(SET)
    {
        charset = _charset_;
    }

    _exp_item_t(unsigned int _a_, unsigned int _b_): type(RNG)
    {
        a = _a_;
        b = _b_;
    }

    _exp_item_t(const _exp_item_t<MEM>& another): type(another.type)
    {
        if (is_set())
        {
            charset = char_set_t::create();
            charset->copy(another.charset);
        }
        else
        {
            ch = another.ch;
            a = another.a;
            b = another.b;
        }
    }

    ~_exp_item_t()
    {
        if (is_set())
        {
            char_set_t::release(charset);
            charset = NULL;
        }
    }

    bool is_set() const
    {
        return type <= SET && type >= SET_d;
    }
};

template <class MEM> struct _node_t
{
    typedef _set_t<MEM> pos_set_t;
    typedef _char_set_t<MEM> char_set_t;

    pos_set_t first;
    pos_set_t last;
    pos_set_t follow;
    char_set_t charset;

    _node_t* r;
    _node_t* l;

    item_type_t type;
    bool nul;
    position_t pos;

    _node_t(_exp_item_t<MEM>* item): r(NULL), l(NULL), type(item->type), nul(false)
    {
        if (type <= ECHAR)
        {
            charset.add(item->ch);
        }
        else if (type <= SET)
        {
            charset.copy(item->charset);
        }
    }

    _node_t(item_type_t t): r(NULL), l(NULL), type(t), nul(false)
    {}

    _node_t(const _node_t& another):
        first(another.first), last(another.last), follow(another.follow), charset(another.charset),
        r(another.r), l(another.l), type(another.type), nul(another.nul), pos(another.pos)
    {}

    int has(unsigned char x)
    {
        return charset.has(x);
    }
};

template <class MEM> struct _APE_t 
{
    typedef _char_set_t<MEM> char_set_t;
    typedef _exp_item_t<MEM> exp_item_t;
    typedef _node_t<MEM> node_t;
    typedef array_t<exp_item_t, MEM> exp_items_t;
    typedef stack_t<item_type_t, MEM> opt_stack_t;
    typedef array_t<node_t, MEM> nodes_t;
    typedef array_t<node_t*, MEM> pos_nodes_t;

    const char* _exp;
    size_t _explen;
    exp_items_t* _items;
    nodes_t _nodes;
    pos_nodes_t posnodes;
    opt_stack_t stack;
    error_t _en;
    position_t _pos;

    bool _bor;
    bool _bstar;
    bool _bplus;
    bool _bqust;

    bool matchbegin;
    bool matchend;

    _APE_t(const char* exp, size_t explen):
        _exp(exp), _explen(explen),
        _items(NULL), _en(NO_ERR), _pos(0),
        _bor(false), _bstar(false), _bplus(false), _bqust(false),
        matchbegin(false), matchend(false)
    {}

    ~_APE_t()
    {
        exp_items_t::release(_items);
        _items = NULL;
    }

    error_t parse()
    {
        if (__preprocess() == NO_ERR)
        {
            _nodes.reserve(_items->size() + 3);
            for (exp_item_t* item = _items->begin(); item != _items->end(); item++)
            {
                __add_node(item);
            }
            while (!stack.empty())
            {
                _nodes.append(stack.top());
                stack.pop();
            }
            _nodes.append(OK);
            node_t* pnode = &_nodes.last();
            pnode->pos = _pos;
            posnodes.append(pnode);
            _nodes.append(CAT);
        }
        return _en;
    }

    node_t* node_at_pos(position_t pos)
    {
        return posnodes[pos];
    }

    position_t reception_pos() const
    {
        return _pos;
    }

    error_t error() const
    {
        return _en;
    }

    bool has_or() const
    {
        return _bor;
    }

    size_t to_sample_pattern(unsigned char** pat)
    {
        exp_item_t* p = _items->begin();
        exp_item_t* e = _items->end();
        unsigned char* pc = *pat = (unsigned char*)MEM::allocate(_items->size());
        while (p != e)
        {
            if (p->type <= ECHAR)
            {
                *pc++ = p->ch;
            }
            p++;
        }
        return pc - *pat;
    }

    void __add_node(exp_item_t* item)
    {
        item_type_t type = item->type;
        if (type <= QUST)
        {
            _nodes.append(node_t(item));
            if (type <= SET)
            {
                node_t* pnode = &_nodes.last();
                pnode->pos = _pos++;
                posnodes.append(pnode);
            }
        }
        else if (type == LP)
        {
            stack.push(LP);
        }
        else if (type <= CAT)
        {
            while (!stack.empty() && type - stack.top() <= 0)
            {
                _nodes.append(stack.top());
                stack.pop();
            }
            stack.push(type);
        }
        else if (type == RP)
        {
            while (stack.top() != LP)
            {
                _nodes.append(stack.top());
                stack.pop();
            }
            stack.pop();
        }
    }

    int __preprocess()
    {
        exp_items_t tmp, *items = NULL;
        const char *p, *e, *q;
        unsigned int a, b;
        bool notSET = true, hasrng = false;
        item_type_t pretype = BOE, curtype;

        #define GOTO_ERR(x) do{_en=(x);goto ERR;}while(0)

        if (!_explen)
        {
            GOTO_ERR(ERR_EMP);
        }
        if (*_exp == '^')
        {
            matchbegin= true;
            _exp++;
            _explen--;
        }
        if (*(_exp + _explen - 1) == '$')
        {
            matchend = true;
            _explen--;
        }
    
        items = exp_items_t::create();
        p = _exp;
        e = _exp + _explen;

        while (p != e)
        {
            switch (*p)
            {
                case '(':
                    if (notSET)
                    {
                        tmp.append(LP);
                    }
                    else
                    {
                        tmp.append('(');
                    }
                    break;
                case ')':
                    if (notSET)
                    {
                        tmp.append(RP);
                    }
                    else
                    {
                        tmp.append(')');
                    }
                    break;
                case '[':
                    if (notSET)
                    {
                        tmp.append(LB);
                        notSET = false;
                    }
                    else
                    {
                        tmp.append('[');
                    }
                    break;
                case ']':
                    if (notSET)
                    {
                        tmp.append(']');
                    }
                    else
                    {
                        tmp.append(RB);
                        notSET = true;
                    }
                    break;
                case '{':
                    if (notSET && (q = __parse_range(p + 1, e, &a, &b)))
                    {
                        if (b < a)
                        {
                            GOTO_ERR(ERR_RNG);
                        }
                        hasrng = true;
                        tmp.append(exp_item_t(a, b));
                        p = q;
                        _bqust = _bqust || a != b;
                    }
                    else
                    {
                        tmp.append('{');
                    }
                    break;

                case '.':
                    if (notSET)
                    {
                        tmp.append(SET_ANY);
                    }
                    else
                    {
                        tmp.append('.');
                    }
                    break;
                case '|':
                    if (notSET)
                    {
                        tmp.append(OR);
                        _bor = true;
                    }
                    else
                    {
                        tmp.append('|');
                    }
                    break;
                case '*':
                    if (notSET)
                    {
                        tmp.append(STAR);
                        _bstar = true;
                    }
                    else
                    {
                        tmp.append('*');
                    }
                    break;
                case '+':
                    if (notSET)
                    {
                        tmp.append(PLUS);
                        _bplus = true;
                    }
                    else
                    {
                        tmp.append('+');
                    }
                    break;
                case '?':
                    if (notSET)
                    {
                        tmp.append(QUST); 
                        _bqust = true;
                    }
                    else
                    {
                        tmp.append('?'); 
                    }
                    break;
                case '\\':
                    if (p + 1 != e)
                    {
                        p++;
                        switch (*p)
                        {
                            case 'd': tmp.append(SET_d); break;
                            case 'D': tmp.append(SET_D); break;
                            case 'w': tmp.append(SET_w); break;
                            case 'W': tmp.append(SET_W); break;
                            case 's': tmp.append(SET_s); break;
                            case 'S': tmp.append(SET_S); break;

                            case '\\': case '{': case '}': case '[': case ']':
                            case '.': case '*': case '?': case '+': case '|':
                            case '-': case '^': case '$':
                                tmp.append(exp_item_t(ECHAR, *p));
                                break;

                            case 'x': 
                                if (p + 2 < e)
                                {
                                    int h0 = __hexchar2int(*(p + 1));
                                    int h1 = __hexchar2int(*(p + 2));
                                    if (h0 != -1 && h1 != -1)
                                    {
                                        tmp.append(exp_item_t(ECHAR, (char)((h0 << 4) + h1)));
                                        p += 2;
                                        break;
                                    }
                                }
                                GOTO_ERR(ERR_HEX);

                            default:
                                GOTO_ERR(ERR_ESC);
                        }
                    }
                    else
                    {
                        GOTO_ERR(ERR_ESC);
                    }
                    break;

                default:
                    tmp.append(*p);
            }
            p++;
        }

        for (exp_item_t* i = tmp.begin(); i != tmp.end(); i++)
        {
            if (i->type != LB)
            {
                curtype = i->type;
                if (!__deal_with_cat(pretype, curtype, items))
                {
                    GOTO_ERR(ERR_SYN);
                }
                items->append(*i);
            }
            else
            {
                exp_item_t set(SET);
                if (!(i = __parse_set(i + 1, tmp.end(), &set.charset)))
                {
                    GOTO_ERR(ERR_SET);
                }
                curtype = SET;
                if (!__deal_with_cat(pretype, curtype, items))
                {
                    GOTO_ERR(ERR_SYN);
                }
                items->append(set);
            }
            pretype = curtype;
        }

        if (!__check_parentheses(items))
        {
            GOTO_ERR(ERR_PAT);
        }

        if (!hasrng)
        {
            _items = items;
        }
        else
        {
            _items = exp_items_t::create(); 
            exp_item_t *x = items->begin(), *y = x, *z;

            while (x != items->end())
            {
                if (y->type == RNG)
                {
                    GOTO_ERR(ERR_RNG);
                }
                else if (x->type == RNG)
                {
                    if ((x - 1)->type == RP)
                    {
                        if (!(z = __get_matched_LP(y, x - 1)))
                        {
                            GOTO_ERR(ERR_RNG_NESTED);
                        }
                    }
                    else
                    {
                        z = x - 1;
                    }
                    exp_items_t* exprng = __expand_rng(z, x, x->a, x->b);
                    _items->append(y, z);
                    if (!exprng->empty())
                    {
                        _items->append(LP);
                        _items->append(exprng);
                        _items->append(RP);
                    }
                    exp_items_t::release(exprng);
                    x++;
                    y = x;
                }
                else
                {
                    x++;
                }
            }
            _items->append(y, x);
            exp_items_t::release(items);
        }
        return NO_ERR;

   ERR: 
        exp_items_t::release(items);
        exp_items_t::release(_items);
        _items = NULL;
        return _en;
    }

    bool __deal_with_cat(item_type_t pre, item_type_t cur, exp_items_t* items)
    {
        int needcat = __need_add_cat(pre, cur);
        if (needcat == 1)
        {
            items->append(CAT);
        }
        return needcat != -1;
    }

    bool __check_parentheses(exp_items_t* items)
    {
        int x = 0;
        for (exp_item_t* p = items->begin(); p != items->end(); p++)
        {
            if (p->type == LP)
            {
                ++x;
            }
            else if (p->type == RP)
            {
                if (--x < 0)
                {
                    return false;
                }
            }
        }
        return x == 0;
    }

    const char* __parse_range(const char* p, const char* e, unsigned int* a, unsigned int* b)
    {
        const char *p0 = p;
        *a = *b = 0;
        while (p != e)
        {
            switch (*p)
            {
                case '0': case'1': case'2': case'3': case'4':
                case '5': case'6': case'7': case'8': case'9':
                    *a = *a * 10 + *p++ - '0';
                    continue;
                case ',':
                    p++;
                    while (p != e)
                    {
                        switch (*p)
                        {
                            case '0': case'1': case'2': case'3': case'4':
                            case '5': case'6': case'7': case'8': case'9':
                                *b = *b * 10 + *p++ - '0';
                                continue;
                            case '}':
                                if (!*b)
                                {
                                    *b = -1;
                                }
                                return p;
                            default:
                                goto NOTMATCH;
                        }
                    }
                    continue;
                case '}':
                    if (p != p0)
                    {
                        *b = *a;
                        return p;
                    }
                default:
                    goto NOTMATCH;
            }
        }

    NOTMATCH:
        return NULL;
    }

    exp_item_t* __parse_set(exp_item_t* p, exp_item_t* e, char_set_t** pcharset)
    {
        if (p != e)
        {
            bool reverse = false;
            exp_item_t* q = p, *p0 = p;
            while (q != e && q->type != RB)
            {
                q++;
            }
            if (q == e || q == p)
            {
                goto ERR;
            }
            if (p->type == CHAR && p->ch == '^')
            {
                if (p + 1 != q)
                {
                    reverse = true;
                    p++;
                }
                else
                {
                    goto ERR;
                }
            }

            *pcharset = char_set_t::create();
            while (p != q)
            {
                switch (p->type)
                {
                    case CHAR:
                        if (p->ch != '-')
                        {
                            (*pcharset)->add(p->ch);
                        }
                        else
                        {
                            exp_item_t *x = p - 1, *y = p + 1;
                            if (x >= p0 && y < q)
                            {
                                if (x->type <= ECHAR && y->type <= ECHAR)
                                {
                                    unsigned char c0 = x->ch;
                                    unsigned char c1 = y->ch;
                                    if (c1 < c0)
                                    {
                                        c0 = y->ch;
                                        c1 = x->ch;
                                    }
                                    while (c0 != c1)
                                    {
                                        (*pcharset)->add(c0++);
                                    }
                                }
                                else
                                {
                                    char_set_t::release(*pcharset);
                                    goto ERR;
                                }
                            }
                            else
                            {
                                (*pcharset)->add('-');
                            }
                        }
                        break;
                    case ECHAR:
                        (*pcharset)->add(p->ch);
                        break;
                    case SET_d:
                    case SET_D:
                    case SET_w:
                    case SET_W:
                    case SET_s:
                    case SET_S:
                    case SET_ANY:
                        (*pcharset)->add(p->charset);
                        break;
                }
                p++;
            }
            if (reverse)
            {
                (*pcharset)->invert();
            }
            return q;
        }
    ERR:
        *pcharset = NULL;
        return NULL;
    }

    exp_item_t* __get_matched_LP(exp_item_t* begin, exp_item_t* p)
    {
        int x = 0;
        while (p >= begin)
        {
            if (p->type == RP)
            {
                ++x;
            }
            else if (p->type == LP)
            {
                if (--x == 0)
                {
                    return p;
                }
            }
            p--;
        }
        return NULL;
    }

    exp_items_t* __expand_rng(exp_item_t* p, exp_item_t* e, int a, int b)
    {
        exp_items_t* res = exp_items_t::create();
        if (a == 0 && b == 0)
        {
            return res;
        }
        res->append(p, e);
        for (int i = 1; i < a; i++)
        {
            res->append(CAT);
            res->append(p, e);
        }
        if (a == 0 && b == -1)
        {
            res->append(STAR);
        }
        else if (a == 0 && b == 1)
        {
            res->append(QUST);
        }
        else if (a == 0 && b > 1)
        {
            res->append(QUST);
            for (int i = 1; i < b; i++)
            {
                res->append(CAT);
                res->append(p, e);
                res->append(QUST);
            }
        }
        else if (a > 0 && b == -1)
        {
            res->append(CAT);
            res->append(p, e);
            res->append(STAR);
        }
        else for (int i = 0; i < b - a; i++)
        {
            res->append(CAT);
            res->append(p, e);
            res->append(QUST);
        }
        return res;
    }

    static int __need_add_cat(item_type_t a, item_type_t b)
    {
        switch (a)
        {
            case CHAR:
            case ECHAR:
            case SET_d:
            case SET_D:
            case SET_w:
            case SET_W:
            case SET_s:
            case SET_S:
            case SET_ANY:
            case SET:
                return b <= SET || b == LP;
            case STAR:
            case PLUS:
            case QUST:
            case RNG:
                switch (b)
                {
                    case CHAR: case ECHAR:
                    case SET_d: case SET_D: case SET_w: case SET_W:
                    case SET_s: case SET_S: case SET_ANY: case SET:
                        return 1;
                    case STAR: case PLUS: case QUST: case RNG:
                        return -1;
                    case OR:
                        return 0;
                    case LP:
                        return 1;
                    case RP:
                        return 0;
                }
            case BOE:
            case LP:
            case OR:
                switch (b)
                {
                    case CHAR: case ECHAR:
                    case SET_d: case SET_D: case SET_w: case SET_W:
                    case SET_s: case SET_S: case SET_ANY: case SET:
                        return 0;
                    case STAR: case PLUS: case QUST: case RNG:
                        return -1;
                    case LP:
                        return 0;
                    case OR:
                    case RP:
                        return -1;
                }
            case RP:
                switch (b)
                {
                    case CHAR: case ECHAR:
                    case SET_d: case SET_D: case SET_w: case SET_W:
                    case SET_s: case SET_S: case SET_ANY: case SET:
                        return 1;
                    case STAR: case PLUS: case QUST: case RNG:
                        return 0;
                    case LP:
                        return 1;
                    case OR:
                    case RP:
                        return 0;
                }
        }
        return -1;
    }

    static int __hexchar2int(char x)
    {
        if (x >= '0' && x <= '9')
        {
            return x - '0';
        }
        else if (x >= 'a' && x <= 'f')
        {
            return 10 + x - 'a';
        }
        else if (x >= 'A' && x <= 'F')
        {
            return 10 + x - 'A';
        }
        return -1;
    }
};

template <class MEM> struct _tree_t: public _APE_t<MEM>
{
protected:
    typedef _node_t<MEM> node_t;
    stack_t<node_t*, MEM> _node_stack;

public:
    node_t* root;

    _tree_t(const char* exp, size_t explen): _APE_t<MEM>(exp, explen)
    {}

    error_t build()
    {
        if (_APE_t<MEM>::parse() == NO_ERR)
        {
            if (__build())
            {
                __get_follow(root);
            }
        }
        return this->_en;
    }

    void ignore_case()
    {
        for (node_t** i = this->posnodes.begin(); i != this->posnodes.end(); i++)
        {
            (*i)->charset.ignore_case();
        }
    }

    void debug_follow(const _set_t<MEM>& follow) const
    {
        if (follow.empty())
        {
            printf("### empty");
        }
        else for (int j = 0; j <= follow.last(); j++)
        {
            if (follow.has(j))
            {
                printf("%d ", j);
            }
        }
    }

    void debug() const
    {
        for (const node_t* i = this->_nodes.begin(); i != this->_nodes.end(); i++)
        {
            const typename node_t::pos_set_t& follow = i->follow;
            item_type_t type = i->type;
            int pos = i->pos;

            if (type <= OK)
            {
                switch (type)
                {
                    case SET_d:   printf("\\d\t%d\t\t", pos); break;
                    case SET_w:   printf("\\w\t%d\t\t", pos); break;
                    case SET_s:   printf("\\s\t%d\t\t", pos); break;
                    case SET_D:   printf("\\D\t%d\t\t", pos); break;
                    case SET_W:   printf("\\W\t%d\t\t", pos); break;
                    case SET_S:   printf("\\S\t%d\t\t", pos); break;
                    case SET_ANY: printf("ANY\t%d\t\t", pos); break;
                    case SET:     printf("SET\t%d\t\t", pos); break;
                    case OK:      printf("OK\t%d\t\t",  pos); break;
                    default:      printf("%c\t%d\t\t", i->charset.first(), pos);
                }
                debug_follow(follow);
                printf("\n");
            }
            else
            {
                switch (type)
                {
                    case CAT:  printf("CAT\n");  break;
                    case OR:   printf("OR\n");   break;
                    case STAR: printf("STAR\n"); break;
                    case PLUS: printf("PLUS\n"); break;
                    case QUST: printf("QUST\n"); break;
                    default:   printf("ERROR\t%d\n", type); break;
                }
            }
        }
        printf("reception pos: %d\n", this->reception_pos());
        printf("\n");
        printf("accepted chars:\n");
        for (int i = 0; i < 256; i++)
        {
            if (root->charset.has(i))
            {
                if (i >= 32 && i < 127)
                {
                    printf("%c ", i);
                }
                else
                {
                    printf("\\x%x ", i);
                }
            }
        }
        printf("\n");
    }

protected:
    int __build()
    {
        node_t *r, *l;
        node_t *p = this->_nodes.begin(), *e = this->_nodes.end();
        for (; p != e; p++)
        {
            item_type_t type = p->type;
            if (type <= OK)
            {
                _node_stack.push(p);
                p->first.add(p->pos);
                p->last.add(p->pos);
            }
            else if (type <= QUST)
            {
                p->r = r = _node_stack.top();
                _node_stack.pop();
                _node_stack.push(p);

                p->first.merge(r->first);
                p->last.merge(r->last);

                p->nul = r->nul;
                if (type == STAR || type == QUST)
                {
                    p->nul = true;
                }
                p->charset.add(&r->charset);
            }
            else
            {
                p->r = r = _node_stack.top();
                _node_stack.pop();
                p->l = l = _node_stack.top();
                _node_stack.pop();
                _node_stack.push(p);

                p->first.merge(l->first);
                p->last.merge(r->last);

                if (type == OR || l->nul)
                {
                    p->first.merge(r->first);
                }
                if (type == OR || r->nul)
                {
                    p->last.merge(l->last);
                }
                if ((type == CAT && r->nul && l->nul) || (type == OR && (r->nul || l->nul)))
                {
                    p->nul = true;
                }
                p->charset.add(&l->charset);
                p->charset.add(&r->charset);
            }
        }
        root = _node_stack.top();
        return 1;    
    }

    void __get_follow_cat(node_t* p)
    {
        int pos = p->l->last.first();
        int lastpos = p->l->last.last();
        while (pos <= lastpos)
        {
            if (p->l->last.has(pos))
            {
                this->node_at_pos(pos)->follow.merge(p->r->first);
            }
            pos++;
        }
    }

    void __get_follow_star(node_t* p)
    {
        int pos = p->r->last.first();
        int lastpos = p->r->last.last();
        while (pos <= lastpos)
        {
            if (p->r->last.has(pos))
            {
                this->node_at_pos(pos)->follow.merge(p->r->first);
            }
            pos++;
        }
    }

    void __get_follow_plus(node_t* p)
    {
        __get_follow_star(p);
    }

    void __get_follow(node_t* p)
    {
        if (p)
        {
            __get_follow(p->l);
            __get_follow(p->r);

            switch (p->type)
            {
                case STAR:
                    __get_follow_star(p);
                    break;
                case PLUS:
                    __get_follow_plus(p);
                    break;
                case CAT:
                    __get_follow_cat(p);
                    break;
            }
        }
    }
};

struct _delta_t
{
    input_t input;
    id_t id;

    _delta_t(input_t x, id_t y): input(x), id(y)
    {}
};

template <class MEM> struct _state_t: public _set_t<MEM>
{
    array_t<_delta_t, MEM> deltas;
    id_t id;
    bool ok;
    input_t min;
    input_t max;

    _state_t(size_t n = 0): id(n), ok(false), min(0xff), max(0)
    {}

    static _state_t* create(size_t n = 0)
    {
        _state_t* p = (_state_t*)MEM::allocate(sizeof(_state_t));
        ::new (p) _state_t(n);
        return p;
    }

    static void release(_state_t* p)
    {
        if (p)
        {
            p->~_state_t();
            MEM::deallocate(p);    
        }
    }

    void add_delta(input_t input, id_t id)
    {
        deltas.append(_delta_t(input, id));
        if (input > max)
        {
            max = input;
        }
        if (input < min)
        {
            min = input;
        }
    }
};

struct _result_t
{
    const unsigned char* p;
    const unsigned char* e;

    _result_t(const unsigned char* x, const unsigned char* y): p(x), e(y)
    {}

    std::string to_string()
    {
        return std::string((const char*)p, (const char*)e);
    }

    std::string& to_string(std::string& str)
    {
        str.assign((const char*)p, (const char*)e);
        return str;
    }
};

template <class MEM> struct _results_t: public array_t<_result_t, MEM>
{
    const unsigned char* next_begin() const
    {
        return this->last().e;
    }
};

template <class MEM> struct _state_table_t
{
    struct _item_t
    {
        unsigned short* ptr;
        unsigned char min;
        unsigned char max;

        _item_t(unsigned short* _ptr_, unsigned char _min_, unsigned char _max_):
            ptr(_ptr_), min(_min_), max(_max_)
        {}
    };

    typedef _state_t<MEM> state_t;
    typedef array_t<_item_t, MEM> table_t;

    table_t _table;

    _state_table_t()
    {}

    ~_state_table_t()
    {
        for (_item_t* i = _table.begin(); i != _table.end(); i++)
        {
            MEM::deallocate(i->ptr);
        }
    }

    void add(state_t* s)
    {
        unsigned char min = s->min;
        unsigned char max = s->max;
        unsigned short len = max - min + 1;
        unsigned short* ptr = (unsigned short*)MEM::allocate(len * sizeof(unsigned short));
        memset(ptr, 0xff, len);
        for (_delta_t* p = s->deltas.begin(); p != s->deltas.end(); p++)
        {
            ptr[p->input - min] = p->id;
        }
        _table.append(_item_t(ptr, min, max));
    }

    id_t trans(id_t id, input_t i)
    {
        _item_t* p = _table._dat + id;
        if (i >= p->min && i <= p->max)
        {
            return p->ptr[i - p->min];
        }
        return 0xff;
    }

    static _state_table_t* create()
    {
        _state_table_t* p = (_state_table_t*)MEM::allocate(sizeof(_state_table_t));
        ::new (p) _state_table_t();
        return p;
    }

    static void release(_state_table_t* p)
    {
        MEM::deallocate(p);
    }
};

template <class MEM> struct re_t
{
    typedef _tree_t<MEM> tree_t;
    typedef _state_t<MEM> state_t;
    typedef array_t<state_t*, MEM> state_array_t;
    typedef typename state_array_t::iterator_t state_iterator_t;
    typedef stack_t<state_t*, MEM> state_stack_t;
    typedef array_t<_delta_t, MEM> delta_array_t;
    typedef _results_t<MEM> results_t;
    typedef const unsigned char* (re_t::*match_fun_ptr)(const unsigned char*, const unsigned char*);
    typedef int (re_t::*search_fun_ptr)(const unsigned char*, const unsigned char*, results_t&, long);
    typedef const unsigned char* (re_t::*search_prefix_fun_ptr)(const unsigned char*, const unsigned char*);
    typedef typename _results_t<MEM>::iterator_t result_iterator_t;
    typedef typename _results_t<MEM>::const_iterator_t const_result_iterator_t;

    char* _exp;
    size_t _explen;
    size_t _num;
    bool* _accept;
    unsigned char* _trans;
    unsigned char* _prefix;
    size_t _prelen;
    unsigned char* _pretable;
    match_fun_ptr _match_fun;
    search_fun_ptr _search_fun;
    search_prefix_fun_ptr _search_prefix_fun;

    bool _ignorecase, _matchword, _badcharopt;
    bool matchbegin, matchend, matchmin;
    unsigned char _cmin, _cmax;

    re_t()
    {
        memset(this, 0, sizeof(re_t));
    }

    re_t(const char* exp, long flag = 0)
    {
        memset(this, 0, sizeof(re_t));
        if (exp)
        {
            __set_exp(exp, strlen(exp), flag);
        }
    }

    re_t(const char* exp, size_t explen, long flag = 0)
    {
        memset(this, 0, sizeof(re_t));
        if (exp)
        {
            __set_exp(exp, explen, flag);
        }
    }

    re_t(const re_t& another)  // 待实现
    {}

    ~re_t()
    {
        release();
    }

    error_t compile()
    {
        tree_t tree(_exp, _explen);
        error_t ret = tree.build();

        if (ret == NO_ERR)
        {
            if (_ignorecase)
            {
                tree.ignore_case();
            }
    #ifdef _RE_DEBUG
            tree.debug();
    #endif
            matchbegin = tree.matchbegin;
            matchend = tree.matchend;

            state_t *start, *curstate;
            state_stack_t stack;
            state_array_t states;
            state_array_t newstates;
            delta_array_t newdelta;
    
            start = state_t::create();
            start->merge(tree.root->first);
            stack.push(start);
    
            _cmin = tree.root->charset.first();
            _cmax = tree.root->charset.last();
    
            while (!stack.empty())
            {
                states.append(stack.top());
                stack.pop();
                curstate = states.last();
    
                /*
                 * 对curstate输入范围为[cmin,cmax]的字符，
                 * 将新生成的状态记录在newstates中，状态转移情况记录在newdelta中
                 */
                __generate_new_state(curstate, tree, newstates, newdelta);
                __merge_and_add(curstate, newstates, newdelta, states, stack);
            }
            _num++;
            __set_acceptions(states, tree);
            __sort_states(states);
            if (__deal_with_prefix(states))
            {
                _num = 0;  //??有用咩
                _match_fun = &re_t::__match_sample;
                _search_fun = &re_t::__search_sample;
            }
            else
            {
                if (_num <= THR)
                {
                    __generate_DFA_bthr(states, tree);
                    if (_matchword)
                    {
                        _match_fun = &re_t::__match_word_bthr;
                        _search_fun = &re_t::__search_word_bthr;
                    }
                    else
                    {
                        _match_fun = &re_t::__match_bthr;
                        _search_fun = _prefix?
                            &re_t:: __search_with_prefix_bthr:
                            &re_t::__search_without_prefix_bthr;
                    }
                }
                else
                {
                    __generate_DFA_athr(states, tree);
                    if (_matchword)
                    {
                        _match_fun = &re_t::__match_word_athr;
                        _search_fun = &re_t::__search_word_athr;
                    }
                    else
                    {
                        _match_fun = &re_t::__match_athr;
                        _search_fun = _prefix?
                            &re_t:: __search_with_prefix_athr:
                            &re_t::__search_without_prefix_athr;
                    }
                }
            }

            for (state_iterator_t i = states.begin(); i != states.end(); i++)
            {
    #ifdef _RE_DEBUG
                __debug_state(i);
    #endif
                state_t::release(*i);
            }
        }
        return ret;
    }

    error_t compile(const char* exp, size_t explen, long flag = 0)
    {
        release();
        if (exp)
        {
            __set_exp(exp, _explen = explen, flag);
            return compile();
        }
        return ERR_EMP;
    }

    error_t compile(const char* exp, long flag = 0)
    {
        return compile(exp, strlen(exp), flag);
    }

    const unsigned char* match(const unsigned char* p, const unsigned char* e)
    {
        return (this->*_match_fun)(p, e);
    }

    const char* match(const char* p, const char* e)
    {
        return (const char*)(this->*_match_fun)((const unsigned char*)p, (const unsigned char*)e);
    }

    const char* match(const char* p)
    {
        return (const char*)(this->*_match_fun)((const unsigned char*)p, (const unsigned char*)(p + strlen(p)));
    }

    size_t search(const unsigned char* p, const unsigned char* e, results_t& results, long n = SEARCH_FIRST)
    {
        size_t s0 = results.size();
        __search(p, e, results, n);
        return results.size() - s0;
    }

    size_t search(const char* p, const char* e, results_t& results, long n = SEARCH_FIRST)
    {
        return search((const unsigned char*)p, (const unsigned char*)e, results, n);
    }

    size_t search(const char* p, results_t& results, long n = SEARCH_FIRST)
    {
        return search((const unsigned char*)p, (const unsigned char*)(p + strlen(p)), results, n);
    }

    #define __memchr(p, ch, len) (const unsigned char*)memchr((p), (ch), (len))

    static __must_inline(const unsigned char*) __find_linesep(const unsigned char* p, const unsigned char* e, unsigned char c0, unsigned char c1)
    {
        const unsigned char* q;
        while ((q = __memchr(p, c0, e - p - 1)))
        {
            if (*(q + 1) == c1)
            {
                return q;
            }
            p = ++q;
        }
        return NULL;
    }

    size_t search_lines(const unsigned char* p, const unsigned char* e, results_t& results, long n = SEARCH_FIRST, const char* sep = "\n")
    {
        size_t s0 = results.size(), s = s0;
        const unsigned char* q;
        if (__exp1(!*(sep + 1)))
        {
            while (p < e && (q = __memchr(p, *sep, e - p)))
            {
                if (__search(p, q, results, n) == RESULTS_ENOUGH) return n;
                p = ++q;
                n -= results.size() - s;
                s = results.size();
            }
        }
        else if (!*(sep + 2))
        {
            char c0 = *(sep + 0);
            char c1 = *(sep + 1);
            while (p < e && (q = __find_linesep(p, e, c0, c1)))
            {
                if (__search(p, q, results, n) == RESULTS_ENOUGH) return n;
                p = q + 2;
                n -= results.size() - s;
                s = results.size();
            }
        }
        else
        {
            // ...
        }
        __search(p, e, results, n);
        return results.size() - s0;
    }

    size_t search_lines(const char* p, const char* e, results_t& results, long n = SEARCH_FIRST, const char* sep = "\n")
    {
        return search_lines((const unsigned char*)p, (const unsigned char*)e, results, n, sep);
    }

    size_t search_lines(const char* p, results_t& results, long n = SEARCH_FIRST, const char* sep = "\n")
    {
        return search_lines((const unsigned char*)p, (const unsigned char*)(p + strlen(p)), results, n, sep);
    }

    bool accept_empty() const
    {
        return _accept[0];
    }

    void release()
    {
        MEM::deallocate(_exp);
        MEM::deallocate(_prefix);
        MEM::deallocate(_pretable);
        MEM::deallocate(_accept);
        MEM::deallocate(_trans);
        memset(this, 0, sizeof(re_t));
    }

    void __set_exp(const char* exp, size_t explen, long flag)
    {
        _exp = (char*)MEM::allocate(_explen = explen);
        memcpy(_exp, exp, _explen);
        matchbegin = (flag & MATCH_BEGIN) != 0;
        matchend = (flag & MATCH_END) != 0;
        matchmin =  (flag & MATCH_MIN) != 0;
        _matchword = (flag & MATCH_WORD) != 0;
        _ignorecase = (flag & MATCH_ICASE) != 0; 
        _badcharopt = (flag & BAD_CHAR_OPT) != 0;
    }

    void __generate_new_state(state_t* curstate, 
                              tree_t& tree,
                              state_array_t& newstates,
                              delta_array_t& newdelta)
    {
        state_t* newstat;
        state_iterator_t found;
        int temp_statnum = 0;

        newdelta.clear();
        newstates.clear();

        for (int c = _cmin; c <= _cmax; c++)
        {
            if ((newstat = __input_one_char(curstate, c, tree)))
            {
                if ((found = __find_state(newstates.begin(), newstates.end(), newstat)))
                {
                    newdelta.append(_delta_t(c, (*found)->id));
                    state_t::release(newstat);
                }
                else
                {
                    newstat->id = ++temp_statnum;
                    newdelta.append(_delta_t(c, newstat->id));
                    newstates.append(newstat);
                }
            }
        }
    }

    void __merge_and_add(state_t* curstate,
                         state_array_t& newstates,
                         delta_array_t& newdelta,
                         state_array_t& states,
                         state_stack_t& stack)
    {
        state_iterator_t found, i;
        for (i = newstates.begin(); i != newstates.end(); i++)
        {
            if ((found = __find_state(states.begin(), states.end(), *i)))
            {
                for (_delta_t* p = newdelta.begin(); p != newdelta.end(); p++)
                {
                    if (p->id == (*i)->id)
                    {
                        curstate->add_delta(p->input, (*found)->id);
                    }
                }
                state_t::release(*i);
            }
            else
            {
                ++_num;
                for (_delta_t* p = newdelta.begin(); p != newdelta.end(); p++)
                {
                    if (p->id == (*i)->id)
                    {
                        curstate->add_delta(p->input, _num);
                    }
                }
                (*i)->id = _num;
                stack.push(*i);
            }
        }
    }

    void __set_acceptions(state_array_t& states, const tree_t& tree)
    {
        state_iterator_t i = states.begin();
        while (i != states.end())
        {
            if ((*i)->has(tree.reception_pos()))
            {
                (*i)->ok = true;
            }
            i++;
        }
    }

    void __sort_states(state_array_t& states)
    {
        if (states.size() > 1)
        {
            typename state_array_t::value_t tmp;
            state_iterator_t i = states.begin() + 1, j;
            while (i != states.end())
            {
                tmp = *i;
                for (j = i++; j > states.begin() && (*(j - 1))->id > tmp->id; j--)
                {
                    *j = *(j - 1);
                }
                *j = tmp;
            }
        }
    }

    void __generate_DFA_bthr(state_array_t& states, tree_t& tree)
    {
        size_t transize = _num << 8;
        _trans = (unsigned char*)MEM::allocate(transize);
        memset(_trans, 0xff, transize);
        _accept = (bool*)MEM::allocate(_num * sizeof(bool));
        for (state_iterator_t i = states.begin(); i != states.end(); i++)
        {
            id_t id = (*i)->id;
            _accept[id] = (*i)->ok;
            for (_delta_t* j = (*i)->deltas.begin(); j != (*i)->deltas.end(); j++)
            {
                _trans[(id << 8) + j->input] = (unsigned char)(j->id);
            }
        }
        if (_badcharopt)
        {
            unsigned char c, *p = _trans + 256;
            while (p < _trans + transize)
            {
                memset(p, 0xfe, _cmin);
                for (c = _cmin + 1, p += c; c < _cmax; c++, p++)
                {
                    if (!tree.root->charset.has(c))
                    {
                        *p = 0xfe;
                    }
                }
                memset(++p, 0xfe, 255 - _cmax);
                p += 255 - _cmax;
            }
        }
    }

    void __generate_DFA_athr(state_array_t& states, tree_t& tree)
    {
        // ...
    }

    state_t* __input_one_char(state_t* stat, unsigned char c, tree_t& tree)
    {
        state_t* newstat = NULL;
        int pos = stat->first();
        int lastpos = stat->last();
        while (pos <= lastpos)
        {
            if (stat->has(pos) && tree.node_at_pos(pos)->has(c))
            {
                if (__exp0(newstat == NULL))    // 改成newstat != NULL的形式??
                {
                    newstat = state_t::create();
                }
                newstat->merge(tree.node_at_pos(pos)->follow);
            }
            pos++;
        }
        return newstat;
    }

    state_iterator_t __find_state(state_iterator_t i, state_iterator_t e, state_t* aim)
    {
        while (i != e && !(*i)->equal(aim))   // 代码有待推敲
        {
            i++;
        }
        return cond_ptr(i, i != e);
    }

    bool __deal_with_prefix(state_array_t& states)
    {
        if (__exp0(_matchword))
        {
            _pretable = (unsigned char*)MEM::allocate(256);
            memset(_pretable, 0, 256);
            const unsigned char* p = (const unsigned char*)BOUND_CHARS;
            while (*p)
            {
                _pretable[*p++] = 1;
            }
            _pretable[0] = 1;  // '\0' is also a word boundary
            return false;
        }

        bool sample = false;
        unsigned char buf[256];
        id_t id = 0;
        while (id < _num)
        {
            state_t* s = states[id];
            if (s->deltas.size() != 1 || s->deltas.first().id != id + 1 || s->ok)
            {
                break;
            }
            buf[_prelen++] = s->deltas.first().input;
            id++;
        }
        if (id == _num)
        {
            sample = true;
        }
        if (_prelen)
        {
            _prefix = (unsigned char*)MEM::allocate(_prelen);
            memcpy(_prefix, buf, _prelen);
            switch (_prelen)
            {
                case 0: break;
                case 1:  _search_prefix_fun = &re_t::__sp1;  break;
                case 2:  _search_prefix_fun = &re_t::__sp2;  break;
                case 3:  _search_prefix_fun = &re_t::__sp3;  break;
                case 4:  _search_prefix_fun = &re_t::__sp4;  break;
                case 5:  _search_prefix_fun = &re_t::__sp5;  break;
                case 6:  _search_prefix_fun = &re_t::__sp6;  break;
                case 7:  _search_prefix_fun = &re_t::__sp7;  break;
                case 8:  _search_prefix_fun = &re_t::__sp8;  break;
                case 9:  _search_prefix_fun = &re_t::__sp9;  break;
                case 10: _search_prefix_fun = &re_t::__sp10; break;
                case 11: _search_prefix_fun = &re_t::__sp11; break;
                case 12: _search_prefix_fun = &re_t::__sp12; break;
                case 13: _search_prefix_fun = &re_t::__sp13; break;
                case 14: _search_prefix_fun = &re_t::__sp14; break;
                case 15: _search_prefix_fun = &re_t::__sp15; break;
                case 16: _search_prefix_fun = &re_t::__sp16; break;
                default: _search_prefix_fun = &re_t::__spx;  break;
            }
            if (_prelen >= 4)
            {
                unsigned char *p, *e;
                _pretable = (unsigned char*)MEM::allocate(256);
                memset(_pretable, _prelen + 1, 256);
                for (p = _prefix, e = _prefix + _prelen; p != e; p++)
                {
                    _pretable[*p] = _prelen - (p - _prefix); 
                } 
            }
        }
        return sample;
    }

    void __debug_state(state_iterator_t i) const
    {
        printf("%u\t", (*i)->id);
        for (int j = 0; j <= (*i)->last(); j++)
        {
            if ((*i)->has(j))
            {
                printf("%d ", j);
            }
        }
        printf("\t\t");
        for (_delta_t* j = (*i)->deltas.begin(); j != (*i)->deltas.end(); j++)
        {
            if (j->input >= 33 && j->input <= 126)
            {
                printf("(%c, %d) ", j->input, j->id);
            }
            else
            {
                printf("(\\x%x, %d) ", j->input, j->id);
            }
        }
        if ((*i)->ok)
        {
            printf("OK");
        }
        printf("\n");
    }

    const unsigned char* __match_bthr(const unsigned char* p, const unsigned char* e)
    {
        size_t s = 0, a = 0;
        const unsigned char* r = NULL;
        while (p < e && !(a && matchmin))
        {
            if ((s = _trans[(s << 8) + *p++]) >= 0xfe)
            {
                break;
            }
            if ((a = _accept[s] && (!matchend || (matchend && p == e))))
            {
                r = p;
            }
        }
        return r;
    }

    const unsigned char* __match_word_bthr(const unsigned char* p, const unsigned char* e)
    {
        size_t s = 0, a = 0;
        const unsigned char* r = NULL;
        while (p < e && !(a && matchmin) && (s = _trans[(s << 8) + *p++]) < 0xfe)
        {
            if (__exp1(!matchend))
            {
                a = _accept[s] && (p == e || _pretable[*p]);
            }
            else
            {
                a = _accept[s] && p == e;
            }
            if (__exp1(a))
            {
                r = p;
            }
        }
        return r;
    }

    const unsigned char* __match_athr(const unsigned char* p, const unsigned char* e)
    {
        return NULL;
    }

    const unsigned char* __match_word_athr(const unsigned char* p, const unsigned char* e)
    {
        return NULL;
    }

    const unsigned char* __match_sample(const unsigned char* P, const unsigned char* E)
    {
        if (ptrdiff_t(E - P) >= ptrdiff_t(_prelen))
        {
            const unsigned short* p = (const unsigned short*)(P);
            const unsigned short* q = (const unsigned short*)(_prefix);
            const unsigned short* r = (const unsigned short*)(_prefix + _prelen - (_prelen & 1));
            while (q != r && *p == *q)
            {
                p++;
                q++;
            }
            if (q != r || (_prelen & 1 && *(const unsigned char*)r != *(const unsigned char*)p))
            {
                return NULL;
            }
            return (const unsigned char*)p;
        }
        return NULL;
    }

    int __search(const unsigned char* p, const unsigned char* e, results_t& results, long n)
    {
        if (__exp1(p < e))
        {
            if (__exp1(!matchbegin))
            {
                return (this->*_search_fun)(p, e, results, n);
            }
            else
            {
                const unsigned char* r = (this->*_match_fun)(p, e);
                if (r)
                {
                    results.append(_result_t(p, r));
                    if (!--n)
                    {
                        return RESULTS_ENOUGH;
                    }
                }
            }
        }
        return RESULTS_NOT_ENOUGH;
    }

    int __search_with_prefix_bthr(const unsigned char* p, const unsigned char* e, results_t& results, long n)
    {
        size_t s, a;
        const unsigned char *x, *y;
        while ((p = (this->*_search_prefix_fun)(p, e)))
        {
            s = _prelen;
            a = _accept[s];
            x = p + _prelen;
            y = cond_ptr(x, a);
            while (x != e && !(a && matchmin))
            {
                if ((s = _trans[(s << 8) + *x++]) >= 0xfe)
                {
                    break;
                }
                if ((a = _accept[s] && (!matchend || (matchend && x == e))))
                {
                    y = x;
                }
            }
            if (y)
            {
                results.append(_result_t(p, y));
                if (!--n)
                {
                    return RESULTS_ENOUGH;
                }
                p = y;
            }
            else
            {
                p++; 
            }
            if (s == 0xfe)
            {
                p = x;
            }
        }
        return RESULTS_NOT_ENOUGH;
    }

    int __search_without_prefix_bthr(const unsigned char* p, const unsigned char* e, results_t& results, long n)
    {
        size_t s, a;
        const unsigned char *x, *y;
        while (p != e)
        {
            while (p != e && _trans[*p] == 0xff)  // 0xfe 不必出现在0状态中
            {
                p++;
            }
            if (p != e)
            {
                s = _trans[*p];
                a = _accept[s];
                x = p + 1;
                y = cond_ptr(x, a);
                while (x != e && !(a && matchmin))
                {
                    if ((s = _trans[(s << 8) + *x++]) >= 0xfe)  // 将此逻辑分析放到while的逻辑表达式中
                    {
                        break;
                    }
                    if ((a = _accept[s] && (!matchend || (matchend && x == e))))
                    {
                        y = x;
                    }
                }
                if (y)
                {
                    results.append(_result_t(p, y));
                    if (!--n)
                    {
                        return RESULTS_ENOUGH;
                    }
                    p = y;
                }
                else
                {
                    p++;
                }
                if (s == 0xfe)
                {
                    p = x;
                }
            }
        }
        return RESULTS_NOT_ENOUGH;
    }

    int __search_word_bthr(const unsigned char* p, const unsigned char* e, results_t& results, long n)
    {
        const unsigned char *q;
        while (p != e)
        {
            while (p != e && _pretable[*p])
            {
                p++;
            }
            if ((q = __match_word_bthr(p, e)))
            {
                results.append(_result_t(p, q));
                if (!--n)
                {
                    return RESULTS_ENOUGH;
                }
                p = q;   // +1 ??
            }
            else while (p != e && !_pretable[*p])
            {
                p++;
            }
        }
        return RESULTS_NOT_ENOUGH;
    }

    int __search_with_prefix_athr(const unsigned char* p, const unsigned char* e, results_t& results, long n)
    {
        return RESULTS_NOT_ENOUGH;
    }

    int __search_without_prefix_athr(const unsigned char* p, const unsigned char* e, results_t& results, long n)
    {
        return RESULTS_NOT_ENOUGH;
    }

    int __search_word_athr(const unsigned char* p, const unsigned char* e, results_t& results, long n)
    {
        return RESULTS_NOT_ENOUGH;
    }

    int __search_sample(const unsigned char* p, const unsigned char* e, results_t& results, long n)
    {
        const unsigned char* q;
        while ((p = (this->*_search_prefix_fun)(p, e)) && --n)
        {
            q = p + _prelen;
            results.append(_result_t(p, q));
            p = q;
        }
        return !n;
    }

    const unsigned char* __sp1(const unsigned char* P, const unsigned char* E)
    {
        return __memchr(P, *_prefix, E - P);
    }

    const unsigned char* __sp2(const unsigned char* P, const unsigned char* E)
    {
        E--;
        const unsigned char* p = P;
        ptrdiff_t len = E - p;
        while (len > 0 && (p = __memchr(p, *_prefix, len)))
        {
            if (*(const unsigned short*)p == *(const unsigned short*)_prefix) return p;
            len = E - ++p;
        }
        return NULL;
    }

    const unsigned char* __sp3(const unsigned char* P, const unsigned char* E)
    {
        E -= 2;
        const unsigned char* p = P;
        ptrdiff_t len = E - p;
        unsigned short v = *(unsigned short*)(_prefix + 1);
        while (len > 0 && (p = __memchr(p, *_prefix, len)))
        {
            if (*(const unsigned short*)(p + 1) == v) return p;
            len = E - ++p;
        }
        return NULL;
    }

    #define __SUNDAY_SEARCH(fun_name, __mem_neq__) \
    const unsigned char* fun_name (const unsigned char* P, const unsigned char* E)\
    {\
        const unsigned char *p = P, *e = P + _prelen;\
        while (__exp1(e < E))\
        {\
            if (__exp1(__mem_neq__))\
            {\
                p += _pretable[*e]; e += _pretable[*e]; continue;\
            }\
            return p;\
        }\
        if (__exp0(e == E && !(__mem_neq__))) return p;\
        return NULL;\
    }
    __SUNDAY_SEARCH (__sp4,  *(U32*)p != *(U32*)_prefix)
    __SUNDAY_SEARCH (__sp5,  *(U32*)p != *(U32*)_prefix || *(p + 4) != *(_prefix + 4))
    __SUNDAY_SEARCH (__sp6,  *(U32*)p != *(U32*)_prefix || *(U16*)(p + 4) != *(U16*)(_prefix + 4))
    __SUNDAY_SEARCH (__sp7,  *(U32*)p != *(U32*)_prefix || *(U16*)(p + 4) != *(U16*)(_prefix + 4) || *(p + 6) != *(_prefix + 6))
    __SUNDAY_SEARCH (__sp8,  *(U64*)p != *(U64*)_prefix)
    __SUNDAY_SEARCH (__sp9,  *(U64*)p != *(U64*)_prefix || *(p + 8) != *(_prefix + 8))
    __SUNDAY_SEARCH (__sp10, *(U64*)p != *(U64*)_prefix || *(U16*)(p + 8) != *(U16*)(_prefix + 8))
    __SUNDAY_SEARCH (__sp11, *(U64*)p != *(U64*)_prefix || *(U16*)(p + 8) != *(U16*)(_prefix + 8) || *(p + 10) != *(_prefix + 10))
    __SUNDAY_SEARCH (__sp12, *(U64*)p != *(U64*)_prefix || *(U32*)(p + 8) != *(U32*)(_prefix + 8))
    __SUNDAY_SEARCH (__sp13, *(U64*)p != *(U64*)_prefix || *(U32*)(p + 8) != *(U32*)(_prefix + 8) || *(p + 12) != *(_prefix + 12))
    __SUNDAY_SEARCH (__sp14, *(U64*)p != *(U64*)_prefix || *(U32*)(p + 8) != *(U32*)(_prefix + 8) || *(U16*)(p + 12) != *(U16*)(_prefix + 12))
    __SUNDAY_SEARCH (__sp15, *(U64*)p != *(U64*)_prefix || *(U32*)(p + 8) != *(U32*)(_prefix + 8) || *(U16*)(p + 12) != *(U16*)(_prefix + 12) || *(p + 14) != *(_prefix + 14))
    __SUNDAY_SEARCH (__sp16, *(U64*)p != *(U64*)_prefix || *(U64*)(p + 8) != *(U64*)(_prefix + 8))
    #undef __SUNDAY_SEARCH

    __must_inline(bool) __spx_mem_neq(const unsigned char* p)
    {
        const unsigned char* q = _prefix + 16;
        const unsigned char* r = _prefix + _prelen;
        while (q != r && *q == *p)
        {
            p++;
            q++;
        }
        return q != r;
    }

    const unsigned char* __spx(const unsigned char* P, const unsigned char* E)
    {
        #define __mem_neq__ *(U64*)p != *(U64*)_prefix || *(U64*)(p + 8) != *(U64*)(_prefix + 8) || __spx_mem_neq(p + 16)
        const unsigned char *p = P, *e = P + _prelen;
        while (e < E)
        {
            if (__exp1(__mem_neq__))
            {
                p += _pretable[*e]; e += _pretable[*e]; continue;
            }
            return p;
        }
        if (__exp0(e == E && !(__mem_neq__)))
        {
            return p;
        }
        return NULL;
        #undef __mem_neq__
    }
};

typedef re_t<default_memory_allocator_t> myre_t;
typedef _results_t<default_memory_allocator_t> results_t;
typedef _result_t result_t;

// examples
#define PAT_IPV4    "(\\d\\d?\\d?\\.){3}\\d\\d?\\d?"
#define PAT_EMAIL   "\\w+@(\\w+\\.)+\\w+"
#define PAT_HEX     "0x[0-9a-fA-F]+"

} // namespace end

