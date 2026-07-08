/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#include <stdexcept>
#include <sstream>
#include <vector>
#include <any>
#include <memory>

#include <Wt/WApplication.h>
#include <Wt/WEnvironment.h>

#include "Url.hpp"
#include "util.hpp"

namespace Wt {

namespace Wc {

namespace url {

Node::Node():
    WObject(),
    opened_(nullptr),
    slash_strategy_(DEFAULT)
{ }

Node::~Node() {
    delete opened_;
}

void Node::write_to(std::ostream& path, bool is_last) const {
    path << value();
    if (slash_strategy() == ALWAYS ||
            (slash_strategy() == IF_NOT_LAST && !is_last) ||
            (slash_strategy() == IF_HAS_CHILD && !children().empty())) {
        path << "/";
    }
}

void Node::write_all_to(std::ostream& path, Node* root) const {
    std::vector<const Node*> nodes;
    const Node* it = this;
    while (it != root && it != nullptr) {
        nodes.push_back(it);
        it = it->node_parent();
    }
    path << '/'; // even if (nodes.size() == 0)
    for (int i = nodes.size() - 1; i >= 0; i--) {
        const Node* node = nodes[i];
        node->write_to(path, i == 0);
    }
}

std::string Node::full_path() const {
    std::stringstream path;
    write_all_to(path);
    return path.str();
}

#ifdef WC_HAVE_WLINK
WLink Node::link() const {
    return WLink(WLink::InternalPath, full_path());
}
#endif

Node* Node::node_parent() const {
    return parent_node_;
}

Parser* Node::parser() const {
    const Node* it = this;
    while (it->node_parent()) {
        it = it->node_parent();
    }
    return dynamic_cast<Parser*>(const_cast<Node*>(it));
}

void Node::open(bool change_path) {
    if (change_path) {
        Wt::WApplication::instance()->setInternalPath(full_path(), true);
    } else {
        if (opened_) {
            opened_->emit();
        }
        if (Parser* p = parser()) {
            p->child_opened().emit(this);
            auto [first, last] = p->handlers_.equal_range(this);
            for (auto it = first; it != last; ++it) {
                it->second();
            }
        }
    }
}

Signal<>& Node::opened() {
    if (opened_ == nullptr) {
        opened_ = new Signal<>();
    }
    return *opened_;
}

void Node::set_value(const std::string& v, bool check) {
    if (check && !meet(v)) {
        throw std::invalid_argument("wrong format");
    }
    value_ = v;
}


PredefinedNode::PredefinedNode(const std::string& predefined):
    Node(), predefined_(predefined) {
    set_value(predefined);
}

bool PredefinedNode::meet(const std::string& part) const {
    return part == predefined_;
}


IntegerNode::IntegerNode():
    Node() {
    set_value("0");
}

bool IntegerNode::meet(const std::string& part) const {
    try {
        std::stoll(part);
        return true;
    } catch (...) {
        return false;
    }
}

long long IntegerNode::integer() const {
    return std::stoll(value());
}

void IntegerNode::set_integer_value(long long v) {
    set_value(std::to_string(v));
}

std::string IntegerNode::get_full_path(long long v) {
    set_integer_value(v);
    return full_path();
}

#ifdef WC_HAVE_WLINK
WLink IntegerNode::get_link(long long v) {
    return WLink(WLink::InternalPath, get_full_path(v));
}
#endif


StringNode::StringNode():
    Node()
{ }

bool StringNode::meet(const std::string& /* part */) const {
    return true;
}

void StringNode::set_string(const std::string& string) {
    set_value(string);
}

const std::string& StringNode::string() const {
    return value();
}

std::string StringNode::get_full_path(const std::string& v) {
    set_string(v);
    return full_path();
}

#ifdef WC_HAVE_WLINK
WLink StringNode::get_link(const std::string& v) {
    return WLink(WLink::InternalPath, get_full_path(v));
}
#endif


Parser::Parser():
    Node() {
    // Parser does not need slashes
    set_slash_strategy(IF_NOT_LAST);
}

bool Parser::meet(const std::string& part) const {
    return part == "";
}

Node* Parser::parse(const std::string& path) {
    std::vector<std::string> parts;
    std::stringstream ss(path);
    std::string item;
    while (std::getline(ss, item, '/')) {
        parts.push_back(item);
    }
    std::vector<std::string>::const_iterator part = parts.begin();
    Node* current = this;
    if (part != parts.end() && current->meet(*part)) {
        part++; // eat first part (typically, empty string before first slash)
    }
    while (current != nullptr && part != parts.end()) {
        const std::string& p = *part;
        bool found = false;
        if (p.empty()) {
            found = true;
        } else {
            for (Node* child : current->children()) {
                if (child && child->meet(p)) {
                    child->set_value(p);
                    current = child;
                    found = true;
                    break;
                }
            }
        }
        if (!found) {
            current = nullptr;
            break;
        }
        part++;
    }
    return current;
}

void Parser::open(Node* node) {
    if (node) {
        node->open(false);
    } else {
        error404().emit();
    }
}

void Parser::open(const std::string& path) {
    open(parse(path));
}

#ifdef WC_HAVE_WLINK
void Parser::open(const WLink& internal_path) {
    open(internal_path.internalPath());
}
#endif

void Parser::connect(Node* child, std::function<void()> handler) {
    handlers_.insert(std::make_pair(child, handler));
}

void Parser::disconnect(Node* child) {
    handlers_.erase(child);
}


SiteMapGenerator::SiteMapGenerator(Node* root):
    root_(root) {
    std::string schema = Wt::WApplication::instance()->environment().urlScheme();
    std::string host = Wt::WApplication::instance()->environment().hostName();
    base_loc_ = schema + "://" + host;
    default_params_.changefreq = MONTHLY;
    default_params_.priority = 0.5;
}

void SiteMapGenerator::generate(std::ostream& out) const {
    out << "<?xml version='1.0' encoding='UTF-8'?>" << std::endl;
    out << "<urlset xmlns='http://www.sitemaps.org/schemas/sitemap/0.9'>" << std::endl;
    dig_node(root(), out);
    out << "</urlset>" << std::endl;
}

void SiteMapGenerator::for_each_value(Node* /* node */,
                                      const AnyCaller& /* callback */) const
{ }

bool SiteMapGenerator::node_handler(Node* /* node */,
                                    UrlParams& /* params */) const {
    return true;
}

void SiteMapGenerator::dig_node(Node* node, std::ostream& out) const {
    bool is_predefined = dynamic_cast<PredefinedNode*>(node) != nullptr ||
                         dynamic_cast<Parser*>(node) != nullptr;
    if (is_predefined) {
        visit_node(node, out, std::string());
    } else {
        for_each_value(node, [this, node, &out](std::any v){ this->visit_node(node, out, v); });
    }
    for (Node* child : node->children()) {
        if (child) {
            dig_node(child, out);
        }
    }
}

void SiteMapGenerator::visit_node(Node* node, std::ostream& out,
                                  std::any value) const {
    std::string str_val;
    if (value.type() == typeid(int)) {
        str_val = std::to_string(std::any_cast<int>(value));
    } else if (value.type() == typeid(std::string)) {
        str_val = std::any_cast<std::string>(value);
    }
    if (str_val.empty() || node->meet(str_val)) {
        if (!str_val.empty()) {
            node->set_value(str_val);
        }
        UrlParams params = default_params();
        if (node_handler(node, params)) {
            out << "<url>" << std::endl;
            out << "\t<loc>";
            out << base_loc();
            node->write_all_to(out, root());
            out << "</loc>" << std::endl;
            if (params.lastmod.isValid()) {
                out << "\t<lastmod>";
                out << params.lastmod.toString("yyyy-MM-dd").toUTF8();
                out << "</lastmod>" << std::endl;
            }
            std::string changefreq;
            if (params.changefreq == ALWAYS) {
                changefreq = "always";
            } else if (params.changefreq == HOURLY) {
                changefreq = "hourly";
            } else if (params.changefreq == DAILY) {
                changefreq = "daily";
            } else if (params.changefreq == WEEKLY) {
                changefreq = "weekly";
            } else if (params.changefreq == MONTHLY) {
                changefreq = "monthly";
            } else if (params.changefreq == YEARLY) {
                changefreq = "yearly";
            } else if (params.changefreq == NEVER) {
                changefreq = "never";
            }
            if (!changefreq.empty()) {
                out << "\t<changefreq>";
                out << changefreq;
                out << "</changefreq>" << std::endl;
            }
            if (params.priority != 0.5) {
                out << "\t<priority>";
                out << params.priority;
                out << "</priority>" << std::endl;
            }
            out << "</url>" << std::endl;
        }
    }
}

}

}

}
