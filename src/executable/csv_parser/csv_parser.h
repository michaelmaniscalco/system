#pragma once

#include <iterator>
#include <vector>
#include <functional>
#include <cstring>


static std::tuple<char const *, char const *> parse_field(char const * begin, char const * end, char delimiter)
{
    auto cur = begin;
    while ((cur < end) && (*cur != delimiter))
    {
        if (*cur == '"')
        {
            while ((++cur < end) && (*cur != '"'))
                ;
            if (cur >= end)
                throw std::runtime_error("unmatched quotes found");
        }
        ++cur;
    }
    return {begin, cur};
}


class field_view
{
public:

    field_view():begin_(nullptr), end_(nullptr){}

    field_view(char const * begin, char const * end):begin_(&*begin), end_(&*end){}

    field_view(std::tuple<char const *, char const *> range):begin_(&*std::get<0>(range)), end_(&*std::get<1>(range)){}

    std::size_t size() const {return std::distance(begin_, end_);}
    char const * begin() const {return begin_;}
    char const * end() const { return end_;}

protected:
private:
    char const * const begin_;
    char const * const end_;
}; // class field_view


class row_view
{
public:

    row_view():fields_(){}

    row_view(char const * begin, char const * end, char delimiter)
    {
        parse(begin, end, delimiter);
    }

    void parse(char const * begin, char const * end, char delimiter)
    {
        fields_.clear();
        auto cur = begin - 1;
        while (++cur < end)
            cur = fields_.emplace_back(parse_field(cur, end, delimiter)).end();
    }

    auto begin() const{return std::begin(fields_);}
    auto end() const{return std::end(fields_);}

protected:
private:
    std::vector<field_view> fields_;
}; // class row_view


class csv_parser
{
public:

    using on_header_handler = std::function<void(csv_parser const &, row_view const &)>;
    using on_row_handler = std::function<void(csv_parser const &, row_view const &)>;
    using on_end_handler = std::function<void(csv_parser const &)>;

    struct configuration_type 
    {
        on_header_handler   onHeaderHandler_;
        on_row_handler      onRowHandler_;
        on_end_handler      onEndHandler_;
        bool                hasHeader_;
        char                delimiter_;
    }; // struct configuration_type

    csv_parser(configuration_type const & configuration):
        onHeaderHandler_((configuration.hasHeader_) ? (configuration.onHeaderHandler_ ? configuration.onHeaderHandler_ : [](auto, auto){}) : nullptr),
        onRowHandler_(configuration.onRowHandler_),
        onEndHandler_(configuration.onEndHandler_),
        delimiter_(configuration.delimiter_)
    {
    }


    ~csv_parser()
    {
        if (onEndHandler_) 
            onEndHandler_(*this);
    }
    

    char const * parse
    (
        char const * begin,
        char const * end
    )
    {
        auto cur = begin;
        while (cur < end)
        {
            auto next = (char const *)memchr(cur, 10, (end - cur));
            if (next != nullptr)
            {
                rowView_.parse(cur, next, delimiter_);
                (onHeaderHandler_) ? onHeaderHandler_(*this, rowView_) : onRowHandler_(*this, rowView_);
                onHeaderHandler_ = nullptr;
                cur = next + 1;
            }
            else
            {
                // partial row at end
                break;
            }
        }
        return end;
    }

protected:
private:

    on_header_handler           onHeaderHandler_;
    on_row_handler              onRowHandler_;
    on_end_handler              onEndHandler_;
    bool                        parseHeader_;
    char                        delimiter_;
    row_view                    rowView_;
};