/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include <vector>
#include <sstream>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>

#include <Wt/WApplication>

#include "Url.hpp"
#include "util.hpp"

namespace Wt {

namespace Wc {

namespace url {

Parser::Parser(WObject* parent):
    WObject(parent)
{ }

Node* Parser::parse(const std::string& path) {
    using namespace boost::algorithm;
    std::vector<std::string> parts;
    split(parts, path, is_any_of("/"), token_compress_on);
    WObject* node = this;
    BOOST_FOREACH (const std::string& part, parts) {
        if (part.empty()) {
            continue;
        }
        bool next = false;
        BOOST_FOREACH (WObject* o, node->children()) {
            if (isinstance<Node>(o) && downcast<Node*>(o)->meet(part)) {
                next = true;
                node = o;
                downcast<Node*>(o)->set_value(part);
                break;
            }
        }
        if (!next) {
            return 0;
        }
    }
    return node == this ? 0 : downcast<Node*>(node);
}

void Parser::open(const std::string& path) {
    Node* node = parse(path);
    if (node) {
        node->open();
    }
}

Node::Node(WObject* parent):
    WObject(parent)
{ }

void Node::set_value(const std::string& v, bool check) {
    if (!check || meet(v)) {
        value_ = v;
    }
}

void Node::write_to(std::ostream& path) const {
    path << value_ << '/';
}

void Node::write_all_to(std::ostream& path) const {
    const Node* node = this;
    std::vector<const Node*> nodes;
    while (node) {
        nodes.push_back(node);
        node = node->node_parent();
    }
    BOOST_REVERSE_FOREACH (const Node* node, nodes) {
        node->write_to(path);
    }
}

std::string Node::full_path() const {
    std::stringstream ss;
    ss << '/';
    write_all_to(ss);
    return ss.str();
}

Node* Node::node_parent() const {
    return isinstance<Node>(parent()) ? downcast<Node*>(parent()) : 0;
}

Parser* Node::parser() const {
    const Node* node = this;
    while (Node* parent = node->node_parent()) {
        node = parent;
    }
    WObject* parent = node->parent();
    return isinstance<Parser>(parent) ? downcast<Parser*>(parent) : 0;
}

void Node::open(bool change_path) {
    if (change_path) {
        wApp->setInternalPath(full_path());
    }
    opened_.emit();
}

PredefinedNode::PredefinedNode(const std::string& predefined, WObject* parent):
    Node(parent), predefined_(predefined) {
    set_value(predefined_);
}

bool PredefinedNode::meet(const std::string& part) const {
    return part == predefined_;
}

IntegerNode::IntegerNode(WObject* parent):
    Node(parent)
{ }

bool IntegerNode::meet(const std::string& part) const {
    try {
        boost::lexical_cast<long long>(part);
        return true;
    } catch (...) {
        return false;
    }
}

long long IntegerNode::integer() const {
    return boost::lexical_cast<long long>(value());
}

void IntegerNode::set_integer_value(long long v) {
    set_value(boost::lexical_cast<std::string>(v), false);
}

std::string IntegerNode::get_full_path(long long v) {
    set_integer_value(v);
    return full_path();
}

StringNode::StringNode(WObject* parent):
    Node(parent)
{ }

bool StringNode::meet(const std::string&) const {
    return true;
}

std::string StringNode::get_full_path(const std::string& v) {
    set_value(v);
    return full_path();
}

}

}

}
