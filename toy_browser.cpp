#include "stdafx.h"

using namespace std;

class html_element_node;

class html_node
{
public:
    html_node *parent_node_;

    html_node(html_node *parent_node)
        : parent_node_(parent_node)
    {
    }

    html_element_node * parent_element();

    virtual void print(ostream &o) = 0;

    virtual ~html_node()
    {
    }
};

class html_element_node : public html_node
{
public:
    string element_name_;
    vector<unique_ptr<html_node>> children_;
    map<string, string> attributes_;

    html_element_node(html_node *parent_node, string element_name)
        : html_node(parent_node), element_name_(move(element_name))
    {
    }

    string & operator[](const string &name)
    {
        return attributes_[name];
    }

    void print(ostream &o) override
    {
        o << '<' << element_name_;

        for (auto &&attr : attributes_)
            o << ' ' << attr.first << "=\"" << attr.second << '"';

        o << '>';

        for (auto &&node : children_)
            node->print(o);

        o << "</" << element_name_ << '>';
    }
};

html_element_node * html_node::parent_element()
{
    return dynamic_cast<html_element_node *>(parent_node_);
}

class html_text_node : public html_node
{
public:
    string text_;

    html_text_node(html_node *parent_node, string text)
        : html_node(parent_node), text_(move(text))
    {
    }

    void print(ostream &o) override
    {
        o << text_;
    }
};

class html_parser
{
    enum parser_state
    {
        before_element,
        before_tag_start_or_close,
        consume_whitespace_for_close_element,
        in_element_name,
        close_element_name,
        before_attribute_name,
        attribute_name,
        attribute_separator,
        before_attribute_value,
        attribute_value,
        before_element_start_close,
        in_element,
        in_element_end
    };

    parser_state state_;
    string element_name_;
    string element_text_;
    string attribute_name_;
    string attribute_value_;
    html_element_node *current_element_;

public:

    html_element_node root_;

    html_parser()
        : state_(parser_state::before_element), root_(nullptr, string())
    {
        current_element_ = &root_;
    }

    void consume(const char *data, size_t len)
    {
        for (; len; data++, len--)
        {
            switch (state_)
            {
            case parser_state::before_element:
                if (*data == '<')
                    state_ = parser_state::in_element_name;
                else
                    throw exception("unexpected input while waiting for element start");
                break;
            case parser_state::in_element_name:
                if (*data == '>' || *data == ' ')
                {
                    auto child_element = make_unique<html_element_node>(current_element_, element_name_);
                    auto child_ptr = child_element.get();
                    current_element_->children_.emplace_back(move(child_element));
                    current_element_ = child_ptr;

                    element_name_.clear();

                    if (*data == '>')
                        state_ = parser_state::in_element;
                    else
                        state_ = parser_state::before_attribute_name;
                }
                else
                    element_name_ += *data;
                break;
            case parser_state::before_attribute_name:
                if (*data == ' ')
                    continue;
                else if (*data == '>')
                    state_ = parser_state::in_element;
                else
                {
                    data--;
                    len++;
                    state_ = attribute_name;
                }
                break;
            case parser_state::attribute_name:
                if (*data == ' ')
                    state_ = parser_state::attribute_separator;
                else if (*data == '=')
                    state_ = parser_state::before_attribute_value;
                else if (*data == '>')
                {
                    (*current_element_)[attribute_name_] = attribute_name_;
                    attribute_name_.clear();

                    state_ = parser_state::in_element;
                }
                else
                    attribute_name_ += *data;
                break;
            case parser_state::attribute_separator:
                if (*data == ' ')
                    continue;
                else if (*data == '=')
                    state_ = parser_state::before_attribute_value;
                else
                    throw exception("unexpected input while waiting for attribute separator");
                break;
            case parser_state::before_attribute_value:
                if (*data == ' ')
                    continue;
                if (*data == '"' || *data == '\'')
                    state_ = parser_state::attribute_value;
                else
                    throw exception("unexpected input while waiting for attribute value");
                break;
            case parser_state::attribute_value:
                if (*data == '"' || *data == '\'')
                {
                    (*current_element_)[attribute_name_] = attribute_value_;
                    attribute_name_.clear();
                    attribute_value_.clear();

                    state_ = parser_state::before_attribute_name;
                }
                else
                    attribute_value_ += *data;
                break;
            case parser_state::in_element:
                if (*data == '<')
                {
                    if (!element_text_.empty())
                    {
                        current_element_->children_.emplace_back(make_unique<html_text_node>(current_element_, element_text_));
                        element_text_.clear();
                    }

                    state_ = parser_state::before_tag_start_or_close;
                }
                else
                    element_text_ += *data;
                break;
            case parser_state::before_tag_start_or_close:
                if (*data == '/')
                    state_ = parser_state::close_element_name;
                else
                {
                    data -= 2;
                    len += 2;
                    state_ = parser_state::before_element;
                }
                break;
            case parser_state::close_element_name:
                if (*data == '>')
                {
                    data--;
                    len++;
                    state_ = parser_state::consume_whitespace_for_close_element;
                }
                else if (*data == ' ')
                    state_ = parser_state::consume_whitespace_for_close_element;
                else
                    element_name_ += *data;
                break;
            case parser_state::consume_whitespace_for_close_element:
                if (*data == ' ')
                    continue;
                else if (*data == '>')
                {
                    if (element_name_ != current_element_->element_name_)
                        throw exception("mismatched close tag");

                    element_name_.clear();

                    current_element_ = current_element_->parent_element();

                    state_ = parser_state::in_element;
                }
                else
                    throw exception("unexpected input while reading whitespace before element close");
                break;
            }
        }
    }
};

int main()
{
    try
    {
        //string input = "<html foo='bar'></html>";
        string input = "<html x>A<div y>B</div></html>";
        //string input = "<a>x</a>";
        html_parser parser;
        parser.consume(input.c_str(), input.size());
        parser.root_.print(cerr);
    }
    catch (const exception &e)
    {
        cerr << e.what() << endl;
    }
}
