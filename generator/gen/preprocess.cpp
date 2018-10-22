#include "common.hpp"

#include <iostream>
#include <unistd.h>

int main(int argc, char ** argv)
{
  std::cout << "Hello world!" << std::endl;
  for (int i = 0; i < argc; ++i)
    std::cout << argv[i] << "\n";


}
