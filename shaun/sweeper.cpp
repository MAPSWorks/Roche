#include "sweeper.hpp"
#include <sstream>
#include <string>

namespace shaun
{

sweeper::sweeper(shaun * root)
{
    root_ = root;
    current_ = root_;
    current_name_ = "root";
    next_ = 0;
}

sweeper::sweeper(const sweeper& swp)
{
    current_name_ = swp.current_name_;
    current_      = root_ = swp.current_;
    next_ = 0;
}

sweeper::~sweeper()
{
}

shaun * sweeper::compute_path(const std::string& path)
{
    std::string::const_iterator first, second;
    shaun * ret = current_;

    first = second = path.begin();
    while (first != path.end())
    {
        if (second == path.end())
        {
            if (ret->type() != Type::object)
                throw ("expected object but " + current_name_ + " has type " + type_to_string(ret->type()));

            current_name_.assign(first, second);
            ret = static_cast<object*>(ret)->get_variable(current_name_);

            break;
        }

        if (*(second) == ':' || *(second) == '[')
        {
            if (ret->type() != Type::object)
                throw ("expected object but " + current_name_ + " has type " + type_to_string(ret->type()));

            current_name_.assign(first, second);

            ret = static_cast<object*>(ret)->get_variable(current_name_);

            if (*(second) == '[')
            {
                if (ret->type() != Type::list)
                    throw ("expected list but " + current_name_ + " has type " + type_to_string(ret->type()));

                first = ++second;
                while (*second != ']') ++second;
                

                int index = std::stoi(std::string(first, second));

                try
                {
                    ret = static_cast<list*>(ret)->elements().at(index).get();
                }
                catch (...)
                {
                    throw ("index out of range");
                }

                ++second;
            }

            first = ++second;
        }
        else
            ++second;
    }

    return ret;
}

sweeper& sweeper::operator[](size_t i)
{
    if (current_->type() != Type::list)
        throw (current_name_ + " is not a list");

    try
    {
        next_.reset(new sweeper(static_cast<list*>(current_)->elements().at(i).get()));
    }
    catch (...)
    {
        throw ("index out of range");
    }
    
    return *next_;
}

sweeper& sweeper::get(const std::string& path)
{
    next_.reset(new sweeper(compute_path(path)));
    return *next_;
}

sweeper& sweeper::operator()(const std::string& path)
{
    return get(path);
}

#define VALUE(TYPE) template<>\
    TYPE& sweeper::value() const    \
    {                      \
        if (current_->type() != Type::TYPE)\
            throw ("expected " + type_to_string(Type::TYPE) + " but " + current_name_ + " has type " + type_to_string(current_->type()));\
                           \
        return *static_cast<TYPE*>(current_);\
    }

VALUE(null)
VALUE(boolean)
VALUE(number)
VALUE(string)
VALUE(object)
VALUE(list)

Type sweeper::type() const
{
    return current_->type();
}

bool sweeper::is_null() const
{
  return current_->is_null();
}

} // namespace