#pragma once
#include <iostream>
#include <string>

struct Object {
    int val;
    std::string str;

    Object() = delete;
    Object(int v, std::string s) : val(v), str(s) {};
    void print() {
        std::cout << this->val << " " << this->str << std::endl;
    }

    bool operator==(const Object& other) const {
        return this->val == other.val && this->str == other.str;
    }
    friend std::ostream& operator<<(std::ostream& os, const Object& obj) {
        return os << obj.val << " " << obj.str << "\n";
    }
};



struct SimpleObj {
    int val;
    std::string str;

    SimpleObj() = default;
    SimpleObj(int v, std::string s) : val(v), str(s) {};
    friend std::ostream& operator<<(std::ostream& os, const SimpleObj& obj) {
        return os << obj.val << " " << obj.str << "\n";
    }

    bool operator==(const SimpleObj& other) const {
        return this->val == other.val && this->str == other.str;
    }
};

size_t get_timestamp_us() {
    return 1;
}