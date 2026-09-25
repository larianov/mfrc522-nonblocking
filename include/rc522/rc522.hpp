#include "transport.hpp"
class Rc522 {
    Transport& t_;
public:
    Rc522(Transport& t) : t_(t) {}        

};