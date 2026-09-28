#include <iostream>

#include "pricer/version.hpp"

int main() {
    std::cout << "option-pricer " << pricer::version() << '\n';
    return 0;
}
