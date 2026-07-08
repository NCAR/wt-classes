/*
 * wt-classes, utility classes used by Wt applications
 * Copyright (C) 2011 Boris Nagaev
 *
 * See the LICENSE file for terms of use.
 */

#ifndef WC_URL_HPP_
#define WC_URL_HPP_

#include "config.hpp"

#include <map>
#include <string>
#include <ostream>
#include <functional>
#include <any>
#include <vector>
#include <memory>

#include <Wt/WGlobal.h>
#include <Wt/WObject.h>
#include <Wt/WSignal.h>
#include <Wt/WDate.h>
#ifdef WC_HAVE_WLINK
#include <Wt/WLink.h>
#endif

#include "global.hpp"

namespace Wt {

namespace Wc {

namespace url {

class Parser;
class SiteMapGenerator;

class Node : public WObject {
public:
    enum SlashStrategy {
        IF_NOT_LAST,
        IF_HAS_CHILD,
        ALWAYS,
        DEFAULT = IF_HAS_CHILD
    };

    Node();
    ~Node() override;

    virtual bool meet(const std::string& part) const = 0;

    const std::string& value() const {
        return value_;
    }

    void write_to(std::ostream& path, bool is_last = false) const;
    void write_all_to(std::ostream& path, Node* root = nullptr) const;
    std::string full_path() const;

#ifdef WC_HAVE_WLINK
    WLink link() const;
#endif

    Node* node_parent() const;
    Parser* parser() const;

    void open(bool change_path = true);
    Signal<>& opened();

    SlashStrategy slash_strategy() const {
        return slash_strategy_;
    }

    void set_slash_strategy(SlashStrategy slash_strategy) {
        slash_strategy_ = slash_strategy;
    }

    /** Add child node (shadows WObject::addChild to track children and parents internally in Wt 4) */
    template <typename Child>
    Child* addChild(std::unique_ptr<Child> child) {
        Child* ptr = child.get();
        if (Node* node = dynamic_cast<Node*>(ptr)) {
            children_.push_back(node);
            node->parent_node_ = this;
        }
        Wt::WObject::addChild(std::move(child));
        return ptr;
    }

    const std::vector<Node*>& children() const {
        return children_;
    }

protected:
    void set_value(const std::string& v, bool check = false);

private:
    Signal<>* opened_;
    std::string value_;
    SlashStrategy slash_strategy_ : 8;
    std::vector<Node*> children_;
    Node* parent_node_ = nullptr;

    friend class Parser;
    friend class SiteMapGenerator;
};

class PredefinedNode : public Node {
public:
    PredefinedNode(const std::string& predefined);
    bool meet(const std::string& part) const override;
    const std::string& predefined() const {
        return predefined_;
    }

private:
    const std::string predefined_;
};

class IntegerNode : public Node {
public:
    IntegerNode();
    bool meet(const std::string& part) const override;
    long long integer() const;
    void set_integer_value(long long v);
    std::string get_full_path(long long v);

#ifdef WC_HAVE_WLINK
    WLink get_link(long long v);
#endif
};

class StringNode : public Node {
public:
    StringNode();
    bool meet(const std::string& part) const override;
    void set_string(const std::string& string);
    const std::string& string() const;
    std::string get_full_path(const std::string& v);

#ifdef WC_HAVE_WLINK
    WLink get_link(const std::string& v);
#endif
};

class Parser : public Node {
public:
    Parser();
    bool meet(const std::string& part) const override;
    Node* parse(const std::string& path);

#ifndef DOXYGEN_ONLY
    void open(Node* node);
#endif

    void open(const std::string& path);

#ifdef WC_HAVE_WLINK
    void open(const WLink& internal_path);
#endif

    Signal<>& error404() {
        return error404_;
    }

    Signal<Node*>& child_opened() {
        return child_opened_;
    }

    void connect(Node* child, std::function<void()> handler);
    void disconnect(Node* child);

private:
    friend class Node;
    typedef std::function<void()> Handler;
    typedef std::multimap<Node*, Handler> Handlers;

    Signal<> error404_;
    Signal<Node*> child_opened_;
    Handlers handlers_;
};

class SiteMapGenerator {
public:
    enum ChangeFreq {
        ALWAYS, HOURLY, DAILY, WEEKLY, MONTHLY, YEARLY, NEVER
    };

    struct UrlParams {
        WDate lastmod;
        ChangeFreq changefreq;
        float priority;
    };

    SiteMapGenerator(Node* root);
    void generate(std::ostream& out) const;

    Node* root() const {
        return root_;
    }

    void set_root(Node* root) {
        root_ = root;
    }

    const std::string& base_loc() const {
        return base_loc_;
    }

    void set_base_loc(const std::string& base_loc) {
        base_loc_ = base_loc;
    }

    const UrlParams& default_params() const {
        return default_params_;
    }

    void set_default_params(const UrlParams& default_params) {
        default_params_ = default_params;
    }

protected:
    typedef std::function<void(std::any)> AnyCaller;
    virtual void for_each_value(Node* node, const AnyCaller& callback) const;
    virtual bool node_handler(Node* node, UrlParams& params) const;

private:
    Node* root_;
    std::string base_loc_;
    UrlParams default_params_;

    void dig_node(Node* node, std::ostream& out) const;
    void visit_node(Node* node, std::ostream& out, std::any value) const;
};

}

}

}

#endif
