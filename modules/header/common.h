#include <iostream>
#include <string>

class Object {
private:
    int val;
    std::string str;

    Object() = delete;
    Object(int v, std::string s) : val(v), str(s) {};
    void print() {
        std::cout << this->val << " " << this->str << std::endl;
    }

    friend std::ostream& operator<<(std::ostream& os, const Object& obj) {
        return os << obj.val << " " << obj.str << "\n";
    }
};



class SimpleObj {
private:
    int val;
    std::string str;
public:
    SimpleObj() = default;
    SimpleObj(int v, std::string s) : val(v), str(s) {};
    friend std::ostream& operator<<(std::ostream& os, const SimpleObj& obj) {
        return os << obj.val << " " << obj.str << "\n";
    }
};