#ifndef PARSER_HPP
#define PARSER_HPP

#include <clocale>
#include <cctype>
#include <sstream>
#include <string>
#include <istream>
#include <ostream>
#include <fstream>
#include <stack>
#include <utility>
#include <iostream>
#include "shaun.hpp"
#include "exception.hpp"

namespace shaun
{

object parse(const std::string& str);
object parse(std::istream& str);
object parse_file(const std::string& str);

} // namespace

#endif
